#include "physX.h"
#include "Scene.h"
#include "Material.h"
#include <algorithm>
#include <cmath>
#include <string>

// namespace {
// constexpr float kRestVelocityThreshold  = 0.5f;
// constexpr float kVelocitySleepThreshold = 0.003f;
// constexpr float kPositionSlop           = 0.001f;
// constexpr float kLinearDamping          = 0.05f;
// constexpr float groundLevel = 0.0;
// }

float PhysX::kRestVelocityThreshold = 0.5f;
float PhysX::kVelocitySleepThreshold = 0.003f;
float PhysX::kPositionSlop           = 0.001f;
float PhysX::kLinearDamping          = 0.05f;
float PhysX::groundLevel = 0.0;
float PhysX::Gravity = -2.8f;

PhysX::PhysX(Scene& scene)
{
    for (SceneNode& s : scene.nodes)
    {
        if (s.rbIndex >= 0 && s.rbIndex < static_cast<int>(scene.rbs.size()) && scene.rbs[s.rbIndex].isEnabled)
            activeRbIndices.push_back(s.rbIndex);
    }
}

void PhysX::step(float dt, Scene& scene)
{
    integrate(dt, scene);
    updateWorldBounds(scene);
    resolveCollision(scene);
    solveCollision(scene);
}

// set static should zero out velocity too, to prevent unintended consequences
void PhysX::setStatic(RigidBody& rb)
{
    // rb.isStatic = true; // increasingly this is getting more and more sloppy.
    rb.velocity = {0,0,0};
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
        scene.rbs[s.rbIndex].center = {s.position[0], s.position[1], s.position[2]};
    }
}
// collects all scene collisions
void PhysX::resolveCollision(Scene& scene)
{
    collisions.clear();
    const int nodeCount = static_cast<int>(scene.nodes.size());
    for (int i = 0; i < nodeCount; ++i)
    {
        SceneNode& s1 = scene.nodes[i];
        if (s1.rbIndex < 0 || s1.rbIndex >= static_cast<int>(scene.rbs.size()))
            continue;

        RigidBody& rb1 = scene.rbs[s1.rbIndex];
        if (!rb1.isEnabled)
            continue;

        for (int j = i + 1; j < nodeCount; ++j)
        {
            SceneNode& s2 = scene.nodes[j];
            if (s2.rbIndex < 0 || s2.rbIndex >= static_cast<int>(scene.rbs.size()))
                continue;

            RigidBody& rb2 = scene.rbs[s2.rbIndex];
            if (!rb2.isEnabled)
                continue;
            if (rb1.isStatic && rb2.isStatic)
                continue;
            if (collides(rb1, rb2))
                collisions.push_back({i, j});
        }
    }
}

bool PhysX::collides(RigidBody& rb1, RigidBody& rb2)
{
    vec3f min1 = rb1.worldMin;
    vec3f max1 = rb1.worldMax;
    vec3f min2 = rb2.worldMin;
    vec3f max2 = rb2.worldMax;

    bool xOverlap = (min1.x <= max2.x) && (max1.x >= min2.x);
    bool yOverlap = (min1.y <= max2.y) && (max1.y >= min2.y);
    bool zOverlap = (min1.z <= max2.z) && (max1.z >= min2.z);

    return xOverlap && yOverlap && zOverlap;
}

