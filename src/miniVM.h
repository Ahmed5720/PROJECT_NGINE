#pragma once
#include <vector>
#include <cmath>

// mini vector / matrix library
struct vec2f
{
    float x,y;
    vec2f()
    {
        x = 0;
        y = 0;
    }
    vec2f(float _x, float _y)
    {
        x = _x;
        y = _y;
    }
};
struct vec3i
{
    int x,y,z;
    vec3i(int _x, int _y, int _z)
    {
        x = _x;
        y = _y;
        z = _z;
    }
    vec3i()
    {
        x = 0;
        y = 0;
        z = 0;
    }
};
struct vec3f
{
    //float x,y,z,w = 1.0f;
    float x,y,z;
    vec3f()
    {
        x = 0;
        y = 0;
        z = 0;
       // w = 1.0f;
    }
    vec3f(float _x, float _y, float _z)
    {
        x = _x;
        y = _y;
        z = _z;
    }
    vec3f(float _x, float _y, float _z, float _w)
    {
        x = _x;
        y = _y;
        z = _z;
     //   w = _w; 
    }
    vec3f(vec3i vi)
    {
        x = float(vi.x);
        y = float(vi.y);
        z = float(vi.z);
     //   w = 1.0f;
    }
};
struct vec4f
{
    float x,y,z,w = 1.0f;
    vec4f()
    {
        x = 0;
        y = 0;
        z = 0;
        w = 1.0f;
    }
    vec4f(float _x, float _y, float _z, float _w)
    {
        x = _x;
        y = _y;
        z = _z;
     //   w = _w; 
    }

};

inline vec3f& operator+=(vec3f& v1, const vec3f& v2) {
    v1.x += v2.x; v1.y += v2.y; v1.z += v2.z;
    return v1;
}
inline vec3f& operator*=(vec3f& v1, const vec3f& v2) {
    v1.x *= v2.x; v1.y *= v2.y; v1.z *= v2.z;
    return v1;
}

struct mat4x4 {
    float m[4][4] = {0};
};

