* # NGine: Deep-Dive Analysis


This document covers **design choices and architecture**, **implementation details and maths** for the SPH fluid simulation and Gaussian splatting renderer, and a **Q&A discussion**.

---

## Part 1: Architecture and Design Choices

### 1.1 High-Level Structure

The engine is structured as:

- **Application** – Owns the GLFW window, config, and scene; runs the main loop (`init` → `run`); owns the SPH simulator, Phong shader, particle renderer, Gaussian renderer, and the render pipeline. Single entry point from `main()`.
- **Config / AppArgs** – Centralized window size, FOV, near/far, and all asset paths (OBJ, textures, shaders). Paths are derived from a single `basePath` via `resolvePaths()`. CLI is minimal (`--ply <path>`).
- **Scene** – Holds camera, mesh + texture, Gaussian list, model transform, lighting (direction, color, ambient/specular), background color, and UI flags. Single “world” state used by the pipeline.
- **Camera** – Position, yaw (radians), up vector, FOV. Produces view matrix via look-at + invert and projection via a custom perspective helper.
- **RenderPipeline** – Given scene and simulator, performs a fixed draw order: clear → Phong mesh → (optional) Gaussian splats → particles → ImGui. It also runs the ImGui control panels and syncs them with the scene.

**Chosen approach:** One application, one scene, one pipeline, explicit ownership (Application owns subsystems, Scene owns mesh/Gaussians/camera). No ECS, no generic “entity” list.

**Alternatives that could have been chosen:**

- **ECS (Entity Component System):** Entities for mesh, point cloud, fluid, camera; components for transform, mesh, splat data, etc. More flexible for many object types and systems, but heavier for a small, fixed feature set.
- **Global state:** No Application/Scene; globals for camera, mesh, and renderers (as in the original pre-refactor code). Simpler to wire up but harder to test, reuse, or run multiple views.
- **Renderer abstraction:** Interface `IRenderer` with `render(Scene&)`, with Phong, Gaussian, and particle as implementations. Would make adding new pass types cleaner at the cost of extra indirection and boilerplate for a small number of passes.
- **Config file:** Load window size, paths, and default PLY from JSON/INI instead of only CLI and code defaults. Better for non-developers; the current design keeps everything in code and one CLI flag.

### 1.2 Rendering Pipeline Order

Fixed sequence each frame:

1. Clear color and depth.
2. **Phong pass** – Opaque mesh with depth test and depth write.
3. **Gaussian pass** (if any) – Blend enabled, depth test **disabled**, depth write off; draw order is **back-to-front** from CPU sort; premultiplied alpha.
4. **Particle pass** – Blend, depth test (LEQUAL), no depth write; points from SSBO.
5. **ImGui** – On top.

**Choice:** Gaussians are drawn without depth test so that the mesh’s depth buffer does not cull them; correctness of transparency is delegated entirely to **sorting** (back-to-front). Particles are drawn after Gaussians and use depth so they can occlude or blend with the scene.

**Alternatives:**

- **Depth test on for Gaussians:** Would cull splats behind the mesh; could be desired for “inside room” scenes but would hide splats that should be visible behind geometry when the camera is outside.
- **Depth write on for Gaussians:** Would prevent later splats from blending correctly where they overlap; the current “no depth write” preserves correct alpha blending at the cost of not writing depth for splats.
- **GPU sort for Gaussians:** Would allow depth test and possibly depth write with a sorted buffer; the project chose CPU sort to avoid GPU sort implementation and buffer management.

### 1.3 Ownership and Lifetimes

- **Application** creates and owns: `SPHSimulator*`, `shader*` (Phong), `ParticleRenderer*`, `GaussianRenderer*`, `RenderPipeline*`, and holds `Scene` by value. On exit it deletes the pointers and calls `scene_.destroy()` (mesh + texture).
- **RenderPipeline** receives raw pointers to the Phong shader and the two renderers; it does not own them.
- **Scene** owns `MeshGPU` and the `GLuint` texture; `destroy()` releases them. It holds `std::vector<Gaussian>`; the Gaussian renderer receives this by const reference and builds a sorted GPU buffer per frame.

**Choice:** Central ownership in Application, no shared ownership (no `shared_ptr`). Scene is a single coherent state object.

