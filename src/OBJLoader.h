#include <unordered_map>
#include <sstream>
#include "miniVM.h"
#include "geometry.h"

struct ObjIndex {
    int v = 0, vt = 0, vn = 0;
    bool operator==(const ObjIndex& o) const { return v==o.v && vt==o.vt && vn==o.vn; }
};

struct ObjIndexHash {
    size_t operator()(const ObjIndex& k) const {
        // simple hash combine
        size_t h1 = std::hash<int>{}(k.v);
        size_t h2 = std::hash<int>{}(k.vt);
        size_t h3 = std::hash<int>{}(k.vn);
        return h1 ^ (h2 * 16777619u) ^ (h3 * 2166136261u);
    }
};

static ObjIndex parseFaceToken(const std::string& tok) {
    // tok like "12/5/9"
    ObjIndex out;
    int slash1 = (int)tok.find('/');
    int slash2 = (int)tok.find('/', slash1 + 1);

    out.v  = std::stoi(tok.substr(0, slash1));
    out.vt = std::stoi(tok.substr(slash1 + 1, slash2 - (slash1 + 1)));
    out.vn = std::stoi(tok.substr(slash2 + 1));
    return out;
}

bool LoadOBJ_Indexed(
    const std::string& path,
    std::vector<Vertex>& outVerts,
    std::vector<uint32_t>& outIdx
) {
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::vector<vec3f> positions;
    std::vector<vec3f> normals;
    std::vector<std::pair<float,float>> uvs;

    std::unordered_map<ObjIndex, uint32_t, ObjIndexHash> lut;

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string head;
        ss >> head;

        if (head == "v") {
            vec3f p; ss >> p.x >> p.y >> p.z;
            positions.push_back(p);
        } else if (head == "vt") {
            float u, v; ss >> u >> v;
            uvs.push_back({u, v});
        } else if (head == "vn") {
            vec3f n; ss >> n.x >> n.y >> n.z;
            normals.push_back(n);
        } else if (head == "f") {
            // assume triangulated OBJ (3 verts per face)
            std::string t0, t1, t2;
            ss >> t0 >> t1 >> t2;
            std::string toks[3] = {t0, t1, t2};

            for (int i = 0; i < 3; i++) {
                ObjIndex ix = parseFaceToken(toks[i]);

                // OBJ indices are 1-based
                const vec3f& p = positions[ix.v - 1];
                const auto& uv = uvs[ix.vt - 1];
                const vec3f& n = normals[ix.vn - 1];

                auto it = lut.find(ix);
                if (it == lut.end()) {
                    Vertex vtx;
                    vtx.px = p.x; vtx.py = p.y; vtx.pz = p.z;
                    vtx.nx = n.x; vtx.ny = n.y; vtx.nz = n.z;
                    vtx.u = uv.first; vtx.v = uv.second;

                    uint32_t newIndex = (uint32_t)outVerts.size();
                    outVerts.push_back(vtx);
                    outIdx.push_back(newIndex);
                    lut[ix] = newIndex;
                } else {
                    outIdx.push_back(it->second);
                }
            }
        }
    }
    return !outVerts.empty() && !outIdx.empty();
}
