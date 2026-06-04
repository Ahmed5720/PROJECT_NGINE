#include "physX.h"
#include "Scene.h"
#include <algorithm>



PhysX::PhysX(Scene& scene)
{
    // initialize activeRBindices vector
    int i = 0;
    for (SceneNode& s : scene.nodes)
    {   
        int rbIndex = s.rbIndex;
        if(scene.rbs[i].isEnabled)
        {
            activeRbIndices.push_back(i);
            i++;
            
        }
    }

}

void PhysX::step(float dt, Scene& scene)
{
    updateWorldBounds(scene);

    for (SceneNode& s : scene.nodes) {
        if (s.rbIndex < 0 || s.rbIndex >= static_cast<int>(scene.rbs.size()))
            continue;
        RigidBody rb = scene.rbs[s.rbIndex];
        vec3f force;
        if (rb.isStatic == false && rb.useGravity)
        {
            force += vec3f(0, rb.mass * Gravity, 0);
            rb.velocity += force / (rb.mass * dt);
            force = {0,0,0};
            
            s.position[0] += rb.velocity.X * dt;
            s.position[1] += rb.velocity.Y * dt;
            s.position[2] += rb.velocity.Z * dt;

        }
    }
}


void PhysX::updateWorldBounds(Scene& scene)
{
    for (const SceneNode& s : scene.nodes) {
        if (s.rbIndex < 0 || s.rbIndex >= static_cast<int>(scene.rbs.size()))
            continue;

        const mat4x4 m_matrix = s.modelMatrix();
        const vec3f minCorner = scene.rbs[s.rbIndex].localMin;
        const vec3f maxCorner = scene.rbs[s.rbIndex].localMax;

        const vec3f vertices[8] = {
            minCorner,
            vec3f(maxCorner.X, minCorner.Y, minCorner.Z),
            vec3f(maxCorner.X, maxCorner.Y, minCorner.Z),
            vec3f(minCorner.X, maxCorner.Y, minCorner.Z),
            vec3f(minCorner.X, minCorner.Y, maxCorner.Z),
            vec3f(maxCorner.X, minCorner.Y, maxCorner.Z),
            vec3f(minCorner.X, maxCorner.Y, maxCorner.Z),
            maxCorner
        };

        vec3f worldMax = {-1000.0f, -1000.0f, -1000.0f};
        vec3f worldMin = {1000.0f, 1000.0f, 1000.0f};
        for (const vec3f& v : vertices) {
            const vec3f w = vectorMatMul(v, m_matrix);
            worldMax = vecMax(worldMax, w);
            worldMin = vecMin(worldMin, w);
        }

        scene.rbs[s.rbIndex].worldMax = worldMax;
        scene.rbs[s.rbIndex].worldMin = worldMin;
    }
}
void PhysX::resolveCollision(Scene& scene)
{
    
}
void PhysX::integrate(Scene& scene)
{

}
