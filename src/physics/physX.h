#pragma once
#include "miniVM.h"



struct Scene;// forward declaration
struct RigidBody
{
    vec3f localMin = {0,0,0}; // local bounding box x,y,z min and max determined by scene node mesh at initialization
    vec3f localMax = {1,1,1};
    vec3f worldMin = {0,0,0};
    vec3f worldMax = {1,1,1};
    vec3f velocity = {0,0,0};
    vec3f position;
    vec3f force = {0,0,0};
    vec3f center = {0,0,0}; // to do: ensure that center actually updates with sceneNode position
    float mass = 1.0f;
    float restitution = 3.5f;
    bool isStatic = true; // responds to collisions when enabled is true
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
{   
    public:
        static float kRestVelocityThreshold;
        static float kVelocitySleepThreshold;
        static float kPositionSlop;
        static float kLinearDamping;
        static float groundLevel;
        static float Gravity;

        const float eps = 0.01; // min collision distance
        static constexpr int kMaxProjectiles = 32;
        std::vector<std::pair<int, int>> collisions; // scene node index pairs
        std::vector<int> projectileNodeIndices_;

        explicit PhysX(Scene& scene);
        ~PhysX() = default;
        void step(float dt, Scene& scene);
        void updateWorldBounds(Scene& scene);
        void updateTransforms(Scene& scene);
        static void setStatic(RigidBody& rb);
        void shootProjectile(Scene& scene, int meshIndex, const vec3f& origin,
                             const vec3f& direction, float speed, float scale = 0.25f);
    private:
        vec3f calcCollisionNormal(RigidBody& rb1, RigidBody& rb2);
        float calcPenetrationDepth(RigidBody& rb1, RigidBody& rb2);
        void solveCollision(Scene& scene);
        bool collides(RigidBody& s1, RigidBody& s2);
        void resolveCollision(Scene& scene);
        void integrate(float dt, Scene& scene);
        void removeProjectileNode(Scene& scene, int nodeIndex);
        std::vector<int> activeRbIndices;

};