**Alternatives:** RenderPipeline could own the renderers and shaders; Scene could hold optional/pointer to simulator; or a separate “Simulation” object could own SPH and expose only the particle buffer.

### 1.4 Math Library (miniVM)

- **Chosen:** Custom small math library: `vec2f`, `vec3f`, `vec4f`, `vec3i`, `mat4x4` (row-major in C: `m[row][col]`), and free functions for add, sub, dot, cross, length, normalize, and matrix helpers (identity, translation, rotation X/Y/Z, projection, point-at, quick invert, multiply). No external math lib (e.g. GLM).
- **Rationale:** No dependency, full control, and a single convention inside the C++ code.
- **OpenGL interaction:** Shaders expect column-major matrices. The Gaussian renderer (and any code that uploads matrices to GL) **transposes** when building the float array: columns of the logical matrix are written as consecutive floats so that `glUniformMatrix4fv(..., GL_FALSE, data)` matches the GLSL layout. The Phong path may still pass row-major data; consistency with the shader’s expectation should be verified per pass.

**Alternatives:** Use GLM or Eigen; then all math and uploads would follow one library’s convention. Would simplify matrix handling and reduce bugs at the cost of a dependency.

---

## Part 2: SPH Fluid Simulation

### 2.1 Physics Model

SPH (Smoothed Particle Hydrodynamics) approximates a fluid with **particles**. Each particle has position, velocity, and carries mass; density and pressure are computed from neighbors within a **smoothing radius** \(h\) using **kernel functions**.

**Density:**

\[
\rho_i = \sum_j m_j \, W(r_{ij}, h), \quad r_{ij} = |\mathbf{x}_i - \mathbf{x}_j|
\]

**Pressure (equation of state):**

\[
p_i = k\, (\rho_i - \rho_0)
\]

with \(\rho_0\) rest density and \(k\) a stiffness constant (e.g. `PRESSURE_CONSTANT`).

**Forces:**

- **Pressure force** (symmetric, from pressure gradient): uses the **spiky kernel gradient** so the force points away from neighbors when \(p_i + p_j\) is positive.
- **Viscosity** (velocity Laplacian): uses a **viscosity kernel** and \((v_j - v_i)\) to smooth velocity and damp oscillations.

**Integration:** Explicit Euler: advance velocity with (forces/ρ + gravity)*dt, then advance position with velocity*dt. **Boundary:** Axis-aligned box; on crossing a face, position is clamped and the normal component of velocity is reflected and scaled by `BOUNCE_DAMPING`.

### 2.2 Kernel Choices and Maths

**Poly6 (density):**

\[
W_{\text{poly6}}(r) = \alpha_{\text{poly6}} \,(h^2 - r^2)^3 \quad \text{for } r < h
\]

- Used only for **density** (scalar, smooth). Constant \(\alpha_{\text{poly6}} = \frac{315}{64\pi h^9}\) so the kernel integrates to 1.
- In code: `poly6Kernel(r2)` uses `params.poly6Constant * (h2 - r2)^3` with \(h^2 =\) `smoothingRadius2`.

**Spiky gradient (pressure force):**

\[
\nabla W_{\text{spiky}}(r) = \alpha_{\text{spiky}} \,(h - r)^2 \,\hat{\mathbf{r}}, \quad \hat{\mathbf{r}} = \frac{\mathbf{r}}{r}
\]

- Gives a **gradient** that goes to zero at \(r = h\), avoiding singularities when particles get close. Constant \(\alpha_{\text{spiky}} = -\frac{45}{\pi h^6}\) (minus so that the gradient points from \(j\) to \(i\) in the right direction for pressure).
- In code: `spikyGradient(r, rNorm)` returns `params.spikyConstant * (h - r)^2 * rNorm`.

**Viscosity kernel:**

\[
W_{\text{visc}}(r) = -\frac{r^3}{2h^3} + \frac{r^2}{h^2} + \frac{h}{2r} - 1
\]

- Used in the viscosity term \(\propto (v_j - v_i)\, W_{\text{visc}}(r)\). In code: `viscosityKernel(r)` implements this formula for \(0 < r < h\).

### 2.3 Spatial Hash and Neighbor Search

**Goal:** For each particle \(i\), only sum over particles \(j\) with \(|\mathbf{x}_i - \mathbf{x}_j| < h\) (or within a few cells of the same size).

