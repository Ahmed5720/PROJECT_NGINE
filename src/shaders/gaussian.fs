#version 430 core

// ---------------------------------------------------------------------------
// Inputs interpolated from the vertex shader.
// Because we're drawing a quad and the vertex shader outputs the same
// v_cov2d_inv and v_color_alpha from all 4 corners (they're per-Gaussian,
// not per-vertex), the interpolation is just a passthrough — every fragment
// in the quad gets the same covariance and color. Only v_offset varies
// across the quad (it's the pixel-space Δ from the Gaussian center).
// ---------------------------------------------------------------------------
in vec2  v_offset;        // pixel-space Δ from center
in vec3  v_cov2d_inv;     // packed inverse 2D covariance: (a,b,c) → [[a,b],[b,c]]
in vec4  v_color_alpha;   // rgb = SH color, a = sigmoid(opacity)

out vec4 frag_color;

void main() {
    // Unpack the inverse covariance
    float a = v_cov2d_inv.x;  // Σ⁻¹[0][0]
    float b = v_cov2d_inv.y;  // Σ⁻¹[0][1] = Σ⁻¹[1][0]
    float c = v_cov2d_inv.z;  // Σ⁻¹[1][1]

    float dx = v_offset.x;
    float dy = v_offset.y;

    // Evaluate the 2D Gaussian exponent:
    //   power = -0.5 * Δᵀ * Σ⁻¹ * Δ
    //         = -0.5 * (dx² * a + 2*dx*dy * b + dy² * c)
    float power = -0.5 * (dx*dx*a + 2.0*dx*dy*b + dy*dy*c);

    // Clamp power to 0 — positive values would give weight > 1 which is wrong
    // (can happen at the very center due to the regularizer we added to Σ_2D).
    if (power > 0.0) discard;

    float weight = exp(power);

    // Effective alpha for this fragment:
    //   alpha = sigmoid(opacity) * gaussian_weight
    float alpha = v_color_alpha.a * weight;

    // Discard nearly transparent fragments to avoid wasting blend operations
    // and to prevent floating-point accumulation artifacts.
    if (alpha < 50.0 / 255.0) discard;

    // Output premultiplied alpha so that GL_ONE, GL_ONE_MINUS_SRC_ALPHA blending
    // gives correct over-compositing:
    //   result = src_rgb * src_a + dst_rgb * (1 - src_a)
    vec3 rgb = v_color_alpha.rgb;
    frag_color = vec4(rgb * alpha, alpha);
}
