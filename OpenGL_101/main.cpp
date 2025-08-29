// ImGui includes
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


#include <random>



#include "Shader.h"
#include "camera.h"
#include "particle.h"

#include <vector>
#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// camera
Camera camera(glm::vec3(0.0f, 15.0f, 20.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;	// time between current frame and last frame
float lastFrame = 0.0f;

int num_particles = 400;
float timestep_ = 1.0f;
float particle_size = 0.1f;
float gravity = 9.8f;
float minDense = 1, maxDense = 5;
glm::vec3 bounding_box = glm::vec3(5, 5, 5);



bool show_settings = true;
bool vsync_enabled = true;
bool mouse_captured = true;

std::vector<float> particlePositions_x;
std::vector<float> particlePositions_y;
std::vector<float> particlePositions_z;



void gen_particle_positions(particle& particles)
{
    
    for (unsigned int i = 0; i < num_particles; i++)
    {

        std::random_device rd;
        std::mt19937 gen(rd()); // Mersenne Twister engine seeded

        // Define ranges for each float
        std::uniform_real_distribution<float> dist_x(-bounding_box.x / 2, bounding_box.x / 2);
        std::uniform_real_distribution<float> dist_y(-bounding_box.y / 2, bounding_box.y / 2);
        std::uniform_real_distribution<float> dist_z(-bounding_box.z / 2, bounding_box.z / 2);

        // Generate random values
        float x = dist_x(gen);
        float y = dist_y(gen);
        float z = dist_z(gen);

        particles.positions.push_back(glm::vec3(x, y, z));
        particles.velocities.push_back(glm::vec3(0));
        particles.densities.push_back(0);

    }
}
int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

   // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);


    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "scene", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);


   
  

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");



    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);

    // build and compile our shader zprogram
    // ------------------------------------
    Shader ourShader("shaders/shader.vs", "shaders/shader.fs");

    // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------
    float vertices[] = {
        // positions         // colors
        -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,

        -0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f,

        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,

         0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f,

        -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f,

        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 1.0f
    };
    // world space positions of our cubes
    glm::vec3 cubePositions[] = {
        glm::vec3(0.0f,  0.0f,  0.0f),
        glm::vec3(2.0f,  5.0f, -15.0f),
        glm::vec3(-1.5f, -2.2f, -2.5f),
        glm::vec3(-3.8f, -2.0f, -12.3f),
        glm::vec3(2.4f, -0.4f, -3.5f),
        glm::vec3(-1.7f,  3.0f, -7.5f),
        glm::vec3(1.3f, -2.0f, -2.5f),
        glm::vec3(1.5f,  2.0f, -2.5f),
        glm::vec3(1.5f,  0.2f, -1.5f),
        glm::vec3(-1.3f,  1.0f, -1.5f)
    };
    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // texture coord attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);


    particle particles;

    gen_particle_positions(particles);
    
    ourShader.use();
  


    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // per-frame time logic
        // --------------------
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        // -----
        processInput(window);


        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();


        
        // Settings window
        if (show_settings)
        {
            ImGui::Begin("Settings", &show_settings, ImGuiWindowFlags_AlwaysAutoResize);

            ImGui::Text("Simulation Settings");
            ImGui::Separator();

            // Particle settings
            ImGui::SliderInt("Number of Particles", &num_particles, 1, 1000);
         
            ImGui::SliderFloat("Particle Size", &particle_size, 0.1, 5.0f);

            ImGui::SliderFloat("smoothing radius", &particles.smoothingRadius, 0, 20.0f);
            ImGui::SliderFloat("particle mass", &particles.mass, 0, 100.0f);
            ImGui::SliderFloat("Target Density ", &particles.targetDensity, -100, 100.0f);
            ImGui::SliderFloat("Pressure Multiplier ", &particles.pressureMultiplier, 0, 10.0f);
            ImGui::SliderFloat("damping", &particles.damping, 0, 10.0f);
            ImGui::SliderFloat("gravity", &gravity, 0, 10.0f);
            ImGui::SliderFloat("minDense", &minDense, -100, 100.0f);
            ImGui::SliderFloat("maxDense", &maxDense, -100, 100.0f);
            ImGui::SliderFloat("particle collider", &particles.min_dist, 0.1, 10.0f);

            ImGui::SliderFloat("timestep", &timestep_, 0, 10.0f);
            // Bounding box settings
            ImGui::Text("Bounding Box Size");
            ImGui::SliderFloat("Box X", &bounding_box.x, 0.1f, 50.0f);
            ImGui::SliderFloat("Box Y", &bounding_box.y, 0.1f, 50.0f);
            ImGui::SliderFloat("Box Z", &bounding_box.z, 0.1f, 50.0f);

            // Camera settings
            ImGui::Separator();
            ImGui::Text("Camera Position: %.1f, %.1f, %.1f",
                camera.Position.x, camera.Position.y, camera.Position.z);

            // Performance info
            ImGui::Separator();
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

            // V-Sync toggle
            if (ImGui::Checkbox("V-Sync", &vsync_enabled))
            {
                glfwSwapInterval(vsync_enabled ? 1 : 0);
            }

            



            // Reset camera button
            if (ImGui::Button("Reset Camera"))
            {
                camera.Position = glm::vec3(0.0f, 15.0f, 20.0f);
                camera.Yaw = -90.0f;
                camera.Pitch = 0.0f;
                camera.updateCameraVectors();
            }
            glfwSetMouseButtonCallback(window, mouse_button_callback);
            ImGui::End();
        }

        // render
        // ------
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // activate shader
        ourShader.use();

        // pass projection matrix to shader (note that in this case it could change every frame)
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        ourShader.setMat4("projection", projection);

        // camera/view transformation
        glm::mat4 view = camera.GetViewMatrix();
        ourShader.setMat4("view", view);

        // render boxes
        glBindVertexArray(VAO);
        
        //draw our bounding box
        glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
        model = glm::scale(model, bounding_box);
        //float angle = 20.0f * i + (float)glfwGetTime() * 50.0f; // Add rotation over time
        //model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
        ourShader.setMat4("model", model);
        ourShader.setVec3("color", glm::vec3(1,1,1));
        glDrawArrays(GL_LINES, 0, 36);

        glm::vec3 boxSize = bounding_box; // dimensions of bounding box
        for (int x = 0; x < (int)boxSize.x; x++) {
            for (int y = 0; y < (int)boxSize.y; y++) {
                for (int z = 0; z < (int)boxSize.z; z++) {

                    glm::mat4 model = glm::mat4(1.0f);

                    // translate cube to grid position
                    model = glm::translate(model, glm::vec3(x - boxSize.x/2, y - boxSize.y/2, z - boxSize.z/2 ));

                    // scale to unit cube (side length = 1)
                    model = glm::scale(model, glm::vec3(1.0f));

                    // set uniforms
                    ourShader.setMat4("model", model);
                    ourShader.setVec3("color", glm::vec3(1, 1, 1));

                    // draw cube as wireframe
                    glDrawArrays(GL_LINES, 0, 36);
                }
            }
        }
       
        //draw our particles
        //for (unsigned int i = 0; i < num_particles; i++)
        //{

        //    // particles.densities[i] = particles.CalcDensity(particles.positions[i]);
        //   // particles.velocities[i] += glm::vec3(0, -gravity, 0);
        //}
        for (unsigned int i = 0; i < num_particles; i++)
        {
            
            particles.densities[i] = particles.CalcDensity(particles.positions[i]);
            //std::cout << particles.densities[i] << "\n";
            particles.velocities[i] += glm::vec3(0, -gravity * timestep_,0);
        }
        for (unsigned int i = 0; i < num_particles; i++)
        {   
            
            // calculate the model matrix for each object and pass it to shader before drawing
            glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first


            //std::random_device rd;
            //std::mt19937 gen(rd()); // Mersenne Twister engine seeded

            //// Define ranges for each float
            //std::uniform_real_distribution<float> dist_x(-1, 1);
            //std::uniform_real_distribution<float> dist_y(-1, 1);
            //std::uniform_real_distribution<float> dist_z(-1, 1);

            //// Generate random values
            //float x = dist_x(gen);
            //float y = dist_y(gen);
            //float z = dist_z(gen);

            //glm::vec3 randDir(x, y, z);

   
            //float magnitude = sqrt(particles.mass * density);
            //particles.velocities[i] += (magnitude * randDir);

            glm::vec3 pressureForce = particles.CalcPressureForce(particles.positions[i]);
            glm::vec3 pressureAcceleration = pressureForce / particles.densities[i];
            particles.velocities[i] += pressureAcceleration;

            //std::cout << "vs" <<(particles.velocities[i].x) << "\n";
            particles.positions[i] += particles.velocities[i] * 0.010f * timestep_;
            particles.check_collision(-bounding_box.x/2, bounding_box.x / 2, -bounding_box.y / 2, bounding_box.y / 2, -bounding_box.z / 2, bounding_box.z / 2, i);
            particles.check_particle_collision(i);
            //particle must recieve force that is in the opposite direction its going
            //direction is just normalized velocity so take that and multiply it by force magnitude. force is the density at that point for now
            // new velocity = f = ma then sqrt (f/m)?
            // position just += velocity * timestep?
                // since d/t = v therefore d = vt
            //and then update positions directly with that.
            // but at first velocity is 
            
            model = glm::translate(model, particles.positions[i]);
            model = glm::scale(model, glm::vec3(particle_size, particle_size, particle_size));
            
            glm::vec3 heat = particles.heatmap(minDense, maxDense, particles.densities[i]);
            //float angle = 20.0f * i + (float)glfwGetTime() * 50.0f; // Add rotation over time
            //model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
            ourShader.setMat4("model", model);
            ourShader.setVec3("color", heat);
            particles.renderSphere();
            //glDrawArrays(GL_TRIANGLES, 0, 36);

          
        }

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    
   

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}



// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    // Let ImGui handle it first
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);

    // If ImGui is using the mouse, don't process further
    if (ImGui::GetIO().WantCaptureMouse) return;

    // Your custom mouse handling
    if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (action == GLFW_PRESS)
        {
            mouse_captured = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        else if (action == GLFW_RELEASE)
        {
            mouse_captured = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
}

void processInput(GLFWwindow* window)
{
    ImGuiIO& io = ImGui::GetIO();

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    static bool tab_pressed = false;

    if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS && !io.WantCaptureKeyboard)
    {
        if (!tab_pressed)
        {
            show_settings = !show_settings;
            tab_pressed = true;
        }
    }
    else if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_RELEASE)
    {
        tab_pressed = false;
    }

    // Only process camera movement if mouse is captured AND ImGui doesn't want keyboard
    if (mouse_captured && !io.WantCaptureKeyboard)
    {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camera.ProcessKeyboard(FORWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camera.ProcessKeyboard(BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camera.ProcessKeyboard(LEFT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camera.ProcessKeyboard(RIGHT, deltaTime);
    }


}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}


// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return; 


    if (mouse_captured)
    {
        float xpos = static_cast<float>(xposIn);
        float ypos = static_cast<float>(yposIn);

        if (firstMouse)
        {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }

        float xoffset = xpos - lastX;
        float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

        lastX = xpos;
        lastY = ypos;

        camera.ProcessMouseMovement(xoffset, yoffset);

    }
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{   
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;

    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}