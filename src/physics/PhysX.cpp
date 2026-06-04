#include "physX.h"
#include "Scene.h"
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
    calcLocalBounds(scene);

}

PhysX::~PhysX()
{

}

void PhysX::step(float dt, Scene& scene)
{

}

void PhysX::calcLocalBounds(Scene& scene)
{       
    // for (SceneNode& s : scene.nodes)
    // {
    //     int rbIdx = s.rbIndex;
    //     int meshIdx = s.meshIndex;

    // }
}

void PhysX::updateWorldBounds(Scene& scene)
{   
    // bounds are updated by getting sceneNodes transform and multiplying all 8 vertices of bounds by it, then checking across these 8 vertices to get the new bounds
    for (SceneNode& s : scene.nodes)
    {
        int rbIdx = s.rbIndex;
        int meshIdx = s.meshIndex;
        mat4x4 m_matrix = s.modelMatrix();
        vec3f minCorner = scene.rbs[s.rbIndex].localMin;
        
        vec3f maxCorner = scene.rbs[s.rbIndex].localMax;

        vec3f vertices[8] = {
            minCorner,                                          // p1: 
            vec3f(maxCorner.x, minCorner.y, minCorner.z),      // p2: 
            vec3f(maxCorner.x, maxCorner.y, minCorner.z),      // p3: 
            vec3f(minCorner.x, maxCorner.y, minCorner.z),      // p4:
            vec3f(minCorner.x, minCorner.y, maxCorner.z),      // p5: 
            vec3f(maxCorner.x, minCorner.y, maxCorner.z),      // p6: 
            vec3f(minCorner.x, maxCorner.y, maxCorner.z),      // p7: 
            maxCorner                                           // p8: 
        };

        vec3f worldMax = {-1000,-1000,-1000};
        vec3f worldMin = {1000, 1000, 1000};
        for (vec3f v : vertices)
        {
            worldMax = std::max(worldMax,  vectorMatMul(v, m_matrix));
            worldMin = std::min(worldMin,  vectorMatMul(v, m_matrix));
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
