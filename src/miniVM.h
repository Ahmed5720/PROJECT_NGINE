#include <vector>
#include <cmath>
// mini vector / matrix library
struct vec3f
{
    float x,y,z,w = 1;
    vec3f()
    {
        x = 0;
        y = 0;
        z = 0;
        w = 1;
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
        w = _w; 
    }
};
vec3f& operator+=(vec3f& v1, const vec3f& v2)
{
    v1.x += v2.x;
    v1.y += v2.y;
    v1.z += v2.z;
    return v1;
}
vec3f& operator*=(vec3f& v1, const vec3f& v2)
{
    v1.x *= v2.x;
    v1.y *= v2.y;
    v1.z *= v2.z;
    return v1;
}

struct mat4x4
{
    float m[4][4] = {0};
};

vec3f vector_add(vec3f& vec1, vec3f& vec2)
{
    return {vec1.x + vec2.x, vec1.y + vec2.y, vec1.z + vec2.z};
}
vec3f vector_sub(vec3f& vec1, vec3f& vec2)
{
    return {vec1.x - vec2.x, vec1.y - vec2.y, vec1.z - vec2.z};
}

vec3f vector_mul(vec3f vec, float s)
{
    return {vec.x * s, vec.y * s,  vec.z * s};
}

vec3f vector_div(vec3f vec, float s)
{
    return {vec.x / s, vec.y / s, vec.z / s};
}
vec3f vector_cross(vec3f vec1, vec3f vec2)
{
    return {vec1.y * vec2.z - vec1.z * vec2.y
        , vec1.z * vec2.x - vec1.x * vec2.z
        , vec1.x * vec2.y - vec1.y * vec2.x};
}
float vector_dot(vec3f &vec1, vec3f &vec2)
{
    return vec1.x * vec2.x +  vec1.y * vec2.y + vec1.z * vec2.z;
}
float vector_length(vec3f &v)
{
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}
vec3f vector_normalize(vec3f vec1)
{
    float l = vector_length(vec1);
    return {vec1.x / l, vec1.y/ l, vec1.z / l};
}


vec3f vectorMatMul(vec3f& v, mat4x4& m)
{   
    vec3f o;
    o.x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0];
    o.y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1];
    o.z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2];
    o.w = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + m.m[3][3];
    return o;
}

mat4x4 matrix_makeIdentity()
{
    mat4x4 mat;
    mat.m[0][0] = 1.0f;
    mat.m[1][1] = 1.0f;
    mat.m[2][2] = 1.0f;
    mat.m[3][3] = 1.0f;
    return mat;
}
mat4x4 matrix_makeTranslation(float x, float y, float z)
{
    mat4x4 mat = matrix_makeIdentity();
    mat.m[3][0] = x;
    mat.m[3][1] = y;
    mat.m[3][2] = z;
    return mat;
}
mat4x4 matrix_makeRotationX(float angleRad)
{
    mat4x4 mat;
    mat.m[0][0] = 1;
    mat.m[1][1] = cosf(angleRad);
    mat.m[1][2] = sinf(angleRad);
    mat.m[2][1] = -sinf(angleRad);
    mat.m[2][2] = cosf(angleRad);
    mat.m[3][3] = 1;
    return mat;
}
mat4x4 matrix_makeRotationY(float angleRad)
{
    mat4x4 mat;
    mat.m[0][0] = cosf(angleRad);
    mat.m[0][2] = sinf(angleRad);
    mat.m[2][0] = -sinf(angleRad);
    mat.m[1][1] = 1.0f;
    mat.m[2][2] = cosf(angleRad);
    mat.m[3][3] = 1.0f;
    return mat;
}
mat4x4 matrix_makeRotationZ(float angleRad)
{
    mat4x4 mat;
    mat.m[0][0] = cosf(angleRad);
    mat.m[0][1] = sinf(angleRad);
    mat.m[1][0] = -sinf(angleRad);
    mat.m[1][1] = cosf(angleRad);
    mat.m[2][2] = 1.0f;
    mat.m[3][3] = 1.0f;
    return mat;
}

mat4x4 matrix_makeProjection(float fovDegrees, float aspectRatio, float Znear, float Zfar)
{
    mat4x4 projMat;
   
    const float Zq = Zfar / (Zfar-Znear);
    const float Znq = Zfar * Znear / (Zfar - Znear); 


    projMat.m[0][0] = aspectRatio * fovDegrees;
    projMat.m[1][1] = fovDegrees;
    projMat.m[2][2] = Zq;
    projMat.m[2][3] = 1;
    projMat.m[3][2] = -Znq;
    return projMat;
}

mat4x4 matrix_matmul(mat4x4 &m1, mat4x4& m2)
{   
    mat4x4 res;
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++)
            res.m[r][c] = m1.m[0][0] * m2.m[0][0] + m1.m[r][1] * m2.m[1][c] + m1.m[r][2] * m2.m[2][c] + m1.m[r][3] * m2.m[3][c];   
    return res;
}