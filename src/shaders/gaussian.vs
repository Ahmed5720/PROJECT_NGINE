#version 430 core

 
// since the vertex shader works on each vertex seperately, but our gaussians need to be represented as a quad,
// we need to call the vertex shader 4 times per gaussian, one for each corner of the quad.

layout(location = 0) in vec4 posOpacity; // x,y,z, opacity
layout(location = 1) in vec4 rotation;
layout(location = 2) in vec4 scale;
layout(location = 3) in vec4 sh_0; // sh coeff 0-3 (red)
layout(location = 4) in vec4 sh_1; // sh coeff 4-7 (green)
layout(location = 5) in vec4 sh_2; // sh coeff 8-11 (blue)


uniform mat4 u_view;
uniform mat4 u_proj;
uniform vec2 u_focal;
uniform vec2 u_viewport;

out vec2 v_offset;
out vec3 v_cov2d_inv;
out vec4 v_color_alpha;



float sigmoid(float x)
{
    return 1.0 / (1.0 + exp(-x));
}

// Build a 3x3 rotation matrix from a quaternion (w, x, y, z).
// Assumes the quaternion is already normalized (the PLY file guarantees this).
mat3 quat_to_mat3(vec4 q) {
    float w = q.x, x = q.y, y = q.z, z = q.w; // our storage: (w,x,y,z)
    return mat3(
        1.0 - 2.0*(y*y + z*z),  2.0*(x*y + w*z),         2.0*(x*z - w*y),
        2.0*(x*y - w*z),         1.0 - 2.0*(x*x + z*z),   2.0*(y*z + w*x),
        2.0*(x*z + w*y),         2.0*(y*z - w*x),         1.0 - 2.0*(x*x + y*y)
    );
}

// Compute the 3D covariance matrix Σ_3D = R * S^2 * R^T
// where S = diag(scale) and R is the rotation matrix.
mat3 compute_cov3d(vec3 scale, vec4 rotation) {
    mat3 R = quat_to_mat3(rotation);
    // S is a diagonal matrix; S^2 is diag(scale^2)
    // R * S^2 * R^T = (R * S) * (R * S)^T
    mat3 RS = mat3(
        R[0] * scale.x,
        R[1] * scale.y,
        R[2] * scale.z
    );
    return RS * transpose(RS);
}

// Project 3D covariance to 2D screen-space covariance using the EWA approximation.
// J is the Jacobian of the perspective projection at the given camera-space point.
// Returns the upper-left 2x2 of J * W * Σ_3D * W^T * J^T packed as (a, b, c).
vec3 project_cov3d(mat3 cov3d, vec3 pos_cam, vec2 focal) {
    // W = upper-left 3x3 of the view matrix (rotation only, no translation)
    // We extract it from the uniform.
    mat3 W = mat3(u_view);

    // Bring 3D covariance to camera space
    mat3 cov_cam = W * cov3d * transpose(W);

    // Perspective projection Jacobian at (tx, ty, tz)
    float tx = pos_cam.x, ty = pos_cam.y, tz = pos_cam.z;
    mat3 J = mat3(
        focal.x / tz,    0.0,             -(focal.x * tx) / (tz * tz),
        0.0,             focal.y / tz,    -(focal.y * ty) / (tz * tz),
        0.0,             0.0,             0.0
    );

    // Project: Σ_2D = J * Σ_cam * J^T  (only upper-left 2x2 matters)
    mat3 T = J * cov_cam;
    // Upper-left 2x2 of T * J^T:
    float a = T[0][0]*J[0][0] + T[1][0]*J[1][0];  // Σ_2D[0][0]
    float b = T[0][0]*J[0][1] + T[1][0]*J[1][1];  // Σ_2D[0][1] = Σ_2D[1][0]
    float c = T[0][1]*J[0][1] + T[1][1]*J[1][1];  // Σ_2D[1][1]

    // Add a small regularizer to avoid degenerate (needle-thin) Gaussians
    a += 0.3;
    c += 0.3;

    return vec3(a, b, c);
}

// Invert a 2x2 symmetric matrix [[a,b],[b,c]].
// Returns packed (a', b', c') of the inverse.
vec3 invert_cov2d(float a, float b, float c) {
    float det = a * c - b * b;
    float inv_det = 1.0 / det;
    return vec3(c * inv_det, -b * inv_det, a * inv_det);
}