**Chosen approach:**

1. **Hash grid:** Space is divided into cells of size `cellSize` (= \(h\)). Cell index is \(\lfloor (\mathbf{x} - \text{boxMin}) / h \rfloor\). A **spatial hash** maps 3D cell indices to a 1D bucket index: `hash = (cell.x*p1 ^ cell.y*p2 ^ cell.z*p3) % tableSize` with large primes, so particles in the same cell get the same hash.
2. **COMPUTE_HASH:** For each particle, write its hash and its particle index into `particleHashes[]` and `particleIndices[]` (initially identity).
3. **CPU sort:** Sort the pairs (hash, index) by hash on the CPU. So in the sorted array, all particles in one cell are contiguous; `particleIndices[]` then gives the **original** particle index for each sorted slot.
4. **BUILD_CELL_INDEX:** In one pass over the sorted array, for each distinct hash write the first and one-past-last index into `cellStart[hash]` and `cellEnd[hash]`. So for any cell hash we get a range into the **sorted** array.
5. **COMPUTE_DENSITY / COMPUTE_FORCES:** For particle \(i\), get its cell and the 27 neighboring cells (3×3×3). For each neighbor cell hash, iterate from `cellStart[hash]` to `cellEnd[hash]`; for each sorted index `idx`, set `j = particleIndices[idx]` and use `particles[j].position` (and other fields). So the **particle buffer is never reordered**; only hash and index buffers are sorted. The index buffer is the indirection “sorted slot → original particle.”

**Why CPU sort:** Keeps the GPU pipeline simple (no GPU sort or radix sort), and for moderate particle counts the cost is acceptable. The main cost is the GPU–CPU readback and CPU sort each frame.

**Alternatives:**

- **GPU sort (e.g. radix sort by hash):** Would avoid readback and allow much larger particle counts; requires a GPU sort implementation and possibly reordering the particle buffer so that spatially close particles are contiguous in memory (better cache use in density/force).
- **Uniform grid:** If the domain is a regular grid, cell index could be a 3D index and cells could be stored in a 3D texture or buffer; no hash collisions but needs a fixed grid and more memory for sparse regions.
- **Brute force:** Loop over all pairs. \(O(N^2)\); only acceptable for very small \(N\).

### 2.4 Pipeline Stages and Buffers

- **SSBOs:** `ParticleBuffer` (positions, colors, velocities, life), `SPHDataBuffer` (density, pressure, force per particle), `HashBuffer`, `IndexBuffer`, `CellStartBuffer`, `CellEndBuffer`.
- **UBO:** `SimParams` (dt, gravity, \(h\), \(h^2\), mass, \(\rho_0\), pressure/viscosity constants, box, grid dim, cell size, table size, kernel constants).
- **Stages:** COMPUTE_HASH → (CPU sort) → BUILD_CELL_INDEX → COMPUTE_DENSITY → COMPUTE_FORCES → INTEGRATE, with `GL_SHADER_STORAGE_BARRIER_BIT` between stages.

**Design choice:** Multiple small compute programs compiled from the same GLSL file with different `#define` (e.g. `COMPUTE_HASH`, `BUILD_CELL_INDEX`, …). Keeps shared structs and bindings in one place.

### 2.5 Force Formulation (Code-Level)

In COMPUTE_FORCES, for each pair \((i,j)\) within range:

- **Pressure term:**  
  `(pressureA + pressureB) / (2 * densityB)` times mass and **minus** the spiky gradient (so the force on \(i\) is along \(\mathbf{r}_{ij} = \mathbf{x}_i - \mathbf{x}_j\), i.e. repulsive when pressure is positive). The gradient is in the direction of \(\hat{\mathbf{r}}\) from \(j\) to \(i\); the code uses `diff = posA - posB`, so `rNorm = diff/r` points from \(j\) to \(i\), and the spiky gradient is along that, giving a repulsive force on \(i\) from \(j\) when pressure is positive.
- **Viscosity:**  
  `params.mass * (velB - velA) / densityB * viscosityKernel(r)` plus `viscosityConstant` factor. Smooths velocity differences.

Density is clamped from below (e.g. `restDensity * 0.01`) to avoid division by zero in later stages.

---

## Part 3: Gaussian Splatting Renderer

### 3.1 Representation of a Gaussian

