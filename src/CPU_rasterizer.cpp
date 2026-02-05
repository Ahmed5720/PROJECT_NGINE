#ifdef tezy
#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <cmath>
#include <fstream>
#include <strstream>
#include "miniVM.h"
using namespace std; 


const int Height = 1200;
const int Width = 1200;
const float FOV = 90.0f;
const float PI = 3.1415;
const float Zfar = 1000.0f;
const float Znear = 0.1;

// Vertex shader source code
const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in float aLightIntensity;
    uniform mat4 projection;
    
    out float vLightIntensity;
    
    void main()
    {
        gl_Position =  vec4(aPos, 1.0);
        vLightIntensity = aLightIntensity;
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core
    in float vLightIntensity;
    out vec4 FragColor;
    
    void main()
    {
        vec3 baseColor = vec3(1.0, 1.0, 1.0);
        
        vec3 finalColor = baseColor * max(vLightIntensity, 0.1);
        
        FragColor = vec4(finalColor, 1.0);
    }
)";


unsigned int shaderProgram;
unsigned int VAO, VBO;

unsigned int compileShader(GLenum type, const char* source)
{
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    // Check for compilation errors
    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        cout << "ERROR::SHADER::" << (type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT") 
             << "::COMPILATION_FAILED\n" << infoLog << endl;
    }
    return shader;
}

// Function to set up shaders and buffers
void setupOpenGL()
{
    // Compile shaders
    unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    
    // Create shader program
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    
    // Check for linking errors
    int success;
    char infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << endl;
    }
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    // Generate and bind VAO and VBO
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    
    // We'll use separate buffers for position and lighting intensity
    // For now, just configure the VAO for interleaved attributes
    
    // Configure vertex attributes
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Configure lighting intensity attribute (stride is 4 floats: 3 for position, 1 for intensity)
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

struct triangle
{   
    vec3f p[3]; 
    triangle()
    {
        p[0];
        p[1];
        p[2];
    }
    triangle(vec3f a, vec3f b, vec3f c)
    {
        p[0] = a;
        p[1] = b;
        p[2] = c;
    }
    triangle(float x1, float y1, float z1,
             float x2, float y2, float z2,
             float x3, float y3, float z3)
    {
        p[0] = vec3f(x1,y1,z1);
        p[1] = vec3f(x2,y2,z2);
        p[2] = vec3f(x3,y3,z3);
    }
};

struct mesh
{
	vector<triangle> tris;

	bool LoadFromObjectFile(string sFilename)
	{
		ifstream f(sFilename);
		if (!f.is_open())
			return false;

		// Local cache of verts
		vector<vec3f> verts;

		while (!f.eof())
		{
			char line[128];
			f.getline(line, 128);

			strstream s;
			s << line;

			char junk;

			if (line[0] == 'v')
			{
				vec3f v;
				s >> junk >> v.x >> v.y >> v.z;
				verts.push_back(v);
			}

			if (line[0] == 'f')
			{
				int f[3];
				s >> junk >> f[0] >> f[1] >> f[2];
				tris.push_back({ verts[f[0] - 1], verts[f[1] - 1], verts[f[2] - 1] });
			}
		}

		return true;
	}

    void recenterMesh(mesh& m)
    {
        if (m.tris.empty()) return;

        vec3f minv( 1e9f,  1e9f,  1e9f);
        vec3f maxv(-1e9f, -1e9f, -1e9f);

        for (const auto& t : m.tris)
        {
            for (int i = 0; i < 3; i++)
            {
                const vec3f& v = t.p[i];
                minv.x = std::min(minv.x, v.x);
                minv.y = std::min(minv.y, v.y);
                minv.z = std::min(minv.z, v.z);
                maxv.x = std::max(maxv.x, v.x);
                maxv.y = std::max(maxv.y, v.y);
                maxv.z = std::max(maxv.z, v.z);
            }
        }

        vec3f center(
            (minv.x + maxv.x) * 0.5f,
            (minv.y + maxv.y) * 0.5f,
            (minv.z + maxv.z) * 0.5f
        );

        for (auto& t : m.tris)
            for (int i = 0; i < 3; i++)
                t.p[i] = vector_sub(t.p[i], center);
    }

};



void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}


void initialize(mesh& mesh, mat4x4& projMat)
{
    
    bool loaded = mesh.LoadFromObjectFile("model.obj");
    cout << "OBJ load: " << loaded << " tris=" << mesh.tris.size() << "\n";
    mesh.recenterMesh(mesh);
    const float aspect = (float)Width/(float)Height;
    projMat = matrix_makeProjection(FOV, aspect, Znear, Zfar);

}
float fTheta = 0.0f;
vec3f vCamera = {0,0,0};
float fYaw = 0;
vec3f up = {0,1,0};
vec3f lookDir = {0,0,1};
vec3f LookatTarget = {0,0,1};
mat4x4 CameraMatrix;
mat4x4 ViewMatrix;
    // Illumination
