#pragma once
#include <vector>
#include <cmath>
#define x X
#define y Y
#define z Z

// mini vector / matrix librarY
struct vec2f
{
    float X,Y;
    vec2f()
    {
        X = 0;
        Y = 0;
    }
    vec2f(float _X, float _Y)
    {
        X = _X;
        Y = _Y;
    }
};
struct vec3i
{
    int X,Y,Z;
    vec3i(int _X, int _Y, int _Z)
    {
        X = _X;
        Y = _Y;
        Z = _Z;
    }
    vec3i()
    {
        X = 0;
        Y = 0;
        Z = 0;
    }
};
struct vec3f
{
    
    float X,Y,Z,W;
    vec3f()
    {
        X = 0;
        Y = 0;
        Z = 0;
       // w = 1.0f;
    }
    vec3f(float _X, float _Y, float _Z)
    {
        X = _X;
        Y = _Y;
        Z = _Z;
    }
    vec3f(float* xyz)
    {
        X = xyz[0];
        Y = xyz[1];
        Z = xyz[2];
    }
    vec3f(float _X, float _Y, float _Z, float _w)
    {
        X = _X;
        Y = _Y;
        Z = _Z;
        W = _w; 
    }
    vec3f(vec3i vi)
    {
        X = float(vi.X);
        Y = float(vi.Y);
        Z = float(vi.Z);
     //   w = 1.0f;
    }
};
struct vec4f
{
    float X,Y,Z,w = 1.0f;
    vec4f()
    {
        X = 0;
        Y = 0;
        Z = 0;
        w = 1.0f;
    }
    vec4f(float _X, float _Y, float _Z, float _w)
    {
        X = _X;
        Y = _Y;
        Z = _Z;
     //   w = _w; 
    }

};

inline vec3f vecMin(const vec3f& a, const vec3f& b) {
    return {std::min(a.X, b.X), std::min(a.Y, b.Y), std::min(a.Z, b.Z)};
}
inline vec3f vecMax(const vec3f& a, const vec3f& b) {
    return {std::max(a.X, b.X), std::max(a.Y, b.Y), std::max(a.Z, b.Z)};
}

inline vec3f operator*(const vec3f& v, const float f)
{
    return {v.X * f, v.Y * f, v.Z * f};
}

inline vec3f operator*(const float f, const vec3f& v)
{
    return {v.X * f, v.Y * f, v.Z * f};
}

inline vec3f& operator+=(vec3f& v1, const vec3f& v2) {
    v1.X += v2.X; v1.Y += v2.Y; v1.Z += v2.Z;
    return v1;
}
inline vec3f operator+(const vec3f v1, const vec3f& v2) {
    return {v1.X + v2.X, v1.Y + v2.Y, v1.Z + v2.Z};
}
inline vec3f operator-(const vec3f v1, const vec3f& v2) {
    return {v1.X - v2.X, v1.Y - v2.Y, v1.Z - v2.Z};
}
inline vec3f& operator*=(vec3f& v1, const vec3f& v2) {
    v1.X *= v2.X; v1.Y *= v2.Y; v1.Z *= v2.Z;
    return v1;
}
inline vec3f& operator/=(vec3f& v1, const float f1)
{
    v1.X /= f1; v1.Y /= f1; v1.Z /= f1;
    return v1;
}
inline vec3f operator/(const vec3f v1, const float f1)
{
     return {v1.X/f1, v1.Y / f1, v1.Z/  f1};
}
struct mat4x4 {
    float m[4][4] = {0};
};

