#pragma once
#include "Camera.h"
#include "geometry.h"
#include <vector>
#include "Material.h"
#include "LightEnvironment.h"

// represents a renderable object in the world
// meshGPU is referenced by indexing into Scene::meshes so two nodes can share the same geometry
// RigidBody handle is an index into physicsWorld::bodies
struct SceneNode
{
    
    // contains transform, Mesh handle, materialHandle, RigidBodyHandle for each scene object
    std::string name;

    //transform
    float position[3] = {0.0f, 0.0f, 0.0f};
    float rotation[3] = {0.0f, 0.0f, 0.0f}; // Euler angles, degrees, XYZ order
    float scale[3]    = {1.0f, 1.0f, 1.0f};

    // resource handles
    int meshIndex = -1;
    int rigidBodyHandle = -1;


    // Compose the model matrix from position/rotation/scale.
    // Rotation order: Rx * Ry * Rz (XYZ Euler, degrees).
    mat4x4 modelMatrix() const {
        const float deg2rad = 3.14159265f / 180.0f;
        mat4x4 S = matrix_makeScale(scale[0], scale[1], scale[2]);
        mat4x4 Rx = matrix_makeRotationX(rotation[0] * deg2rad);
        mat4x4 Ry = matrix_makeRotationY(rotation[1] * deg2rad);
        mat4x4 Rz = matrix_makeRotationZ(rotation[2] * deg2rad);
        mat4x4 T = matrix_makeTranslation(position[0], position[1], position[2]);
        // T * Rz * Ry * Rx * S  (column-major convention: applied right to left)
        return matrix_matmul(T, matrix_matmul(Rz, matrix_matmul(Ry, matrix_matmul(Rx, S))));
    }


    // material
    Material material;
    //flags
    bool visible = true;
    bool castsShadow = true;
    bool receivesShadow = true;


};


// central world state which owns the following:

// camera
// scene node list
// Mesh GPU list
struct Scene {
    Camera camera;

    std::vector<SceneNode> nodes;
    std::vector<MeshGPU> meshes;

    LightEnvironment lights;

    //std::vector<Gaussian> gaussians;
    SceneNode& addNode(const std::string& name, int meshIndex, Material&& mat)
    {
        SceneNode node;
        node.name = name;
        node.meshIndex = meshIndex;
        node.material = std::move(mat);
        nodes.push_back(std::move(node));
        return nodes.back();
    }
    float backgroundColor[3] = {0.2f, 0.5f, 0.5f};


    void destroy() {
        for (auto &mesh : meshes)
            mesh.destroy();
    }
};