// Evaluate degree 0 + degree 1 spherical harmonics for a given view direction.
// SH layout per channel: [DC, d1_0, d1_1, d1_2]
// in_sh_0 = R channel, in_sh_1 = G channel, in_sh_2 = B channel
vec3 eval_sh(vec3 view_dir) {
    // SH basis constants:
    //   Y_0^0 = 0.28209479177 (degree 0)
    //   Y_1^-1 = 0.48860251190 * y
    //   Y_1^0  = 0.48860251190 * z
    //   Y_1^1  = 0.48860251190 * x
    const float C0 = 0.28209479177;
    const float C1 = 0.48860251190;

    float x = view_dir.x, y = view_dir.y, z = view_dir.z;

    // Degree-0 contribution (view-independent base color)
    vec3 color = vec3(
        C0 * in_sh_0.x,
        C0 * in_sh_1.x,
        C0 * in_sh_2.x
    );

    // Degree-1 contribution (view-dependent)
    color += vec3(
        C1 * (-y * in_sh_0.y + z * in_sh_0.z + -x * in_sh_0.w),
        C1 * (-y * in_sh_1.y + z * in_sh_1.z + -x * in_sh_1.w),
        C1 * (-y * in_sh_2.y + z * in_sh_2.z + -x * in_sh_2.w)
    );

    // The SH convention adds 0.5 to bring the DC term to a neutral gray
    color += 0.5;

    return clamp(color, 0.0, 1.0);
}

// ---------------------------------------------------------------------------
// Main vertex shader
// ---------------------------------------------------------------------------
void main() {
    vec3 world_pos = in_position_opacity.xyz;
    float raw_opacity = in_position_opacity.w;

    // --- Transform center to camera space ---
    vec4 cam_pos4 = u_view * vec4(world_pos, 1.0);
    vec3 cam_pos  = cam_pos4.xyz;

    // Cull Gaussians behind the camera. We set the vertex to a degenerate
    // position so the GPU discards the quad without extra draw calls.
    if (cam_pos.z >= 0.0) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0); // outside clip space
        return;
    }

    // --- Compute 3D covariance and project to 2D ---
    vec3 real_scale = exp(in_scale.xyz);  // log → linear
    mat3 cov3d      = compute_cov3d(real_scale, in_rotation);
    vec3 cov2d      = project_cov3d(cov3d, cam_pos, u_focal);

    // --- Find quad extent from eigenvalues of Σ_2D ---
    // For a 2x2 symmetric matrix [[a,b],[b,c]], eigenvalues are:
    //   λ = mid ± sqrt(mid² - det)  where mid = (a+c)/2
    float a = cov2d.x, b = cov2d.y, c = cov2d.z;
    float mid  = 0.5 * (a + c);
    float disc = sqrt(max(0.1, mid*mid - (a*c - b*b)));
    float lambda_max = mid + disc;

    // 3-sigma radius in pixels gives us the quad half-size
    float radius = ceil(3.0 * sqrt(lambda_max));

    // --- Project center to screen (NDC then pixel space) ---
    vec4 clip_pos = u_proj * cam_pos4;
    vec2 ndc_center = clip_pos.xy / clip_pos.w;
    // NDC to pixel: NDC [-1,1] → pixel [0, viewport]
    vec2 pixel_center = (ndc_center * 0.5 + 0.5) * u_viewport;

    // --- Position this quad corner ---
    // gl_VertexID % 4 gives corner 0..3 mapped to a unit quad:
    //   0 = bottom-left, 1 = bottom-right, 2 = top-left, 3 = top-right
    vec2 corners[4] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 1.0, -1.0),
        vec2(-1.0,  1.0),
        vec2( 1.0,  1.0)
    );
    vec2 corner = corners[gl_VertexID % 4];

    // Pixel-space offset for this corner (also passed to fragment shader as Δ)
    vec2 pixel_offset = corner * radius;
    vec2 pixel_pos    = pixel_center + pixel_offset;

    // Convert back to NDC for gl_Position
    vec2 ndc_pos = (pixel_pos / u_viewport) * 2.0 - 1.0;
    gl_Position = vec4(ndc_pos, clip_pos.z / clip_pos.w, 1.0);

    // --- Compute view direction for SH evaluation ---
    // Direction from camera origin to Gaussian center, in world space.
    vec3 cam_origin_world = -vec3(u_view[3]) * mat3(u_view); // camera world pos
    vec3 view_dir = normalize(world_pos - cam_origin_world);

    // --- Outputs ---
    v_offset    = pixel_offset;                   // Δ in pixel space
    v_cov2d_inv = invert_cov2d(a, b, c);          // packed (a',b',c') of Σ⁻¹
    v_color_alpha = vec4(eval_sh(view_dir), sigmoid(raw_opacity));
}