void PhysX::solveCollision(Scene& scene)
{
    // solve collision. note that the solver depends on whether the object is static or not.
    // if not static apply -velocity 
    for (auto& c : collisions)
    {
        SceneNode& node1 = scene.nodes[c.first];
        SceneNode& node2 = scene.nodes[c.second];
        RigidBody& rb1 = scene.rbs[node1.rbIndex];
        RigidBody& rb2 = scene.rbs[node2.rbIndex];

        if (rb1.isStatic && rb2.isStatic) continue;

        vec3f normal = calcCollisionNormal(rb1, rb2);
        // Normal points rb1 -> rb2; use v2 - v1 so approaching contacts have vn < 0
        
        // impulse: only apply if objects are approaching
        vec3f relativeVelocity = rb2.velocity - rb1.velocity;
        float velocityAlongNormal = vector_dot(relativeVelocity, normal);

        // already seperating so we can skip
        if (velocityAlongNormal > 0.0f) continue;

        // project velocities on the collision normal
        float v1 = vector_dot(rb1.velocity, normal);
        float v2 = vector_dot(rb2.velocity, normal);


        float totalMass = (rb1.mass + rb2.mass);
        float restitut = 0.5f * (rb1.restitution + rb2.restitution);

        float Nominator1 = ((rb1.mass - restitut * rb2.mass) * v1) + ((restitut+1.0) * rb2.mass * v2);
        float Nominator2 = ((rb2.mass - restitut * rb1.mass) * v2) + ((restitut+1.0) * rb1.mass * v1);
        float vf1 = Nominator1 / totalMass;
        float vf2 = Nominator2 / totalMass;
        
        // back to 3d
        if (!rb1.isStatic) rb1.velocity += (vf1 - v1) * normal;
        if (!rb2.isStatic) rb2.velocity += (vf2 - v2) * normal;

    }
    collisions.clear();
}

vec3f PhysX::calcCollisionNormal(RigidBody& rb1, RigidBody& rb2)
{
    vec3f min1 = rb1.worldMin, max1 = rb1.worldMax;
    vec3f min2 = rb2.worldMin, max2 = rb2.worldMax;

    vec3f center1 = (min1 + max1) * 0.5f;
    vec3f center2 = (min2 + max2) * 0.5f;

    vec3f delta = center2 - center1;

    float overlapX = std::min(max1.X, max2.x) - std::max(min1.x, min2.x);
    float overlapY = std::min(max1.y, max2.y) - std::max(min1.y, min2.y);
    float overlapZ = std::min(max1.z, max2.z) - std::max(min1.z, min2.z);

    vec3f normal = {0, 0, 0};

    if (overlapX < overlapY && overlapX < overlapZ) {
        normal = {1, 0, 0};
        if (delta.x < 0) normal.x = -1;
    }
    else if (overlapY < overlapX && overlapY < overlapZ) {
        normal = {0, 1, 0};
        if (delta.y < 0) normal.y = -1;
    }
    else {
        normal = {0, 0, 1};
        if (delta.z < 0) normal.z = -1;
    }

    return vector_normalize(normal);
}

float PhysX::calcPenetrationDepth(RigidBody& rb1, RigidBody& rb2)
{
    vec3f normal = calcCollisionNormal(rb1, rb2);

    if (normal.y != 0)
        return std::min(rb1.worldMax.y, rb2.worldMax.y) - std::max(rb1.worldMin.y, rb2.worldMin.y);
    if (normal.x != 0)
        return std::min(rb1.worldMax.x, rb2.worldMax.x) - std::max(rb1.worldMin.x, rb2.worldMin.x);

    return std::min(rb1.worldMax.z, rb2.worldMax.z) - std::max(rb1.worldMin.z, rb2.worldMin.z);
}

void PhysX::removeProjectileNode(Scene& scene, int nodeIndex)
{
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(scene.nodes.size()))
        return;

    const int rbIndex = scene.nodes[nodeIndex].rbIndex;
    const int lastNode = static_cast<int>(scene.nodes.size()) - 1;
    const int swappedNode = (nodeIndex != lastNode) ? lastNode : -1;

    if (nodeIndex != lastNode)
        scene.nodes[nodeIndex] = std::move(scene.nodes[lastNode]);
    scene.nodes.pop_back();

    for (auto it = projectileNodeIndices_.begin(); it != projectileNodeIndices_.end(); ) {
        if (*it == nodeIndex) {
            it = projectileNodeIndices_.erase(it);
            continue;
        }
        if (swappedNode >= 0 && *it == swappedNode)
            *it = nodeIndex;
        else if (*it > nodeIndex)
            --(*it);
        ++it;
    }

    if (rbIndex < 0 || rbIndex >= static_cast<int>(scene.rbs.size()))
        return;

    const int lastRb = static_cast<int>(scene.rbs.size()) - 1;
    if (rbIndex != lastRb) {
        scene.rbs[rbIndex] = std::move(scene.rbs[lastRb]);
        for (SceneNode& n : scene.nodes) {
            if (n.rbIndex == lastRb)
                n.rbIndex = rbIndex;
        }
    }
    scene.rbs.pop_back();
}