Each splat is represented by:

- **Position** \(\mathbf{p}\) (world space).
- **Rotation** as a unit quaternion (PLY: `rot_0..rot_3` = w,x,y,z); used to build a 3×3 rotation matrix \(\mathbf{R}\).
- **Scale** in **log space** \(\mathbf{s}_{\log}\); linear scale \(\mathbf{s} = \exp(\mathbf{s}_{\log})\) (so it stays positive).
- **Opacity** in **logit space** \(\alpha_{\log}\); used as \(\alpha = \sigma(\alpha_{\log})\) in the shader (sigmoid).
- **Spherical harmonics (SH)** for color: degree 0 (1 coeff per channel) + degree 1 (3 coeffs per channel) → 4 coeffs × 3 channels = 12 floats. Stored as `sh[0..3]` R, `sh[4..7]` G, `sh[8..11]` B.

So each Gaussian is an anisotropic 3D Gaussian in world space, with view-dependent color via SH.

### 3.2 3D Covariance and Projection to 2D

**3D covariance (in world space):**

\[
\mathbf{\Sigma}_{3D} = \mathbf{R}\,\mathbf{S}^2\,\mathbf{R}^T, \quad \mathbf{S} = \operatorname{diag}(s_x, s_y, s_z)
\]

So \(\mathbf{\Sigma}_{3D}\) is symmetric positive definite. In the vertex shader, \(\mathbf{R}\) is from the quaternion; \(\mathbf{S}^2\) is applied as scaling the rows of \(\mathbf{R}\) by \(s_x,s_y,s_z\), then forming \(\mathbf{R}\mathbf{S}\,(\mathbf{R}\mathbf{S})^T\).

**Camera-space covariance:**

\[
\mathbf{\Sigma}_{\text{cam}} = \mathbf{W}\,\mathbf{\Sigma}_{3D}\,\mathbf{W}^T
\]

where \(\mathbf{W}\) is the upper-left 3×3 of the view matrix (rotation part). So the splat is transformed to camera space.

**Perspective projection to 2D (EWA):** The vertex shader uses a **Jacobian** \(\mathbf{J}\) of the perspective projection at the splat center in camera space \((t_x, t_y, t_z)\):

\[
\mathbf{J} = \begin{bmatrix} f_x/t_z & 0 & -f_x t_x/t_z^2 \\ 0 & f_y/t_z & -f_y t_y/t_z^2 \\ 0 & 0 & 0 \end{bmatrix}
\]

with \(f_x, f_y\) from `u_focal`. The 2D covariance (upper-left 2×2) is:

\[
\mathbf{\Sigma}_{2D} = \mathbf{J}\,\mathbf{\Sigma}_{\text{cam}}\,\mathbf{J}^T
\]

The shader packs the symmetric 2×2 as \((a, b, c)\) with \(\mathbf{\Sigma}_{2D} = \begin{bmatrix} a & b \\ b & c \end{bmatrix}\). A small regularizer (e.g. 0.3) is added to \(a\) and \(c\) to avoid degenerate (needle-thin) ellipses.

### 3.3 Quad Placement and Radius

The splat is drawn as a **screen-aligned quad** centered at the projected center. The **radius** (half-size) in pixels is derived from the **largest eigenvalue** of \(\mathbf{\Sigma}_{2D}\):

\[
\lambda_{\max} = \frac{a+c}{2} + \sqrt{\max(0, (a+c)^2/4 - (ac-b^2))}
\]

Then `radius = ceil(3 * sqrt(lambda_max))` so the quad covers approximately a 3‑sigma extent. The four corners are at \(\pm\) radius in pixel space, then converted back to NDC for `gl_Position`. So each splat is one quad (4 vertices) in a single draw call via **instancing**; `gl_VertexID % 4` picks the corner.

### 3.4 Fragment Shader: 2D Gaussian Weight

In the fragment shader, the vertex shader passes:

