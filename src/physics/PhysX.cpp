#include "physX.h"

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
    for (SceneNode& s : scene.nodes)
    {
        int rbIdx = s.rbIndex;
        int meshIdx = s.meshIndex;
    }
}
void PhysX::resolveCollision(Scene& scene)
{

}
void PhysX::integrate(Scene& scene)
{

}