void PhysX::shootProjectile(Scene& scene, int meshIndex, const vec3f& origin,
                            const vec3f& direction, float speed, float scale)
{
    static int projectileCounter = 0;

    while (static_cast<int>(projectileNodeIndices_.size()) >= kMaxProjectiles) {
        const int oldestNode = projectileNodeIndices_.front();
        removeProjectileNode(scene, oldestNode);
    }

    vec3f dir = direction;
    const float len = vector_length(dir);
    if (len > 0.0001f)
        dir = vector_mul(dir, 1.0f / len);

    const vec3f spawnOffset = vector_mul(dir, 1.5f);
    const vec3f spawnPos = vector_add(origin, spawnOffset);

    const int rbIndex = static_cast<int>(scene.rbs.size());
    RigidBody rb;
    rb.localMin = {-0.5f, -0.5f, -0.5f};
    rb.localMax = {0.5f, 0.5f, 0.5f};
    rb.isStatic = false;
    rb.useGravity = true;
    rb.mass = 1.0f;
    rb.restitution = 0.4f;
    rb.velocity = vector_mul(dir, -speed);
    scene.rbs.push_back(rb);

    Material mat;
    mat.name = "Projectile";
    mat.type = Material::Type::Phong;
    mat.diffuseColor[0] = 1.0f;
    mat.diffuseColor[1] = 0.35f;
    mat.diffuseColor[2] = 0.1f;
    mat.shininess = 64.0f;

    SceneNode& node = scene.addNode(
        "Projectile_" + std::to_string(projectileCounter++), meshIndex, 0);
    node.rbIndex = rbIndex;
    node.position[0] = spawnPos.X;
    node.position[1] = spawnPos.Y;
    node.position[2] = spawnPos.Z;
    node.scale[0] = scale;
    node.scale[1] = scale;
    node.scale[2] = scale;

    projectileNodeIndices_.push_back(static_cast<int>(scene.nodes.size()) - 1);
}

void PhysX::integrate(float dt, Scene& scene)
{
    for (SceneNode& s : scene.nodes) {
        if (s.rbIndex < 0 || s.rbIndex >= static_cast<int>(scene.rbs.size()))
            continue;

        RigidBody& rb = scene.rbs[s.rbIndex];
        if (rb.isStatic || !rb.isEnabled)
            continue;

        if (rb.useGravity  && rb.position.y > groundLevel)
             rb.velocity.Y += Gravity * dt;

        const float damping = 1.0f / (1.0f + kLinearDamping * dt);
        rb.velocity.X *= damping;
        rb.velocity.Y *= damping;
        rb.velocity.Z *= damping;

        rb.position.x += rb.velocity.X * dt;
        rb.position.y += rb.velocity.Y * dt;
        rb.position.z += rb.velocity.Z * dt;

        const float speedSq = rb.velocity.X * rb.velocity.X
                            + rb.velocity.Y * rb.velocity.Y
                            + rb.velocity.Z * rb.velocity.Z;
        if (speedSq < kVelocitySleepThreshold * kVelocitySleepThreshold)
            rb.velocity = {0.0f, 0.0f, 0.0f};

    }
}

void PhysX::updateTransforms(Scene& scene)
{
    for (SceneNode& s : scene.nodes) {
        if (s.rbIndex < 0 || s.rbIndex >= static_cast<int>(scene.rbs.size()))
            continue;

        RigidBody& rb = scene.rbs[s.rbIndex];
        // if (rb.isStatic || !rb.isEnabled)
        //     continue;
        std::copy(&rb.position.x, &rb.position.x + 3, s.position);
    }
}
