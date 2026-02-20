

# PROJECT NGINE

The long term aspiration of this project is to become a small 3D engine that supports features I consider interesting Like importing models, basic mesh editing, particle simulation, Rigid body physics, rasterization and raytracing and perhaps eventually gaussian splatting. A specific aspect I intend to focus on is performing as much computation as possible on the GPU. As one might expect from the lack of structure of this project, the intention is not to produce a functionining product but purely to satisfy my curiosity.

## Features

- A small ad-hoc matrix/vector library that performs all nessicary matrix/vector math.  
- OBJ, PLY model loading  
- rasterization pipeline: 
- phong shading, texture sampling.
- SPH fluid simulation
- Gaussian Splatting (WIP)  
![Alt text](media/teapot.png "rasterizer")


## Smoothed particle hydrodynamics fluid simulation

![Alt text](media/psimsnap.png "simulation")

In this project we created a fluid simulation using smoothed particle hydrodynamics.  
based on this paper:

For a few particles the task, is fairly trivial as it can be computed on the cpu. However as soon as soon as we require more than a few hundred particles, the CPU struggles to maintain 60FPS as the pressure Force on each particle calculation requires checking every other particle.  
Therefore, we implement a spatial hashgrid to optimize neighbor search. The goal of the spatial hashgrid, is to allow us to only search neihboring particles for each particle, by partitioning particles into cells. Each cell is given a hash value. Particles are stored in a buffer. in a seperate buffer we store the keys to each particle, and we sort them by that key such that all particles in the same cell are stored consequitively. Finally a 3rd Buffer stores the starting indices of each cell which allows us to retrieve them immediately.

```cpp
particle Buffer: [0,1,3,4,5,6,7,8,9,10,11]

spatialKeys Buffer: [0,0,0,1,1,2,2,2,2,3,3]  
                      ^      ^   ^       ^  
                     cell 0 Cell 1      Cell 3


spatialOffsets Buffer: [0,3,5,9]
                       C0,C1,C2,C3 starting indices
```
## Part 1: Architecture

### 1.1 High-Level Structure

The engine is structured as:

- **Application** – Owns the GLFW window, config, and scene; runs the main loop (`init` → `run`); owns the SPH simulator, Phong shader, particle renderer, Gaussian renderer, and the render pipeline. Single entry point from `main()`.
- **Config / AppArgs** – Centralized window size, FOV, near/far, and all asset paths (OBJ, textures, shaders). Paths are derived from a single `basePath` via `resolvePaths()`. CLI is minimal (`--ply <path>`).
- **Scene** – Holds camera, mesh + texture, Gaussian list, model transform, lighting (direction, color, ambient/specular), background color, and UI flags. Single “world” state used by the pipeline.
- **Camera** – Position, yaw (radians), up vector, FOV. Produces view matrix via look-at + invert and projection via a custom perspective helper.
- **RenderPipeline** – Given scene and simulator, performs a fixed draw order: clear → Phong mesh → (optional) Gaussian splats → particles → ImGui. It also runs the ImGui control panels and syncs them with the scene.

**approach:** One application, one scene, one pipeline, explicit ownership (Application owns subsystems, Scene owns mesh/Gaussians/camera). No ECS, no generic “entity” list.



### 1.2 Rendering Pipeline Order

Fixed sequence each frame:

1. Clear color and depth.
2. **Phong pass** – Opaque mesh with depth test and depth write.
3. **Gaussian pass** (if any) – Blend enabled, depth test **disabled**, depth write off; draw order is **back-to-front** from CPU sort; premultiplied alpha.
4. **Particle pass** – Blend, depth test (LEQUAL), no depth write; points from SSBO.
5. **ImGui** – On top.


### 1.3 Ownership and Lifetimes

- **Application** creates and owns: `SPHSimulator*`, `shader*` (Phong), `ParticleRenderer*`, `GaussianRenderer*`, `RenderPipeline*`, and holds `Scene` by value. On exit it deletes the pointers and calls `scene_.destroy()` (mesh + texture).
- **RenderPipeline** receives raw pointers to the Phong shader and the two renderers; it does not own them.
- **Scene** owns `MeshGPU` and the `GLuint` texture; `destroy()` releases them. It holds `std::vector<Gaussian>`; the Gaussian renderer receives this by const reference and builds a sorted GPU buffer per frame.

