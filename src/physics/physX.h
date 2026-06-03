#pragma once
#include "Scene.h"
#include "miniVM.h"
struct RigidBody
{
    vec3f localMin = {0,0,0}; // local bounding box x,y,z min and max determined by scene node mesh at initialization
    vec3f localMax = {1,1,1};
    vec3f worldMin = {0,0,0};
    vec3f worldMax = {1,1,1};
    vec3f velocity = {0,0,0};
    float mass = 1.0f;
    bool isStatic = false; // responds to collisions when enabled is true
    bool useGravity = false; 
    bool isEnabled = true;
};
// initially physX initializes by getting all sceneNodes with RigidBody components enabled and adding their indices to active
// RB indices, then using their mesh it computes localmin and max of the RB component.

// in Step() the following is done every frame:
//  1. we update world bounds of each active rb object by multiplying local bounds with the scene node transform
//  2. we update positions by applying gravity and velocity to it
//  3. we resolve collisions by sorting all rb objects by their min x and checking if ith rb overlaps with i+1th rb a sort and sweep method faster than an n2
//  4. when theres an overlap we find the minimum translation vector and push the objects apart along that axe
//  
class PhysX
{   const float Gravity = -9.8;
    const float eps = 0.01; // min collision distance
    public:
        explicit PhysX(Scene& scene);
        ~PhysX() = default;
        void step(float dt, Scene& scene);
    private:
        void calcLocalBounds(Scene& s); // only at start
        void updateWorldBounds(Scene& scene);
        void resolveCollision(Scene& scene);
        void integrate(Scene& scene);
        std::vector<int> activeRbIndices;

};