inline vec3f vector_add(const vec3f& vec1, const vec3f& vec2) {
    return {vec1.X + vec2.X, vec1.Y + vec2.Y, vec1.Z + vec2.Z};
}
inline vec3f vector_sub(const vec3f& vec1, const vec3f& vec2) {
    return {vec1.X - vec2.X, vec1.Y - vec2.Y, vec1.Z - vec2.Z};
}
inline vec3f vector_mul(vec3f vec, float s) {
    return {vec.X * s, vec.Y * s, vec.Z * s};
}
inline vec3f vector_div(vec3f vec, float s) {
    return {vec.X / s, vec.Y / s, vec.Z / s};
}
inline vec3f vector_cross(vec3f vec1, vec3f vec2) {
    return {vec1.Y * vec2.Z - vec1.Z * vec2.Y, vec1.Z * vec2.X - vec1.X * vec2.Z, vec1.X * vec2.Y - vec1.Y * vec2.X};
}
inline float vector_dot(const vec3f& vec1, const vec3f& vec2) {
    return vec1.X * vec2.X + vec1.Y * vec2.Y + vec1.Z * vec2.Z;
}
inline float vector_dot(const vec4f& vec1, const vec4f& vec2) {
    return vec1.X * vec2.X + vec1.Y * vec2.Y + vec1.Z * vec2.Z + vec1.w * vec2.w;
}
inline float vector_length(const vec3f& v) {
    return sqrtf(v.X * v.X + v.Y * v.Y + v.Z * v.Z);
}
inline vec3f vector_normalize(const vec3f& vec1) {
    float l = vector_length(vec1);
    return {vec1.X / l, vec1.Y / l, vec1.Z / l};
}
inline vec3f vectorMatMul(const vec3f& v, const mat4x4& m) {
    vec3f o;
    o.X = v.X * m.m[0][0] + v.Y * m.m[1][0] + v.Z * m.m[2][0] + m.m[3][0];
    o.Y = v.X * m.m[0][1] + v.Y * m.m[1][1] + v.Z * m.m[2][1] + m.m[3][1];
    o.Z = v.X * m.m[0][2] + v.Y * m.m[1][2] + v.Z * m.m[2][2] + m.m[3][2];
    return o;
}
inline mat4x4 matrix_makeIdentitY() {
    mat4x4 mat;
    mat.m[0][0] = mat.m[1][1] = mat.m[2][2] = mat.m[3][3] = 1.0f;
    return mat;
}
inline mat4x4 matrix_makeTranslation(float X, float Y, float Z) {
    mat4x4 mat = matrix_makeIdentitY();
    mat.m[3][0] = X; mat.m[3][1] = Y; mat.m[3][2] = Z;
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
inline mat4x4 matrix_makeScale(float x, float y, float z)
{
    mat4x4 mat;
    mat.m[0][0] = x;
    mat.m[1][1] = y;
    mat.m[2][2] = z;
    mat.m[3][3] = 1;
    return mat;
}
inline mat4x4 matrix_makeProjection(float fovDeg, float aspect, float ZNear, float ZFar) {
    float f = 1.0f / tanf(fovDeg * 0.5f * 3.1415f / 180.0f);
    mat4x4 m;
    m.m[0][0] = f / aspect; m.m[1][1] = f;
    m.m[2][2] = (ZFar + ZNear) / (ZNear - ZFar); m.m[2][3] = -1.0f;
    m.m[3][2] = (2 * ZFar * ZNear) / (ZNear - ZFar); m.m[3][3] = 0.0f;
    return m;
}

inline mat4x4 matrix_ortho(float left, float right, float bottom, float top, float zNear, float zFar) {
    mat4x4 result;

    // Diagonal scaling terms
    result.m[0][0]  = 2.0f / (right - left);
    result.m[1][1]  = 2.0f / (top - bottom);
    result.m[2][2] = 2.0f / (zFar - zNear);
    result.m[3][3] = 1.0f;

    // Translation terms (column 3)
    result.m[3][0] = -(right + left) / (right - left);
    result.m[3][1] = -(top + bottom) / (top - bottom);
    result.m[3][2] = -(zFar + zNear) / (zFar - zNear);

    return result;
}
inline mat4x4 matrix_pointAt(const vec3f& pos, const vec3f& target, const vec3f& up) {
    vec3f forward = vector_sub(target, pos);
    forward = vector_normalize(forward);

    // Calculate new Up direction
    vec3f a = vector_mul(forward, vector_dot(up, forward));
    vec3f newUp = vector_sub(up, a);
    newUp = vector_normalize(newUp);

    // New Right direction is easY, its just cross product
    vec3f right = vector_cross(newUp, forward);
    mat4x4 matrix;
    matrix.m[0][0] = right.X;
    matrix.m[1][0] = newUp.X;
    matrix.m[2][0] = forward.X;
    matrix.m[3][0] = pos.X;

    matrix.m[0][1] = right.Y;
    matrix.m[1][1] = newUp.Y;
    matrix.m[2][1] = forward.Y;
    matrix.m[3][1] = pos.Y;

    matrix.m[0][2] = right.Z;
    matrix.m[1][2] = newUp.Z;
    matrix.m[2][2] = forward.Z;
    matrix.m[3][2] = pos.Z;

    matrix.m[0][3] = 0.0f;
    matrix.m[1][3] = 0.0f;
    matrix.m[2][3] = 0.0f;
    matrix.m[3][3] = 1.0f;


    return matrix;
}
inline mat4x4 matrix_lookAtRH(const vec3f& eye, const vec3f& center, const vec3f& up) {
    vec3f f = vector_normalize(vector_sub(center, eye));
    vec3f s = vector_normalize(vector_cross(f, up));
    vec3f u = vector_cross(s, f);

    mat4x4 m;
    m.m[0][0]= s.X; m.m[0][1]= u.X; m.m[0][2]=-f.X; m.m[0][3]=0.0f;
    m.m[1][0]= s.Y; m.m[1][1]= u.Y; m.m[1][2]=-f.Y; m.m[1][3]=0.0f;
    m.m[2][0]= s.Z; m.m[2][1]= u.Z; m.m[2][2]=-f.Z; m.m[2][3]=0.0f;
    m.m[3][0]=-vector_dot(s,eye);
    m.m[3][1]=-vector_dot(u,eye);
    m.m[3][2]= vector_dot(f,eye);
    m.m[3][3]=1.0f;
    return m;
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