**Choice:** Central ownership in Application, no shared ownership (no `shared_ptr`). Scene is a single coherent state object.



### 1.4 Math Library (miniVM)

- **Chosen:** Custom small math library: `vec2f`, `vec3f`, `vec4f`, `vec3i`, `mat4x4` (row-major in C: `m[row][col]`), and free functions for add, sub, dot, cross, length, normalize, and matrix helpers (identity, translation, rotation X/Y/Z, projection, point-at, quick invert, multiply). No external math lib (e.g. GLM).
- **Rationale:** No dependency, full control, and a single convention inside the C++ code.
- **OpenGL interaction:** Shaders expect column-major matrices. The Gaussian renderer (and any code that uploads matrices to GL) **transposes** when building the float array: columns of the logical matrix are written as consecutive floats so that `glUniformMatrix4fv(..., GL_FALSE, data)` matches the GLSL layout. The Phong path may still pass row-major data; consistency with the shader’s expectation should be verified per pass.



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

**approach:**

1. **Hash grid:** Space is divided into cells of size `cellSize` (= \(h\)). Cell index is \(\lfloor (\mathbf{x} - \text{boxMin}) / h \rfloor\). A **spatial hash** maps 3D cell indices to a 1D bucket index: `hash = (cell.x*p1 ^ cell.y*p2 ^ cell.z*p3) % tableSize` with large primes, so particles in the same cell get the same hash.
2. **COMPUTE_HASH:** For each particle, write its hash and its particle index into `particleHashes[]` and `particleIndices[]` (initially identity).
3. **CPU sort:** Sort the pairs (hash, index) by hash on the CPU. So in the sorted array, all particles in one cell are contiguous; `particleIndices[]` then gives the **original** particle index for each sorted slot.
4. **BUILD_CELL_INDEX:** In one pass over the sorted array, for each distinct hash write the first and one-past-last index into `cellStart[hash]` and `cellEnd[hash]`. So for any cell hash we get a range into the **sorted** array.
5. **COMPUTE_DENSITY / COMPUTE_FORCES:** For particle \(i\), get its cell and the 27 neighboring cells (3×3×3). For each neighbor cell hash, iterate from `cellStart[hash]` to `cellEnd[hash]`; for each sorted index `idx`, set `j = particleIndices[idx]` and use `particles[j].position` (and other fields). So the **particle buffer is never reordered**; only hash and index buffers are sorted. The index buffer is the indirection “sorted slot → original particle.”



### 2.4 Pipeline Stages and Buffers

- **SSBOs:** `ParticleBuffer` (positions, colors, velocities, life), `SPHDataBuffer` (density, pressure, force per particle), `HashBuffer`, `IndexBuffer`, `CellStartBuffer`, `CellEndBuffer`.
- **UBO:** `SimParams` (dt, gravity, \(h\), \(h^2\), mass, \(\rho_0\), pressure/viscosity constants, box, grid dim, cell size, table size, kernel constants).
- **Stages:** COMPUTE_HASH → (CPU sort) → BUILD_CELL_INDEX → COMPUTE_DENSITY → COMPUTE_FORCES → INTEGRATE, with `GL_SHADER_STORAGE_BARRIER_BIT` between stages.

**Design choice:** Multiple small compute programs compiled from the same GLSL file with different `#define` (e.g. `COMPUTE_HASH`, `BUILD_CELL_INDEX`, …). Keeps shared structs and bindings in one place.

### 2.5 Force Formulation

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


### 3.7 PLY Loading

The loader expects **binary_little_endian** PLY with per-vertex properties: `x,y,z`, `opacity`, `scale_0,1,2`, `rot_0..rot_3`, `f_dc_0,1,2`, and optionally `f_rest_0..8` for degree-1 SH. It fills the `Gaussian` struct (and thus the GPU layout) with position, quaternion, log-scale, logit opacity, and 12 SH coefficients; higher-degree SH in the PLY are ignored.