vec3f light_direction = { 0.0f, 0.0f, 1.0f };
void renderloop(GLFWwindow* window, mesh& mesh, mat4x4& projMat)
{   
    //player input

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        vCamera.y += 0.1f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        vCamera.y -= 0.1f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        vCamera.x += 0.1f;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        vCamera.x -= 0.1f;

    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        fYaw -= 2.0f * (PI / 180.0f);
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        fYaw += 2.0f * (PI / 180.0f);
    vec3f camForwardV = vector_mul(lookDir, 0.01f);
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        vCamera = vector_add(vCamera, camForwardV);
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        vCamera = vector_sub(vCamera, camForwardV);
    
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(shaderProgram);


    float glMat[16] = {
        projMat.m[0][0], projMat.m[0][1], projMat.m[0][2], projMat.m[0][3],
        projMat.m[1][0], projMat.m[1][1], projMat.m[1][2], projMat.m[1][3],
        projMat.m[2][0], projMat.m[2][1], projMat.m[2][2], projMat.m[2][3],
        projMat.m[3][0], projMat.m[3][1], projMat.m[3][2], projMat.m[3][3]
    };

    int projLoc = glGetUniformLocation(shaderProgram, "projection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glMat);
    fTheta += 0.1; 
    mat4x4 modelMat, rotXMat, transMat;


    rotXMat = matrix_makeRotationY(fTheta * 0.5f);
    transMat = matrix_makeTranslation(0.0, 0, -1.0f); 
    
    modelMat = matrix_makeIdentity();
    // order is Scale -> Rot -> Trans (SRT)
    modelMat = matrix_matmul(rotXMat, transMat);

    light_direction = vector_normalize(light_direction);
    mat4x4 matCamRot = matrix_makeRotationY(fYaw);
    vec3f LookatTarget = {0,0,1};
    lookDir = vectorMatMul(LookatTarget, matCamRot);
    lookDir = vector_normalize(lookDir);
    LookatTarget = vector_add(vCamera, lookDir);

    CameraMatrix = matrix_pointAt(vCamera, LookatTarget, up);
    ViewMatrix = matrix_quickInvert(CameraMatrix);
    for (triangle tri : mesh.tris)
    {
        triangle projectedTri, transformedTri, viewedTri;
        //transform
        transformedTri.p[0] = vectorMatMul(tri.p[0], modelMat); 
        transformedTri.p[1] = vectorMatMul(tri.p[1], modelMat); 
        transformedTri.p[2] = vectorMatMul(tri.p[2], modelMat); 

        //then apply view transformation
        viewedTri.p[0] = vectorMatMul(transformedTri.p[0], ViewMatrix);
        viewedTri.p[1] = vectorMatMul(transformedTri.p[1], ViewMatrix);
        viewedTri.p[2] = vectorMatMul(transformedTri.p[2], ViewMatrix);

        // Then project

        // Use Cross-Product to get surface normal
        vec3f normal, line1, line2;

        line1 = vector_sub(viewedTri.p[1], viewedTri.p[0]);
        line2 = vector_sub(viewedTri.p[2], viewedTri.p[0]);
        normal = vector_cross(line1, line2);

        // It's normally normal to normalise the normal
        normal = vector_normalize(normal);
        //if (normal.z < 0)
        //float normal_ddp = vector_dot(normal, d);
        vec3f camRay = vector_sub(transformedTri.p[0], vCamera);
        if(vector_dot(normal, camRay) < 0.0f)
        {

            // How similar is normal to light direction
			float dp = vector_dot(normal, light_direction);
			//color

            // cull backfaces first
            // to do this we need to get the surface normals of each tri
            // normal can be computed as the cross product of two line segments in a tri
            // then we project only if normal dot view_dir > 0
            projectedTri.p[0] = vectorMatMul(viewedTri.p[0], projMat); 
            projectedTri.p[1] = vectorMatMul(viewedTri.p[1], projMat); 
            projectedTri.p[2] = vectorMatMul(viewedTri.p[2], projMat); 

            projectedTri.p[0] = vector_div(projectedTri.p[0], projectedTri.p[0].w);
            projectedTri.p[1] = vector_div(projectedTri.p[1], projectedTri.p[1].w);
            projectedTri.p[2] = vector_div(projectedTri.p[2], projectedTri.p[2].w);
            // draw
            float vertices[] = {
                projectedTri.p[0].x, projectedTri.p[0].y, projectedTri.p[0].z, dp,
                projectedTri.p[1].x, projectedTri.p[1].y, projectedTri.p[1].z, dp,
                projectedTri.p[2].x, projectedTri.p[2].y, projectedTri.p[2].z, dp
            };
            
            
            

            // Bind VAO and update vertex data
            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
            
            // Draw the triangle with fill mode (not wireframe)
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glDrawArrays(GL_TRIANGLES, 0, 3);

        }
    }
}
int main()
{
    if (!glfwInit())
    {
        cout << "Failed to initialize GLFW" << endl;
        return -1;
    }

    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window;
    window = glfwCreateWindow(Width, Height, "Renderer", NULL, NULL);
    if (window == NULL)
    {
        cout << "Failed to open GLFW window" << endl;
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        cout << "Failed to initialize GLAD" << endl;
        return -1;
    }

    glViewport(0, 0, Width, Height);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    setupOpenGL();
    mesh mesh;
    mat4x4 projMat;
    initialize(mesh, projMat);
    glEnable(GL_DEPTH_TEST);

    while(!glfwWindowShouldClose(window))
    {
        
        renderloop(window, mesh, projMat);
        glfwSwapBuffers(window);

        glfwPollEvents();    
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}

#endif