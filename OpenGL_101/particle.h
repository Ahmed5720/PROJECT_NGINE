#include <glad/glad.h>
#include <GLFW/glfw3.h>


#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <iostream>
#include <algorithm>

class particle
{
public:
    // renders (and builds at first invocation) a sphere // adapted from learnopengl.com
    // -------------------------------------------------
    unsigned int sphereVAO = 0;
    unsigned int indexCount;
    std::vector<glm::vec3> velocities;
    std::vector<glm::vec3> positions;
    std::vector<float> densities;
    float smoothingRadius = 3.0f;
    float mass = 10;
    float targetDensity = 1;
    float pressureMultiplier = 1;
    float damping = 0.7;
    double PI = 3.14159265358979323846;
    float min_dist = 0.2f;


    glm::vec3 red = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 yellow = glm::vec3(1.0f, 1.0f, 0.0f);
    glm::vec3 green = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 blue = glm::vec3(0.0f, 0.0f, 1.0f);
    void renderSphere()
    {
        if (sphereVAO == 0)
        {
            glGenVertexArrays(1, &sphereVAO);

            unsigned int vbo, ebo;
            glGenBuffers(1, &vbo);
            glGenBuffers(1, &ebo);

            std::vector<glm::vec3> positions;
            std::vector<glm::vec2> uv;
            std::vector<glm::vec3> normals;
            std::vector<unsigned int> indices;

            const unsigned int X_SEGMENTS = 64;
            const unsigned int Y_SEGMENTS = 64;
            const float PI = 3.14159265359f;
            for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
            {
                for (unsigned int y = 0; y <= Y_SEGMENTS; ++y)
                {
                    float xSegment = (float)x / (float)X_SEGMENTS;
                    float ySegment = (float)y / (float)Y_SEGMENTS;
                    float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
                    float yPos = std::cos(ySegment * PI);
                    float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

                    positions.push_back(glm::vec3(xPos, yPos, zPos));
                    uv.push_back(glm::vec2(xSegment, ySegment));
                    normals.push_back(glm::vec3(xPos, yPos, zPos));
                }
            }

            bool oddRow = false;
            for (unsigned int y = 0; y < Y_SEGMENTS; ++y)
            {
                if (!oddRow) // even rows: y == 0, y == 2; and so on
                {
                    for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
                    {
                        indices.push_back(y * (X_SEGMENTS + 1) + x);
                        indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                    }
                }
                else
                {
                    for (int x = X_SEGMENTS; x >= 0; --x)
                    {
                        indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                        indices.push_back(y * (X_SEGMENTS + 1) + x);
                    }
                }
                oddRow = !oddRow;
            }
            indexCount = static_cast<unsigned int>(indices.size());

            std::vector<float> data;
            for (unsigned int i = 0; i < positions.size(); ++i)
            {
                data.push_back(positions[i].x);
                data.push_back(positions[i].y);
                data.push_back(positions[i].z);
                if (normals.size() > 0)
                {
                    data.push_back(normals[i].x);
                    data.push_back(normals[i].y);
                    data.push_back(normals[i].z);
                }
                if (uv.size() > 0)
                {
                    data.push_back(uv[i].x);
                    data.push_back(uv[i].y);
                }
            }
            glBindVertexArray(sphereVAO);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
            unsigned int stride = (3 + 2 + 3) * sizeof(float);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
        }

        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLE_STRIP, indexCount, GL_UNSIGNED_INT, 0);
    }

    //smoothed particle hydrodynamics
    //first we need a function to calculate a particle's influence at a given distance from it
    float smoothingKernel(float radius, float dst)
    {   
        
        float volume = PI * pow(radius, 8) / 4;
        float value = std::max(0.0f, radius * radius - dst * dst);
        return dst == 0? 0 : value * value * value / volume;
    }


    float smoothingKernelDerivative(float radius, float dst)
    {
        if (dst >= radius) return 0;
        float f = radius * radius - dst * dst;
        float scale = -24 / (PI * pow(radius, 8));
        return scale * dst * f * f;
    }
    //now we can calculate density at a given point by summing up particle influences at this point
    
    float CalcDensity(glm::vec3 sample)
    {
        float density = 0;
       
        int i = 0;
        for (const glm::vec3 pos : positions)
        {
            float dst = glm::distance(pos, sample);
            density += mass * smoothingKernel(smoothingRadius, dst);
            i++;
        }

        return density;
    }


    float DensityToPressure(float density)
    {
        float densityError = density - targetDensity;
        return densityError * pressureMultiplier;
    }

    glm::vec3 CalcPressureForce(glm::vec3 sample)
    {
        glm::vec3 force(0,0,0);
        int i = 0;
        for (const glm::vec3 pos : positions)
        {
            float dst = glm::distance(pos, sample);
            if (fabsf(dst) > 0) {

                glm::vec3 dir = (pos - sample) / dst;
                float slope = smoothingKernelDerivative(smoothingRadius, dst);
                float density = densities[i];
                force += DensityToPressure(density) * dir * slope * mass / density;
               
            }
            i++;
        }
        return force;
    }

    
    void check_particle_collision(int id)
    {
        for (int i = 0; i < positions.size(); i++) {
            if (i == id) continue;

            glm::vec3 delta = positions[id] - positions[i];
            float dist = glm::length(delta);

            if (dist < min_dist && dist > 1e-6f) {
                glm::vec3 normal = delta / dist;

                // --- Velocity along normal
                float v1n = glm::dot(velocities[id], normal);
                float v2n = glm::dot(velocities[i], normal);

                // --- Swap normal components (elastic collision, equal masses)
                float temp = v1n;
                v1n = v2n;
                v2n = temp;

                // --- Recombine into new velocities
                velocities[id] += (v1n - glm::dot(velocities[id], normal)) * normal;
                velocities[i] += (v2n - glm::dot(velocities[i], normal)) * normal;

                // --- Push them apart to avoid overlap
                float penetration = min_dist - dist;
                glm::vec3 correction = 0.5f * penetration * normal;
                positions[id] += correction;
                positions[i] -= correction;
            }
        }
    }
    void check_collision(float x1, float x2, float y1, float y2, float z1, float z2, int i)
    {   

        // checks if particle collides with bounding box, if so invert velocity direction and damp it.
        // X-axis
        if (positions[i].x < x1) {
            positions[i].x = x1; // clamp inside
            velocities[i].x *= -damping;
        }
        else if (positions[i].x > x2) {
            positions[i].x = x2;
            velocities[i].x *= -damping;
        }

        // Y-axis
        if (positions[i].y < y1) {
            positions[i].y = y1;
            velocities[i].y *= -damping;
        }
        else if (positions[i].y > y2) {
            positions[i].y = y2;
            velocities[i].y *= -damping;
        }

        // Z-axis
        if (positions[i].z < z1) {
            positions[i].z = z1;
            velocities[i].z *= -damping;
        }
        else if (positions[i].z > z2) {
            positions[i].z = z2;
            velocities[i].z *= -damping;
        }
    }

    glm::vec3 heatmap(float minVal, float maxVal, float val) {
        // Normalize val to [0,1]
        float t = (val - minVal) / (maxVal - minVal);
        t = std::clamp(t, 0.0f, 1.0f);
        t *= t;
        

        if (t < 0.33f) {
            float localT = t / 0.33f; // [0,1] within [0,0.33]
            return glm::mix(blue, green, localT);
        }
        else if (t < 0.66f) {
            float localT = (t - 0.33f) / (0.33f); // [0,1] within [0.33,0.66]
            return glm::mix(green, yellow, localT);
        }
        else {
            float localT = (t - 0.66f) / (0.34f); // [0,1] within [0.66,1.0]
            return glm::mix(yellow, red, localT);
        }
    }
};