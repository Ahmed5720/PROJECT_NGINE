
/*
loads a ply file containing a model of a gaussian splat as a set of points with the following parameters
*/
#pragma once
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <cstring>
#include <cmath>
#include <geometry.h>
using namespace std;


// Each property in the PLY header is recorded as a name + byte offset within
// the per-vertex binary record.
struct PropertyInfo {
    std::string name;
    int byte_offset; // offset within the flat per-vertex byte block
};

// Read a little-endian float from a raw byte pointer at a given byte offset.
static inline float read_f32(const uint8_t* record, int offset) {
    float v;
    std::memcpy(&v, record + offset, sizeof(float));
    return v;
}


std::vector<Gaussian> load_ply(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[PLY] Cannot open file: " << path << "\n";
        return {};
    }


    // 1. Parse the ASCII header
    int vertex_count = 0;
    std::vector<PropertyInfo> properties; // ordered list of all properties
    bool in_vertex_element = false;
    bool is_binary_little_endian = false;

    std::string line;
    while (std::getline(file, line)) {
        // Trim carriage return (Windows line endings)
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line == "end_header") break;

        std::istringstream ss(line);
        std::string token;
        ss >> token;

        if (token == "format") {
            std::string fmt;
            ss >> fmt;
            is_binary_little_endian = (fmt == "binary_little_endian");
            if (!is_binary_little_endian) {
                std::cerr << "[PLY] Only binary_little_endian is supported (got: " << fmt << ")\n";
                return {};
            }
        } else if (token == "element") {
            std::string elem_name;
            ss >> elem_name;
            in_vertex_element = (elem_name == "vertex");
            if (in_vertex_element) ss >> vertex_count;
        } else if (token == "property" && in_vertex_element) {
            std::string type, name;
            ss >> type >> name;
            // All properties in 3DGS PLY files are float32
            int offset = (int)properties.size() * (int)sizeof(float);
            properties.push_back({name, offset});
        }
    }

    if (vertex_count == 0) {
        std::cerr << "[PLY] No vertices found.\n";
        return {};
    }

    std::cout << "[PLY] Vertex count  : " << vertex_count << "\n";
    std::cout << "[PLY] Property count: " << properties.size() << "\n";

    // 2. Build a name → byte_offset lookup map
    std::unordered_map<std::string, int> prop_offset;
    for (const auto& p : properties) {
        prop_offset[p.name] = p.byte_offset;
    }

    // Helper: get byte offset of a property, or -1 if not present
    auto offset_of = [&](const std::string& name) -> int {
        auto it = prop_offset.find(name);
        return (it != prop_offset.end()) ? it->second : -1;
    };

    // Validate required properties exist
    const std::vector<std::string> required = {
        "x", "y", "z",
        "opacity",
        "scale_0", "scale_1", "scale_2",
        "rot_0", "rot_1", "rot_2", "rot_3",
        "f_dc_0", "f_dc_1", "f_dc_2"
    };
    for (const auto& req : required) {
        if (offset_of(req) == -1) {
            std::cerr << "[PLY] Missing required property: " << req << "\n";
            return {};
        }
    }

    // Pre-resolve offsets for all fields we care about
    const int off_x       = offset_of("x");
    const int off_y       = offset_of("y");
    const int off_z       = offset_of("z");
    const int off_opacity = offset_of("opacity");
    const int off_scale0  = offset_of("scale_0");
    const int off_scale1  = offset_of("scale_1");
    const int off_scale2  = offset_of("scale_2");
    const int off_rot0    = offset_of("rot_0"); // w
    const int off_rot1    = offset_of("rot_1"); // x
    const int off_rot2    = offset_of("rot_2"); // y
    const int off_rot3    = offset_of("rot_3"); // z

    // DC terms (degree 0): one per RGB channel
    const int off_fdc0    = offset_of("f_dc_0");
    const int off_fdc1    = offset_of("f_dc_1");
    const int off_fdc2    = offset_of("f_dc_2");

    // Degree-1 rest terms: f_rest_0..f_rest_8 (3 per channel x 3 channels)
    // The 3DGS convention interleaves them as:
    //   f_rest_0,1,2  = R channel degree-1 (sh[1..3])
    //   f_rest_3,4,5  = G channel degree-1
    //   f_rest_6,7,8  = B channel degree-1
    int off_rest[9];
    for (int i = 0; i < 9; ++i) {
        off_rest[i] = offset_of("f_rest_" + std::to_string(i));
    }

    // 3. Read the binary vertex data
    const int bytes_per_vertex = (int)properties.size() * sizeof(float);
    std::vector<uint8_t> raw(vertex_count * bytes_per_vertex);
    file.read(reinterpret_cast<char*>(raw.data()), raw.size());

    if (!file) {
        std::cerr << "[PLY] File read failed or truncated.\n";
        return {};
    }

    // 4. Unpack each vertex into a Gaussian struct
    std::vector<Gaussian> gaussians(vertex_count);
    for (int i = 0; i < vertex_count; ++i) {
        const uint8_t* rec = raw.data() + i * bytes_per_vertex;
        Gaussian& g = gaussians[i];

        g.pos = { read_f32(rec, off_x), read_f32(rec, off_y), read_f32(rec, off_z) };
        g.opacity  = read_f32(rec, off_opacity);  // logit — sigmoid applied later
        g.scale    = { read_f32(rec, off_scale0), read_f32(rec, off_scale1), read_f32(rec, off_scale2) }; // log — exp applied later
        g.rot= { read_f32(rec, off_rot0), read_f32(rec, off_rot1), read_f32(rec, off_rot2), read_f32(rec, off_rot3) }; // w,x,y,z

        // SH layout in our struct:
        //   sh[0..3]  = R channel (DC, d1_0, d1_1, d1_2)
        //   sh[4..7]  = G channel
        //   sh[8..11] = B channel

        g.sh[0]  = read_f32(rec, off_fdc0); // R DC
        g.sh[4]  = read_f32(rec, off_fdc1); // G DC
        g.sh[8]  = read_f32(rec, off_fdc2); // B DC

        // Degree-1 coefficients (may not be present if trained at degree 0)
        for (int c = 0; c < 3; ++c) {         // channel
            for (int k = 0; k < 3; ++k) {     // basis index 1,2,3
                int rest_idx = c * 3 + k;
                int sh_idx   = c * SH_COEFFS_PER_CHANNEL + 1 + k;
                g.sh[sh_idx] = (off_rest[rest_idx] != -1)
                               ? read_f32(rec, off_rest[rest_idx])
                               : 0.0f;
            }
        }
        // All higher-degree coefficients are ignored for now.
    }

    std::cout << "[PLY] Loaded " << gaussians.size() << " Gaussians.\n";
    return gaussians;
}



