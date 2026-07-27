#include "RenderPipeline.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include <glad/glad.h>
#include <iostream>
#include <string>
namespace {
const float PI = 3.14159265f;

// why the hell is this here and not in shader.h?
void setMat4(shader& s, const char* name, const mat4x4& M) {
    float m[16] = {
        M.m[0][0], M.m[0][1], M.m[0][2], M.m[0][3],
        M.m[1][0], M.m[1][1], M.m[1][2], M.m[1][3],
        M.m[2][0], M.m[2][1], M.m[2][2], M.m[2][3],
        M.m[3][0], M.m[3][1], M.m[3][2], M.m[3][3]
    };
    s.setMat4(name, m);
}

// Build a uniform name like "pointLights[2].diffuse" without heap allocation.
// `buf` must be large enough (64 bytes is plenty).
void fmtUniform(char* buf, size_t sz, const char* array, int idx, const char* field) {
    snprintf(buf, sz, "%s[%d].%s", array, idx, field);
}

void checkGLError(const char* ctx) {
    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
        std::cerr << "GL error in " << ctx << ": 0x" << std::hex << err << std::dec << "\n";
}


// --- TEMP DEBUG: depth texture on a quad
void debugDrawDepthTex(GLuint tex) {
    static GLuint vao = 0, vbo = 0, prog = 0;
    if (prog == 0) {
        const char* vs = R"(#version 430 core
            layout(location=0) in vec2 aPos;
            out vec2 uv;
            void main() { uv = aPos * 0.5 + 0.5; gl_Position = vec4(aPos, 0.0, 1.0); })";
        const char* fs = R"(#version 430 core
            in vec2 uv; out vec4 FragColor;
            uniform sampler2D depthTex;
            void main() {
                float d = texture(depthTex, uv).r;
                FragColor = vec4(vec3(d), 1.0);
            })";
        GLuint v = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(v, 1, &vs, nullptr); glCompileShader(v);
        GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(f, 1, &fs, nullptr); glCompileShader(f);
        prog = glCreateProgram();
        glAttachShader(prog, v); glAttachShader(prog, f); glLinkProgram(prog);
        GLint ok = 0; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        if (!ok) { char log[512]; glGetProgramInfoLog(prog, 512, nullptr, log);
                   std::cerr << "debug quad link failed: " << log << "\n"; }
        glDeleteShader(v); glDeleteShader(f);

        const float quad[] = { -1,-1,  1,-1,  -1,1,  1,1 };
        glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }
    glDisable(GL_DEPTH_TEST);
    glUseProgram(prog);
    glActiveTexture(GL_TEXTURE0);            // your lighting pass leaves this on unit 2
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(glGetUniformLocation(prog, "depthTex"), 0);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}


} // namespace

RenderPipeline::RenderPipeline(shader* pbrShader, shader* wfShader, ParticleRenderer* particleRenderer, shader* skyShader, shader* shadow)
    : pbrShader_(pbrShader), wireFrameShader_(wfShader), particleRenderer_(particleRenderer) , skyBoxShader_(skyShader) , shadowShader_(shadow) {
    wireFrameMesh_.init();
}

RenderPipeline::~RenderPipeline() {
    destroySceneFramebuffer();
}

void RenderPipeline::resizeSceneFramebuffer(int w, int h) {
    if (w < 8 || h < 8)
        return;
    if (w == sceneFbW_ && h == sceneFbH_)
        return;

    destroySceneFramebuffer();
    sceneFbW_ = w;
    sceneFbH_ = h;
    std::cout << "AA\n";
    glGenFramebuffers(1, &sceneFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo_);
    glGenTextures(1, &sceneColorTex_);
    glBindTexture(GL_TEXTURE_2D, sceneColorTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColorTex_, 0);
    std::cout << "BB\n";
    glGenRenderbuffers(1, &sceneDepthRbo_);
    glBindRenderbuffer(GL_RENDERBUFFER, sceneDepthRbo_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, sceneDepthRbo_);
    std::cout << "CC\n";
    glGenFramebuffers(1, &depthMapFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFbo_);
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    std::cout << "DD\n";
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "[RenderPipeline] Scene framebuffer incomplete: 0x" << std::hex << status << std::dec << "\n";

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    std::cout << "EE\n";
}

void RenderPipeline::destroySceneFramebuffer() {
    if (sceneDepthRbo_) {
        glDeleteRenderbuffers(1, &sceneDepthRbo_);
        sceneDepthRbo_ = 0;
    }
    if (sceneColorTex_) {
        glDeleteTextures(1, &sceneColorTex_);
        sceneColorTex_ = 0;
    }
    if (sceneFbo_) {
        glDeleteFramebuffers(1, &sceneFbo_);
        sceneFbo_ = 0;
    }
    if(depthMap)
        glDeleteTextures(1,&depthMap);

    if(depthMapFbo_)
        glDeleteFramebuffers(1,&depthMapFbo_);
    sceneFbW_ = 0;
    sceneFbH_ = 0;
}

