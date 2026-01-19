#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <cmath>
#include <fstream>
#include <strstream>
#include "miniVM.h"
using namespace std; 


const int Height = 800;
const int Width = 600;
const float FOV = 90;
const float PI = 3.14;
const float Zfar = 1000;
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
        gl_Position = projection * vec4(aPos, 1.0);
        vLightIntensity = aLightIntensity;
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core
    in float vLightIntensity;
    out vec4 FragColor;
    
    void main()
    {
        vec3 baseColor = vec3(1.0, 0.5, 0.2);
        
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

};



void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void initialize(mesh& mesh, mat4x4& projMat)
{
    
    

    mesh.LoadFromObjectFile("model.obj");

    const float aspect = Height/Width;
    const float f = 1 / (tanf(FOV * PI / 360.0)); // fov / 2 (to radians)
    projMat = matrix_makeProjection(f, aspect, Zfar, Znear);

}
float fTheta = 0.0f;
vec3f vCamera;
void renderloop(mesh& mesh, mat4x4& projMat)
{
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
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
    fTheta += 0.001; 
    mat4x4 modelMat, rotXMat, transMat;

    // Rotation X

    // order is Scale -> Rot -> Trans (SRT)
    rotXMat = matrix_makeRotationX(fTheta * 0.5f);
    transMat = matrix_makeTranslation(0, 0, 3.0f); 

    modelMat = matrix_makeIdentity();
    modelMat = matrix_matmul(rotXMat, transMat);

        // Illumination
    vec3f light_direction = { 0.0f, 0.0f, -1.0f };
    light_direction = vector_normalize(light_direction);
    for (triangle tri : mesh.tris)
    {
        triangle projectedTri, rotatedTri;
        // rotate
        // vectorMatMul(tri.p[0], rotatedTri.p[0], rotMat); 
        // vectorMatMul(tri.p[1], rotatedTri.p[1], rotMat); 
        // vectorMatMul(tri.p[2], rotatedTri.p[2], rotMat);
        rotatedTri.p[0] = vectorMatMul(tri.p[0], modelMat); 
        rotatedTri.p[1] = vectorMatMul(tri.p[1], modelMat); 
        rotatedTri.p[2] = vectorMatMul(tri.p[2], modelMat); 
   

        // Then project


        // Use Cross-Product to get surface normal
        vec3f normal, line1, line2;

        line1 = vector_sub(rotatedTri.p[1], rotatedTri.p[0]);
        line2 = vector_sub(rotatedTri.p[2], rotatedTri.p[0]);
        normal = vector_cross(line1, line2);

        // It's normally normal to normalise the normal
        normal = vector_normalize(normal);
        //if (normal.z < 0)
        if(normal.x * (rotatedTri.p[0].x - vCamera.x) + 
            normal.y * (rotatedTri.p[0].y - vCamera.y) +
            normal.z * (rotatedTri.p[0].z - vCamera.z) < 0.0f)
        {

            // How similar is normal to light direction
			float dp = vector_dot(normal, light_direction);
			//color

            // cull backfaces first
            // to do this we need to get the surface normals of each tri
            // normal can be computed as the cross product of two line segments in a tri
            // then we project only if normal dot view_dir > 0
            projectedTri.p[0] = vectorMatMul(rotatedTri.p[0], projMat); 
            projectedTri.p[1] = vectorMatMul(rotatedTri.p[1], projMat); 
            projectedTri.p[2] = vectorMatMul(rotatedTri.p[2], projMat); 

            projectedTri.p[0] = vector_div(projectedTri.p[0], projectedTri.p[0].w);
            projectedTri.p[1] = vector_div(projectedTri.p[1], projectedTri.p[0].w);
            projectedTri.p[2] = vector_div(projectedTri.p[2], projectedTri.p[0].w);
            // draw
            float vertices[] = {
                projectedTri.p[0].x, projectedTri.p[0].y, projectedTri.p[0].z, dp,
                projectedTri.p[1].x, projectedTri.p[1].y, projectedTri.p[1].z, dp,
                projectedTri.p[2].x, projectedTri.p[2].y, projectedTri.p[2].z, dp
            };
            
            
            
            // Draw the triangle
            //glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

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
    window = glfwCreateWindow(800, 600, "ZMMR", NULL, NULL);
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

    glViewport(0, 0, Height, Width);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    setupOpenGL();
    mesh mesh;
    mat4x4 projMat;
    initialize(mesh, projMat);
    glEnable(GL_DEPTH);

    while(!glfwWindowShouldClose(window))
    {
        
        renderloop(mesh, projMat);
        glfwSwapBuffers(window);

        glfwPollEvents();    
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}