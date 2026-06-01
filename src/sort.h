#pragma once
#include <vector>
#include <numeric>
#include <algorithm>
#include "miniVM.h"
#include "geometry.h"

// ---------------------------------------------------------------------------
// compute_sorted_indices
//
// For each Gaussian, transforms its world-space center into camera space and
// records its depth (the -z value, so larger = farther away).
// Returns a list of indices sorted back-to-front (largest depth first).
//
// We sort back-to-front because OpenGL's alpha blending accumulates
// contributions in submission order. Each fragment is blended as:
//   result = src_color * src_alpha + dst_color * (1 - src_alpha)
// which requires farther Gaussians to already be in the framebuffer.
//
// Parameters:
//   gaussians  - all loaded Gaussians
//   view       - camera view matrix (world → camera)
//   out_depths - optionally receive the computed depth per Gaussian
// ---------------------------------------------------------------------------
inline std::vector<int> compute_sorted_indices(
    const std::vector<Gaussian>& gaussians,
    const mat4x4& view,
    std::vector<float>* out_depths = nullptr)
{
    const int n = static_cast<int>(gaussians.size());

    // --- Compute camera-space depth for every Gaussian ---
    // We only need the z component of the view-transformed position.
    // view[0..3] are the columns of the matrix in GLM's column-major layout.
    // The z row of the view matrix is (view[0][2], view[1][2], view[2][2], view[3][2]).
    const vec4f z_row = { view.m[0][2], view.m[1][2], view.m[2][2], view.m[3][2] };

    std::vector<float> depths(n);
    for (int i = 0; i < n; ++i) {
        vec4f p = { gaussians[i].pos.x, gaussians[i].pos.y, gaussians[i].pos.z, 1.0f };
        // Camera-space z: dot product with z row.
        // We negate because in OpenGL camera space, the camera looks toward -z,
        // so a Gaussian in front has a negative z. We want "depth" to be positive
        // and larger for farther objects.
        depths[i] = -vector_dot(z_row, p);
    }

    if (out_depths) *out_depths = depths;

    // --- Build an index array and sort it by depth descending ---
    std::vector<int> indices(n);
    std::iota(indices.begin(), indices.end(), 0); // fill 0,1,2,...,n-1

    std::sort(indices.begin(), indices.end(), [&](int a, int b) {
        return depths[a] > depths[b]; // largest depth first = back to front
    });

    return indices;
}