bool RenderPipeline::takeShootRequest() {
    return editorUI_.takeShootRequest();
}

void RenderPipeline::render(Scene& scene, int framebufferW, int framebufferH,
                            float zNear, float zFar) {
    const EditorUI::Layout layout = editorUI_.computeLayout(framebufferW, framebufferH);

    if (layout.sceneW >= 8 && layout.sceneH >= 8) {
        resizeSceneFramebuffer(layout.sceneW, layout.sceneH);
        const float aspect = static_cast<float>(layout.sceneW) / static_cast<float>(layout.sceneH);

        // shadows assume single directional light source, same method would work for spotlights
        // (except that we would use a prespective projection instead of ortho, however doing this for every light seems very expensive there are probably better ways)
        
        const vec3f center = {0.0,0.0,0.0};
        const vec3f lightDir = vector_normalize(vec3f(scene.lights.sun.direction));
        const vec3f up = {0.0, 1.0, 0.0};
        const vec3f lightPos = lightDir * -10.0f; // why does that work
        const mat4x4 view = scene.camera.getViewMatrix();
        const mat4x4 projection = scene.camera.getProjectionMatrix(aspect, zNear, zFar);
        // the choice of the ortho bounds and near and far clipping planes seem to be very delicate
        // near and far should depend on where the light source is, the cube should sorround the entire scene
        const mat4x4 lightProjection = matrix_ortho(-2.0f, 2.0f, -2.0f, 2.0f, 0, 30);
        const mat4x4 lightView = matrix_quickInvert(matrix_pointAt(lightPos,  center, up));
        const mat4x4 lightSpace =  matrix_matmul(lightView, lightProjection);
        glEnable(GL_DEPTH_TEST);
        glClearColor(scene.backgroundColor[0], scene.backgroundColor[1], scene.backgroundColor[2], 1.0f);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFbo_);
        glClear(GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glCullFace(GL_FRONT);
        renderShadowPass(scene, lightView, lightProjection);
        glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo_);
        glViewport(0, 0, layout.sceneW, layout.sceneH);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderSkyBoxPass(scene, view, projection);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        renderLightingPass(scene, view, projection, lightSpace);
        glDisable(GL_BLEND);
        renderWireframePass(scene, view, projection);
       // debugDrawDepthTex(depthMap); 
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    glViewport(0, 0, framebufferW, framebufferH);
    glClearColor(0.06f, 0.06f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    editorUI_.beginFrame();
    editorUI_.draw(scene, sceneColorTex_, layout, &drawBoundingBox);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void RenderPipeline::uploadLighting(shader& s, const LightEnvironment& lights) {
    // Directional light
    const DirectionalLight& sun = lights.sun;
    s.setFloat3("dirLight.direction", sun.direction[0], sun.direction[1], sun.direction[2]);
    s.setFloat3("dirLight.ambient",   sun.ambient[0],   sun.ambient[1],   sun.ambient[2]);
    s.setFloat3("dirLight.diffuse",   sun.diffuse[0],   sun.diffuse[1],   sun.diffuse[2]);
    s.setFloat3("dirLight.specular",  sun.specular[0],  sun.specular[1],  sun.specular[2]);

    // Point lights
    // Only upload up to numPointLights; the shader uses `numPointLights` to
    // bound its loop so unset array slots are never read.
    char buf[64];
    int activePoints = 0;
    for (int i = 0; i < lights.numPointLights; ++i) {
        const PointLight& pl = lights.pointLights[i];
        if (!pl.enabled) continue;

        fmtUniform(buf, sizeof(buf), "pointLights", activePoints, "position");
        s.setFloat3(buf, pl.position[0], pl.position[1], pl.position[2]);

        fmtUniform(buf, sizeof(buf), "pointLights", activePoints, "constant");
        s.setFloat(buf, pl.constant);
        fmtUniform(buf, sizeof(buf), "pointLights", activePoints, "linear");
        s.setFloat(buf, pl.linear);
        fmtUniform(buf, sizeof(buf), "pointLights", activePoints, "quadratic");
        s.setFloat(buf, pl.quadratic);

        fmtUniform(buf, sizeof(buf), "pointLights", activePoints, "ambient");
        s.setFloat3(buf, pl.ambient[0],  pl.ambient[1],  pl.ambient[2]);
        fmtUniform(buf, sizeof(buf), "pointLights", activePoints, "diffuse");
        s.setFloat3(buf, pl.diffuse[0],  pl.diffuse[1],  pl.diffuse[2]);
        fmtUniform(buf, sizeof(buf), "pointLights", activePoints, "specular");
        s.setFloat3(buf, pl.specular[0], pl.specular[1], pl.specular[2]);

        ++activePoints;
    }
    s.setInt("numPointLights", activePoints);
    // // Spot light — shader uses a single `spotLight` uniform (first enabled only)
    // int activeSpots = 0;
    // for (int i = 0; i < lights.numSpotLights && activeSpots < 1; ++i) {
    //     const SpotLight& sl = lights.spotLights[i];
    //     if (!sl.enabled) continue;

    //     s.setFloat3("spotLight.position",  sl.position[0],  sl.position[1],  sl.position[2]);
    //     s.setFloat3("spotLight.direction", sl.direction[0], sl.direction[1], sl.direction[2]);
    //     s.setFloat("spotLight.cutOff",      sl.cutOff);
    //     s.setFloat("spotLight.outerCutOff", sl.outerCutOff);
    //     s.setFloat("spotLight.constant",   sl.constant);
    //     s.setFloat("spotLight.linear",     sl.linear);
    //     s.setFloat("spotLight.quadratic",  sl.quadratic);
    //     s.setFloat3("spotLight.ambient",  sl.ambient[0],  sl.ambient[1],  sl.ambient[2]);
    //     s.setFloat3("spotLight.diffuse",  sl.diffuse[0],  sl.diffuse[1],  sl.diffuse[2]);
    //     s.setFloat3("spotLight.specular", sl.specular[0], sl.specular[1], sl.specular[2]);

    //     activeSpots = 1;
    // }
    //s.setInt("numSpotLights", activeSpots);
}


void RenderPipeline::renderShadowPass(Scene& scene, const mat4x4& light, const mat4x4& projection)
{
    shadowShader_->use();

    int drawn = 0;
    // Per-frame: matrices
    setMat4(*shadowShader_, "light", light);
    setMat4(*shadowShader_, "projection", projection);

    GLint prog = 0; glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    std::cout << "light=" << glGetUniformLocation(prog, "light")
              << " proj=" << glGetUniformLocation(prog, "projection")
              << " model=" << glGetUniformLocation(prog, "model") << "\n";
    // Per-node draw
    for (const SceneNode& node : scene.nodes) {
        if (!node.visible) continue;
        if (!node.castsShadow) continue;
        if (node.meshIndex == -1) continue;
        if (node.meshIndex >= static_cast<int>(scene.meshes.size())) continue;

        // Model matrix
        mat4x4 model = node.modelMatrix();
        setMat4(*shadowShader_, "model", model);
        
        // might incorrectly check for textures?
        scene.meshes[node.meshIndex].draw();
        ++drawn;

    }
    std::cout << "shadow draws: " << drawn << "\n";

    checkGLError("renderShadowDepthPass");
}
// renderLightingPass
//   1. Upload per-frame uniforms: matrices, camera, all lights.
//   2. For each visible SceneNode: upload model matrix + material, draw.

//   We bind the diffuse map to unit 0, specular map to unit 1.
//   If a map is missing we bind a 1×1 white fallback (see getWhiteTex).
void RenderPipeline::renderLightingPass(Scene& scene, const mat4x4& view, const mat4x4& projection, const mat4x4& lightSpace) {
    

    pbrShader_->use();

    // Per-frame: matrices
    setMat4(*pbrShader_, "view",       view);
    setMat4(*pbrShader_, "projection", projection);
    setMat4(*pbrShader_, "lightSpace", lightSpace);
    // Per-frame: camera position
    pbrShader_->setFloat3("viewPos", scene.camera.position.x, scene.camera.position.y, scene.camera.position.z);
    // Per-frame: all lights
    uploadLighting(*pbrShader_, scene.lights);

    // Bind sampler uniforms to their fixed texture units (set once per frame)
    pbrShader_->setInt("material.diffuseMap",  0);   // GL_TEXTURE0
    pbrShader_->setInt("material.roughnessMap", 1);   // GL_TEXTURE1
    pbrShader_->setInt("material.normalMap",2);
    pbrShader_->setInt("material.metallicMap", 3);
    pbrShader_->setInt("material.aoMap", 4);
    pbrShader_->setInt("shadowMap", 5); 

    pbrShader_->setInt("environment", 6);
    
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_CUBE_MAP, scene.cubeMapTexture);

    // Per-node draw
    for (const SceneNode& node : scene.nodes) {
        if (!node.visible)                continue;
        if (node.meshIndex == -1) continue;
        if (node.meshIndex >= static_cast<int>(scene.meshes.size())) continue;

        // Model matrix
        mat4x4 model = node.modelMatrix();
        setMat4(*pbrShader_, "model", model);


        pbrShader_->setFloat("material.roughness", node.material->roughness);
        pbrShader_->setFloat("material.metallic", node.material->metallic);
        pbrShader_->setFloat("material.alpha", node.material->alpha);
        pbrShader_->setFloat3("material.diffuseColor", node.material->diffuseColor);
        pbrShader_->setBool("material.emissive", node.material->emissive);
        
        // Diffuse map
        if (node.material->diffuseMap.valid())
        {
            pbrShader_->setBool("hasDiffuseTex", GL_TRUE);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, node.material->diffuseMap.id);
            std::cout << "Found Diffuse\n";
        }
        else
        {
            pbrShader_->setBool("hasDiffuseTex", GL_FALSE);
            std::cout << "NO Diffuse!\n";
        }
        if(node.material->roughnessMap.valid())
        {
            pbrShader_->setBool("hasRoughnessTex", GL_TRUE);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, node.material->roughnessMap.id);
            std::cout << "Found Rougness\n";
        }
        else
        {
            pbrShader_->setBool("hasRoughnessTex", GL_FALSE);
            std::cout << "No Roughness\n";
        }
        if(node.material->normalMap.valid())
        {   
            pbrShader_->setBool("hasNormalTex", GL_TRUE);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, node.material->normalMap.id);
            std::cout << "Found Normal\n";
        }
        else
        {
            pbrShader_->setBool("hasNormalTex", GL_FALSE);
            std::cout << "no Normal!!\n";
        }
        if(node.material->metallicMap.valid())
        {   
            pbrShader_->setBool("hasMetallicTex", GL_TRUE);
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, node.material->metallicMap.id);
            std::cout << "Found Mettallic\n";
        }
        else
        {
            pbrShader_->setBool("hasMetallicTex", GL_FALSE);
            std::cout << "No Metallic!!\n";
        }
        if(node.material->aoMap.valid())
        {   
            pbrShader_->setBool("hasAoTex", GL_TRUE);
            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D, node.material->aoMap.id);
            std::cout << "Found ao\n";
        }
        else
        {
            pbrShader_->setBool("hasAoTex", GL_FALSE);
            std::cout << "No AO!!!\n";
        }

        scene.meshes[node.meshIndex].draw();
    }

    checkGLError("renderPBRpass");
}
void RenderPipeline::renderWireframePass(Scene& scene, const mat4x4& view, const mat4x4& projection)
{
    if (!drawBoundingBox)
        return;

    wireFrameShader_->use();
    setMat4(*wireFrameShader_, "view", view);
    setMat4(*wireFrameShader_, "projection", projection);
    // Corners are already in world space from updateWorldBounds.
    setMat4(*wireFrameShader_, "model", matrix_makeIdentitY());

    wireFrameShader_->setFloat3("Color", 0.0f, 1.0f, 0.0f);
    glDisable(GL_DEPTH_TEST);
    glLineWidth(1.5f);

    for (const SceneNode& node : scene.nodes) {
        if (!node.visible)
            continue;
        if (node.rbIndex < 0 || node.rbIndex >= static_cast<int>(scene.rbs.size()))
            continue;

        const RigidBody& rb = scene.rbs[node.rbIndex];
        wireFrameMesh_.draw(rb.worldMin, rb.worldMax);
    }
    glEnable(GL_DEPTH_TEST);
    glLineWidth(1.0f);
    checkGLError("renderLinePass");
}

