#pragma once
#include "miniVM.h"

struct Camera {
    vec3f position = {0.0f, 0.0f, 0.0f};
    float yaw      = 0.0f;  // radians
    vec3f up       = {0.0f, 1.0f, 0.0f};

    vec3f getForward() const {
        vec3f dir = {0.0f, 0.0f, 1.0f};
        mat4x4 rot = matrix_makeRotationY(yaw);
        dir = vectorMatMul(dir, rot);
        return vector_normalize(dir);
    }

    mat4x4 getViewMatrix() const {
        vec3f lookDir = getForward();
        vec3f target = vector_add(position, lookDir);
        mat4x4 cam = matrix_pointAt(position, target, up);
        return matrix_quickInvert(cam);
    }

    mat4x4 getProjectionMatrix(float aspect, float zNear, float zFar) const {
        return matrix_makeProjection(fovDeg, aspect, zNear, zFar);
    }

    float fovDeg = 90.0f;
};