- **v_offset:** Pixel-space offset from the splat center to the current fragment (Δ).
- **v_cov2d_inv:** The inverse of \(\mathbf{\Sigma}_{2D}\) packed as \((a', b', c')\) (so \(\mathbf{\Sigma}_{2D}^{-1} = \begin{bmatrix} a' & b' \\ b' & c' \end{bmatrix}\)).
- **v_color_alpha:** RGB from SH and \(\alpha = \sigma(\text{opacity})\).

The 2D Gaussian weight is:

\[
w = \exp\bigl(-0.5\,\mathbf{\Delta}^T \mathbf{\Sigma}_{2D}^{-1} \mathbf{\Delta}\bigr) = \exp\bigl(-0.5\,(a'\,\Delta x^2 + 2b'\,\Delta x\,\Delta y + c'\,\Delta y^2)\bigr)
\]

Final alpha is \(w \cdot \alpha\). Color is premultiplied: `frag_color = (rgb * alpha, alpha)` for correct blending with `GL_ONE, GL_ONE_MINUS_SRC_ALPHA`. Fragments with very small alpha are discarded to avoid noise and unnecessary blending.

### 3.5 Spherical Harmonics (Degree 0 + 1)

SH represent view-dependent color. With direction \(\mathbf{d} = (x,y,z)\):

- **Degree 0:** \(Y_0^0 = C_0\) constant → base color per channel (e.g. `C0 * sh_r.x` for red).
- **Degree 1:** \(Y_1^{-1} \propto y\), \(Y_1^0 \propto z\), \(Y_1^1 \propto x\) with constant \(C_1\). The shader uses the standard coefficients (e.g. 0.28209479177, 0.48860251190) and adds 0.5 to shift the DC term. Result is clamped to \([0,1]\).

So the project uses **only** degrees 0 and 1 (4 coefficients per channel, 12 per splat) to keep storage and bandwidth low; higher degrees would improve reflections at the cost of more data and possibly different PLY layouts.

### 3.6 Ordering and Blending

- **Sort:** On the CPU, `compute_sorted_indices` computes camera-space depth for each Gaussian (e.g. \(-(\text{view row}_2 \cdot \mathbf{p})\)) and returns indices sorted **back-to-front** (largest depth first).
- **Draw:** The Gaussian renderer builds a GPU buffer of splats in that order and draws one instanced quad per splat. So the **draw order** is back-to-front.
- **Blending:** With depth test off and depth write off, later (closer) splats are blended over earlier (farther) ones, giving correct over-compositing for alpha.

**Alternatives:**

- **Tile-based rasterization (e.g. 3DGS-style):** Sort splats per tile and render in tiles; more complex but can reduce overdraw and allow depth.
- **GPU sort:** Sort by depth on the GPU so the CPU doesn’t need to read back; would scale better for very large splat counts.
- **Depth buffer for splats:** Write depth when drawing splats and use depth test; would require a consistent ordering (e.g. front-to-back with a different blend or a single pass that writes depth and then resolves color).

### 3.7 PLY Loading

The loader expects **binary_little_endian** PLY with per-vertex properties: `x,y,z`, `opacity`, `scale_0,1,2`, `rot_0..rot_3`, `f_dc_0,1,2`, and optionally `f_rest_0..8` for degree-1 SH. It fills the `Gaussian` struct (and thus the GPU layout) with position, quaternion, log-scale, logit opacity, and 12 SH coefficients; higher-degree SH in the PLY are ignored.

---

## Part 4: Q&A Discussion

**Q: Why is the SPH sort done on the CPU instead of the GPU?**  
**A:** The design prioritizes simplicity: a single readback of hash and index, `std::sort` by hash, then upload. GPU radix sort would avoid readback and scale better for large \(N\), but would require implementing and maintaining a GPU sort and possibly reordering the particle buffer for better cache behavior. For moderate particle counts (e.g. hundreds to low thousands), the CPU sort cost is often acceptable.

**Q: Why use a hash grid instead of a uniform 3D grid?**  
**A:** A hash grid uses a fixed 1D table (e.g. 16384 entries) regardless of domain size; cells are mapped by a hash function, so memory use is bounded and independent of world size. A uniform grid would need a 3D array of size proportional to (box extent / cell size)^3, which can be huge for large or uneven domains. Hash collisions (multiple cells mapping to the same bucket) are handled by the same start/end range; the iteration over that range still only considers particles that were hashed to that bucket, and spatial coherence is maintained by the 3×3×3 neighbor search.

**Q: Why are Gaussians drawn with depth test disabled?**  
**A:** So that every splat contributes to the image where it projects, and blending order is controlled purely by the CPU sort (back-to-front). If depth test were on, the mesh (and earlier geometry) would occlude splats behind it; for a “point cloud over the scene” look, that would hide many splats. Disabling depth test and depth write keeps the implementation simple and ensures correct alpha blending by draw order.

**Q: What is the “quick invert” used for the view matrix?**  
**A:** The view matrix is built as a **look-at** matrix (camera position, target, up). That matrix transforms from **world to camera** in a specific layout (right, up, forward, position columns). Its inverse is the “camera to world” matrix. For the **view** matrix we need world → camera, which is the inverse of that camera-to-world matrix. The “quick invert” exploits the fact that for a rigid body (rotation + translation), the inverse is the transpose of the 3×3 block plus a translation derived from that. So we get the correct view matrix without a general 4×4 invert.

**Q: Why store scale and opacity in log and logit space?**  
**A:** So that optimization (during training of the 3DGS model) can update them without constraints: \(\exp\) and \(\sigma\) (sigmoid) map \(\mathbb{R}\) to \((0,+\infty)\) and \((0,1)\), so the parameters can be updated freely and the rendered values stay valid. The runtime (and this renderer) just apply \(\exp\) and \(\sigma\) in the shader.

**Q: Why only SH degree 0 and 1?**  
**A:** Degree 0 gives view-independent color; degree 1 adds basic view-dependent variation (e.g. sheen). Higher degrees improve quality but increase storage (e.g. 48+ floats per splat for degree 2) and bandwidth. The project chose a minimal set (12 floats per splat) to keep the buffer small and the shader simple while still getting view-dependent color.

**Q: How does BUILD_CELL_INDEX work if multiple cells hash to the same bucket?**  
**A:** After the sort, the array is ordered by **hash value**, not by cell. So all particles with hash \(H\) are contiguous; then for each **unique** hash value we see in order, we set `cellStart[H]` and `cellEnd[H]`. If two different 3D cells hash to the same \(H\), they are merged into one range: we’d iterate over particles from both cells when we query that hash. That’s a **hash collision**; it only adds extra work (checking a few more particles and rejecting them by distance), not incorrect results. Using a large table and good primes keeps collisions relatively rare.

**Q: Why is the particle buffer never reordered after the sort?**  
**A:** The sort orders **(hash, original_index)**. The GPU only needs to know “for this cell hash, which particle indices fall in this range.” So we only need the **index** buffer sorted by hash; when we iterate neighbors we use `particleIndices[idx]` to read from the **original** particle buffer. Reordering the particle buffer would require either a full GPU reorder (more complex) or writing back sorted positions/velocities from the CPU (extra copy). Keeping particles in place and using an indirection buffer is the minimal change that enables fast neighbor search.

**Q: What would break if view/projection were passed row-major to the Gaussian shader?**  
**A:** GLSL expects column-major. If we passed row-major data and told GL “don’t transpose,” the shader would effectively use the **transpose** of the intended matrices. Then `u_view * position` would be wrong (wrong rotation and translation), so splats would project to wrong places, the 2D covariance would be wrong (wrong W and Jacobian), and the view direction for SH would be wrong. So we’d get wrong positions, wrong shapes, and wrong colors. Uploading in column-major (transposing from the C++ row-major storage when building the float array) fixes this.

**Q: Could the Phong and Gaussian passes share the same matrix layout?**  
**A:** Yes. If the Phong shader is written to use the same column-major convention as GLSL’s default, then both passes should upload matrices in the same way (e.g. column-major float array from the same row-major `mat4x4`). Right now the Phong path may still be passing row-major; making both paths use the same “row-major in C, column-major to GL” convention would avoid subtle bugs and make the codebase consistent.

**Q: What is the role of the regularizer (e.g. +0.3) on the 2D covariance?**  
**A:** When the splat is viewed almost edge-on, one eigenvalue of \(\mathbf{\Sigma}_{2D}\) can become very small, so the inverse blows up and the Gaussian becomes extremely narrow (needle-like). That can cause numerical issues and visual artifacts. Adding a small constant to the diagonal (e.g. 0.3 to \(a\) and \(c\)) keeps the minimum eigenvalue bounded, so the inverse stays stable and the splat remains at least slightly spread in both directions.

---

*End of deep-dive analysis. For implementation details, refer to the source files and shaders listed in the sections above.*