void RenderPipeline::renderSkyBoxPass(Scene& scene, const mat4x4& view, const mat4x4& projection)
{   
    static GLuint skyboxVAO = 0;
    static GLuint skyboxVBO = 0;

    if(skyboxVAO == 0)
    {
        glGenVertexArrays(1, &skyboxVAO);
        glGenBuffers(1, &skyboxVBO);
        float skyboxVertices[] = {
        // positions          
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
        1.0f,  1.0f, -1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
        1.0f, -1.0f,  1.0f
    };
        glBindVertexArray(skyboxVAO);
        glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
        
        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        
        // Unbind
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    
    // Remove translation from view matrix
    mat4x4 viewNoTranslation = view;
    viewNoTranslation.m[3][0] = 0;
    viewNoTranslation.m[3][1] = 0;
    viewNoTranslation.m[3][2] = 0;

    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    skyBoxShader_->use();
    setMat4(*skyBoxShader_, "view", viewNoTranslation);
    setMat4(*skyBoxShader_, "projection", projection);
    glBindVertexArray(skyboxVAO);
    glBindTexture(GL_TEXTURE_CUBE_MAP, scene.cubeMapTexture);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
}
GLuint RenderPipeline::getWhiteTex() {
    if (whiteTex_ != 0) return whiteTex_;
    unsigned char white[4] = {255, 255, 255, 255};
    glGenTextures(1, &whiteTex_);
    glBindTexture(GL_TEXTURE_2D, whiteTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return whiteTex_;
}