inline vec3f vector_add(const vec3f& vec1, const vec3f& vec2) {
    return {vec1.x + vec2.x, vec1.y + vec2.y, vec1.z + vec2.z};
}
inline vec3f vector_sub(const vec3f& vec1, const vec3f& vec2) {
    return {vec1.x - vec2.x, vec1.y - vec2.y, vec1.z - vec2.z};
}
inline vec3f vector_mul(vec3f vec, float s) {
    return {vec.x * s, vec.y * s, vec.z * s};
}
inline vec3f vector_div(vec3f vec, float s) {
    return {vec.x / s, vec.y / s, vec.z / s};
}
inline vec3f vector_cross(vec3f vec1, vec3f vec2) {
    return {vec1.y * vec2.z - vec1.z * vec2.y, vec1.z * vec2.x - vec1.x * vec2.z, vec1.x * vec2.y - vec1.y * vec2.x};
}
inline float vector_dot(const vec3f& vec1, const vec3f& vec2) {
    return vec1.x * vec2.x + vec1.y * vec2.y + vec1.z * vec2.z;
}
inline float vector_dot(const vec4f& vec1, const vec4f& vec2) {
    return vec1.x * vec2.x + vec1.y * vec2.y + vec1.z * vec2.z + vec1.w * vec2.w;
}
inline float vector_length(const vec3f& v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}
inline vec3f vector_normalize(const vec3f& vec1) {
    float l = vector_length(vec1);
    return {vec1.x / l, vec1.y / l, vec1.z / l};
}
inline vec3f vectorMatMul(const vec3f& v, const mat4x4& m) {
    vec3f o;
    o.x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0];
    o.y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1];
    o.z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2];
    return o;
}
inline mat4x4 matrix_makeIdentity() {
    mat4x4 mat;
    mat.m[0][0] = mat.m[1][1] = mat.m[2][2] = mat.m[3][3] = 1.0f;
    return mat;
}
inline mat4x4 matrix_makeTranslation(float x, float y, float z) {
    mat4x4 mat = matrix_makeIdentity();
    mat.m[3][0] = x; mat.m[3][1] = y; mat.m[3][2] = z;
    return mat;
}
inline mat4x4 matrix_makeRotationX(float angleRad) {
    mat4x4 mat;
    mat.m[0][0] = 1;
    mat.m[1][1] = cosf(angleRad); mat.m[1][2] = sinf(angleRad);
    mat.m[2][1] = -sinf(angleRad); mat.m[2][2] = cosf(angleRad);
    mat.m[3][3] = 1;
    return mat;
}
inline mat4x4 matrix_makeRotationY(float angleRad) {
    mat4x4 mat;
    mat.m[0][0] = cosf(angleRad); mat.m[0][2] = sinf(angleRad);
    mat.m[2][0] = -sinf(angleRad); mat.m[1][1] = 1.0f; mat.m[2][2] = cosf(angleRad); mat.m[3][3] = 1.0f;
    return mat;
}
inline mat4x4 matrix_makeRotationZ(float angleRad) {
    mat4x4 mat;
    mat.m[0][0] = cosf(angleRad); mat.m[0][1] = sinf(angleRad);
    mat.m[1][0] = -sinf(angleRad); mat.m[1][1] = cosf(angleRad);
    mat.m[2][2] = mat.m[3][3] = 1.0f;
    return mat;
}
inline mat4x4 matrix_makeProjection(float fovDeg, float aspect, float zNear, float zFar) {
    float f = 1.0f / tanf(fovDeg * 0.5f * 3.1415f / 180.0f);
    mat4x4 m;
    m.m[0][0] = f / aspect; m.m[1][1] = f;
    m.m[2][2] = (zFar + zNear) / (zNear - zFar); m.m[2][3] = -1.0f;
    m.m[3][2] = (2 * zFar * zNear) / (zNear - zFar); m.m[3][3] = 0.0f;
    return m;
}
inline mat4x4 matrix_pointAt(const vec3f& pos, const vec3f& target, const vec3f& up) {
    vec3f forward = vector_sub(target, pos);
    forward = vector_normalize(forward);

    // Calculate new Up direction
    vec3f a = vector_mul(forward, vector_dot(up, forward));
    vec3f newUp = vector_sub(up, a);
    newUp = vector_normalize(newUp);

    // New Right direction is easy, its just cross product
    vec3f right = vector_cross(newUp, forward);
    mat4x4 matrix;
    matrix.m[0][0] = right.x;
    matrix.m[1][0] = up.x;
    matrix.m[2][0] = forward.x;
    matrix.m[3][0] = pos.x;

    matrix.m[0][1] = right.y;
    matrix.m[1][1] = up.y;
    matrix.m[2][1] = forward.y;
    matrix.m[3][1] = pos.y;

    matrix.m[0][2] = right.z;
    matrix.m[1][2] = up.z;
    matrix.m[2][2] = forward.z;
    matrix.m[3][2] = pos.z;

    matrix.m[0][3] = 0.0f;
    matrix.m[1][3] = 0.0f;
    matrix.m[2][3] = 0.0f;
    matrix.m[3][3] = 1.0f;


    return matrix;
}
inline mat4x4 matrix_quickInvert(const mat4x4& m) {
    mat4x4 matrix;
    matrix.m[0][0] = m.m[0][0]; matrix.m[0][1] = m.m[1][0]; matrix.m[0][2] = m.m[2][0]; matrix.m[0][3] = 0.0f;
    matrix.m[1][0] = m.m[0][1]; matrix.m[1][1] = m.m[1][1]; matrix.m[1][2] = m.m[2][1]; matrix.m[1][3] = 0.0f;
    matrix.m[2][0] = m.m[0][2]; matrix.m[2][1] = m.m[1][2]; matrix.m[2][2] = m.m[2][2]; matrix.m[2][3] = 0.0f;
    matrix.m[3][0] = -(m.m[3][0] * matrix.m[0][0] + m.m[3][1] * matrix.m[1][0] + m.m[3][2] * matrix.m[2][0]);
    matrix.m[3][1] = -(m.m[3][0] * matrix.m[0][1] + m.m[3][1] * matrix.m[1][1] + m.m[3][2] * matrix.m[2][1]);
    matrix.m[3][2] = -(m.m[3][0] * matrix.m[0][2] + m.m[3][1] * matrix.m[1][2] + m.m[3][2] * matrix.m[2][2]);
    matrix.m[3][3] = 1.0f;
    return matrix;
}
inline mat4x4 matrix_matmul(const mat4x4& m1, const mat4x4& m2) {
    mat4x4 res;
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            res.m[r][c] = m1.m[r][0] * m2.m[0][c] + m1.m[r][1] * m2.m[1][c] + m1.m[r][2] * m2.m[2][c] + m1.m[r][3] * m2.m[3][c];
    return res;
}