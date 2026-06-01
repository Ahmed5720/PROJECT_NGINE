#pragma once
#include <sstream>
#include <fstream>
#include <vector>
#include <string>
#include "miniVM.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
using namespace std;

// spherical harmonics parameters for gaussian splats (determines color)
// spherical harmonics are a set of othrogonal functions that we can use to model high dimensional relationships
// for our purposes, the 0th degree function models a base color
// the later degrees determined view dependent effects (reflections)
// it would be very costly to include all 4 degrees in our buffer especially since we are sending it to the gpu. and there's minimal difference visually anyway
// so we are only using degrees 0,1
// Degree 0: 1 coefficient * r,g,b
// Degree 1: 3 coefficients * r,g,b
// total = 12 floats / gaussian to represent color. not bad
static constexpr int SH_DEGREE = 1;
static constexpr int SH_COEFFS_PER_CHANNEL = 4;
static constexpr int SH_TOTAL_FLOATS = SH_COEFFS_PER_CHANNEL * 3;
#define Vector3 vec3f
#define Vector2 vec2f
struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float u,v;
    // Vector3 Position;
    // Vector3 Normal;
    // Vector2 TextureCoordinate;
};
struct triangle
{   
    Vector3 p[3]; 
    triangle()
    {
        p[0];
        p[1];
        p[2];
    }
    triangle(Vector3 a, Vector3 b, Vector3 c)
    {
        p[0] = a;
        p[1] = b;
        p[2] = c;
    }
    triangle(float x1, float y1, float z1,
             float x2, float y2, float z2,
             float x3, float y3, float z3)
    {
        p[0] = Vector3(x1,y1,z1);
        p[1] = Vector3(x2,y2,z2);
        p[2] = Vector3(x3,y3,z3);
    }
};
struct mesh
{
	vector<triangle> tris;

	bool LoadFromObjectFile(string sFilename)
	{
		ifstream f(sFilename);
		if (!f.is_open())
			return false;

		// Local cache of verts
		vector<Vector3> verts;

		while (!f.eof())
		{
			char line[128];
			f.getline(line, 128);

			std::istringstream s(line);

			char junk;

			if (line[0] == 'v')
			{
				Vector3 v;
				s >> junk >> v.x >> v.y >> v.z;
				verts.push_back(v);
			}

			if (line[0] == 'f')
			{
				int f[3];
				s >> junk >> f[0] >> f[1] >> f[2];
				tris.push_back({ verts[f[0] - 1], verts[f[1] - 1], verts[f[2] - 1] });
			}
		}

		return true;
	}

    void recenterMesh(mesh& m)
    {
        if (m.tris.empty()) return;

        Vector3 minv( 1e9f,  1e9f,  1e9f);
        Vector3 maxv(-1e9f, -1e9f, -1e9f);

        for (const auto& t : m.tris)
        {
            for (int i = 0; i < 3; i++)
            {
                const Vector3& v = t.p[i];
                minv.x = std::min(minv.x, v.x);
                minv.y = std::min(minv.y, v.y);
                minv.z = std::min(minv.z, v.z);
                maxv.x = std::max(maxv.x, v.x);
                maxv.y = std::max(maxv.y, v.y);
                maxv.z = std::max(maxv.z, v.z);
            }
        }

        Vector3 center(
            (minv.x + maxv.x) * 0.5f,
            (minv.y + maxv.y) * 0.5f,
            (minv.z + maxv.z) * 0.5f
        );

        for (auto& t : m.tris)
            for (int i = 0; i < 3; i++)
                t.p[i] = vector_sub(t.p[i], center);
    }

};
struct MeshGPU
{
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLsizei indexCount = 0;

    GLuint textureId = 0;
    std::string texturePath;

    void upload(const std::vector<Vertex>& verts, const std::vector<uint32_t>& indices)
    {
       indexCount = (GLsizei)indices.size();
       glGenVertexArrays(1, &vao);
       glGenBuffers(1, &vbo);
       glGenBuffers(1, &ebo);
       
       glBindVertexArray(vao);

       glBindBuffer(GL_ARRAY_BUFFER, vbo);
       glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);

       glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
       glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

       // position
       glEnableVertexAttribArray(0);
       glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,sizeof(Vertex), (void*) 0);

       // normal
       glEnableVertexAttribArray(1);
       glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*) (3 * sizeof(float)));


       //uv
       glEnableVertexAttribArray(2);
       glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*) (6 * sizeof(float)));

       glBindVertexArray(0);
       
    }

    void draw() const {

        if (textureId) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, textureId);
        }

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    }

    void destroy() {
        if (ebo) glDeleteBuffers(1, &ebo);
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        if (textureId) glDeleteTextures(1, &textureId);
        vao = vbo = ebo = 0;
        indexCount = 0;
    }

};
// the asstute observer should note that Gaussians are not actual geometry.
struct Gaussian{
    Vector3 pos;  // world space
    vec4f rot;  //  rot_0 to rot_3 -> x,y,z,w (quaternion)
    Vector3 scale;   // in log space, actual scale = exp(scale)
    float opacity; // logit space opacity , actual opacity = sigmoid(opacity)
    float sh[SH_TOTAL_FLOATS];
};

// GPU-friendly layout for instanced rendering (matches shader attribute layout).
struct GaussianGPU {
    vec4f position_opacity;  // xyz = position, w = opacity (logit)
    vec4f rot;               // quaternion
    vec4f scale;             // xyz = log scale, w = unused
    vec4f sh[3];              // SH coeffs per channel (R, G, B)
};