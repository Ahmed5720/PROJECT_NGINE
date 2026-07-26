#include "EditorUI.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "particleSimulation.h"
#include <cmath>
#include <string>

namespace {

constexpr ImGuiWindowFlags kPanelFlags =
    ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoSavedSettings;

void beginFixedPanel(const char* title, ImVec2 pos, ImVec2 size) {
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::Begin(title, nullptr, kPanelFlags);
}

}  // namespace

void EditorUI::applyDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* c = style.Colors;

    ImGui::StyleColorsDark();

    const ImVec4 bg0(0.06f, 0.06f, 0.06f, 1.f);
    const ImVec4 bg1(0.10f, 0.10f, 0.10f, 1.f);
    const ImVec4 bg2(0.14f, 0.14f, 0.14f, 1.f);
    const ImVec4 accent(0.55f, 0.55f, 0.55f, 1.f);
    const ImVec4 text(0.90f, 0.90f, 0.90f, 1.f);
    const ImVec4 textDim(0.55f, 0.55f, 0.55f, 1.f);

    c[ImGuiCol_Text]                  = text;
    c[ImGuiCol_TextDisabled]          = textDim;
    c[ImGuiCol_WindowBg]              = bg0;
    c[ImGuiCol_ChildBg]               = bg0;
    c[ImGuiCol_PopupBg]               = bg1;
    c[ImGuiCol_Border]                = ImVec4(0.22f, 0.22f, 0.22f, 1.f);
    c[ImGuiCol_FrameBg]               = bg1;
    c[ImGuiCol_FrameBgHovered]        = bg2;
    c[ImGuiCol_FrameBgActive]         = ImVec4(0.20f, 0.20f, 0.20f, 1.f);
    c[ImGuiCol_TitleBg]               = bg0;
    c[ImGuiCol_TitleBgActive]         = bg1;
    c[ImGuiCol_TitleBgCollapsed]      = bg0;
    c[ImGuiCol_MenuBarBg]             = bg0;
    c[ImGuiCol_ScrollbarBg]           = bg0;
    c[ImGuiCol_ScrollbarGrab]         = bg2;
    c[ImGuiCol_ScrollbarGrabHovered]  = accent;
    c[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.70f, 0.70f, 0.70f, 1.f);
    c[ImGuiCol_CheckMark]             = ImVec4(0.85f, 0.85f, 0.85f, 1.f);
    c[ImGuiCol_SliderGrab]            = accent;
    c[ImGuiCol_SliderGrabActive]      = ImVec4(0.75f, 0.75f, 0.75f, 1.f);
    c[ImGuiCol_Button]                = bg2;
    c[ImGuiCol_ButtonHovered]         = ImVec4(0.28f, 0.28f, 0.28f, 1.f);
    c[ImGuiCol_ButtonActive]          = ImVec4(0.35f, 0.35f, 0.35f, 1.f);
    c[ImGuiCol_Header]                = bg2;
    c[ImGuiCol_HeaderHovered]         = ImVec4(0.28f, 0.28f, 0.28f, 1.f);
    c[ImGuiCol_HeaderActive]          = ImVec4(0.35f, 0.35f, 0.35f, 1.f);
    c[ImGuiCol_Separator]             = ImVec4(0.25f, 0.25f, 0.25f, 1.f);
    c[ImGuiCol_Tab]                   = bg1;
    c[ImGuiCol_TabHovered]            = bg2;
    c[ImGuiCol_TabActive]             = ImVec4(0.22f, 0.22f, 0.22f, 1.f);
    style.WindowRounding    = 0.f;
    style.ChildRounding     = 0.f;
    style.FrameRounding     = 2.f;
    style.GrabRounding      = 2.f;
    style.WindowBorderSize  = 1.f;
    style.FrameBorderSize   = 0.f;
    style.WindowPadding     = ImVec2(8.f, 8.f);
    style.ItemSpacing       = ImVec2(8.f, 6.f);
}

void EditorUI::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    shootRequested_ = false;
}

bool EditorUI::takeShootRequest() {
    const bool requested = shootRequested_;
    shootRequested_ = false;
    return requested;
}

EditorUI::Layout EditorUI::computeLayout(int framebufferW, int framebufferH) const {
    Layout L;
    L.sceneX = static_cast<int>(L.leftW);
    L.sceneY = 0;
    L.sceneW = framebufferW - static_cast<int>(L.leftW + L.rightW);
    L.sceneH = framebufferH - static_cast<int>(L.bottomH);
    if (L.sceneW < 0) L.sceneW = 0;
    if (L.sceneH < 0) L.sceneH = 0;
    return L;
}

void EditorUI::draw(Scene& scene, unsigned int viewportColorTex, const Layout& layout, bool* drawBoundingBox) {
    drawSceneGraphPanel(scene, layout);
    drawPropertiesPanel(scene, layout, drawBoundingBox);
    drawViewportPanel(viewportColorTex, layout);
    drawStatsPanel(scene, layout);
    ImGui::Render();
}

void EditorUI::drawSceneGraphPanel(Scene& scene, const Layout& layout) {
    const ImGuiViewport* mainVp = ImGui::GetMainViewport();
    const float centerH = mainVp->Size.y - layout.bottomH;

    beginFixedPanel("Scene Graph",
        ImVec2(mainVp->Pos.x, mainVp->Pos.y),
        ImVec2(layout.leftW, centerH));

    ImGui::TextDisabled("Objects");
    ImGui::Separator();

    if (scene.nodes.empty()) {
        ImGui::TextDisabled("(no nodes)");
    } else {
        for (int i = 0; i < static_cast<int>(scene.nodes.size()); ++i) {
            const SceneNode& node = scene.nodes[i];
            std::string label = node.name.empty() ? ("Node " + std::to_string(i)) : node.name;
            if (!node.visible)
                label += "  [hidden]";

            ImGui::PushID(i);
            if (ImGui::Selectable(label.c_str(), i == selectedNode_))
                selectedNode_ = i;
            ImGui::PopID();
        }
    }

    if (selectedNode_ >= 0 && selectedNode_ < static_cast<int>(scene.nodes.size())) {
        ImGui::Separator();
        SceneNode& sel = scene.nodes[selectedNode_];
        ImGui::Checkbox("Visible##sel", &sel.visible);
        ImGui::Checkbox("Cast Shadow##sel", &sel.castsShadow);
        ImGui::Checkbox("Receive Shadow##sel", &sel.receivesShadow);
    }

    ImGui::End();
}

void EditorUI::drawPropertiesPanel(Scene& scene, const Layout& layout, bool* drawBoundingBox) {
    const ImGuiViewport* mainVp = ImGui::GetMainViewport();
    const float centerH = mainVp->Size.y - layout.bottomH;

    beginFixedPanel("Properties",
        ImVec2(mainVp->Pos.x + mainVp->Size.x - layout.rightW, mainVp->Pos.y),
        ImVec2(layout.rightW, centerH));

    constexpr float pi = 3.14159265f;

    if (ImGui::CollapsingHeader("Selection", ImGuiTreeNodeFlags_DefaultOpen))
        drawSelectedNodeSection(scene);

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
        drawCameraSection(scene, pi);

    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen))
        drawLightingSection(scene, pi);

    if (ImGui::CollapsingHeader("Physics"))
        drawPhysicsSection(scene, drawBoundingBox);

    if (ImGui::CollapsingHeader("Environment")) {
        ImGui::ColorEdit3("Background", scene.backgroundColor);
    }

    ImGui::End();
}

void EditorUI::drawViewportPanel(unsigned int viewportColorTex, const Layout& layout) {
    const ImGuiViewport* mainVp = ImGui::GetMainViewport();
    const float centerH = mainVp->Size.y - layout.bottomH;

    beginFixedPanel("Viewport",
        ImVec2(mainVp->Pos.x + layout.leftW, mainVp->Pos.y),
        ImVec2(static_cast<float>(layout.sceneW), centerH));

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (viewportColorTex != 0 && avail.x >= 1.f && avail.y >= 1.f) {
        ImGui::Image((ImTextureID)(intptr_t)viewportColorTex, avail,
                     ImVec2(0.f, 1.f), ImVec2(1.f, 0.f));
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            shootRequested_ = true;
    } else {
        ImGui::Dummy(avail);
        ImGui::TextDisabled("Viewport");
    }

    ImGui::End();
}

void EditorUI::drawStatsPanel(Scene& scene, const Layout& layout) {
    const ImGuiViewport* mainVp = ImGui::GetMainViewport();
    constexpr float pi = 3.14159265f;

    beginFixedPanel("Stats",
        ImVec2(mainVp->Pos.x, mainVp->Pos.y + mainVp->Size.y - layout.bottomH),
        ImVec2(mainVp->Size.x, layout.bottomH));

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("FPS: %.1f   Frame: %.3f ms", io.Framerate, 1000.f / io.Framerate);
    ImGui::SameLine(0.f, 24.f);
    ImGui::Text("Nodes: %d   Meshes: %d", static_cast<int>(scene.nodes.size()),
                static_cast<int>(scene.meshes.size()));
    ImGui::SameLine(0.f, 24.f);
    ImGui::Text("Camera (%.1f, %.1f, %.1f)  Yaw %.1f deg",
                scene.camera.position.x, scene.camera.position.y, scene.camera.position.z,
                scene.camera.yaw * 180.f / pi);
    ImGui::SameLine(0.f, 24.f);
    ImGui::Text("Point lights: %d   Spot lights: %d",
                scene.lights.numPointLights, scene.lights.numSpotLights);
    ImGui::SameLine(0.f, 24.f);
    ImGui::TextDisabled("WASD move | Arrows look | Up/Down forward | Click viewport to shoot");

    ImGui::End();
}

void EditorUI::drawSelectedNodeSection(Scene& scene) {
    if (selectedNode_ < 0 || selectedNode_ >= static_cast<int>(scene.nodes.size())) {
        ImGui::TextDisabled("Select a node in the Scene Graph");
        return;
    }

    SceneNode& node = scene.nodes[selectedNode_];
    ImGui::Text("%s", node.name.c_str());
    ImGui::DragFloat3("Position", node.position, 0.01f, -100.f, 100.f);
    ImGui::DragFloat3("Rotation", node.rotation, 1.f, -180.f, 180.f);
    ImGui::DragFloat3("Scale", node.scale, 0.01f, 0.01f, 100.f);
    ImGui::Separator();
    ImGui::Text("Material: %s", node.material->name.c_str());
    ImGui::ColorEdit3("Diffuse", node.material->diffuseColor);
    ImGui::DragFloat("roughness", &node.material->roughness, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("metallic", &node.material->metallic, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("alpha", &node.material->alpha, 0.01f, 0.0f, 1.0f);
    ImGui::Checkbox("isEmissive", &node.material->emissive);
    ImGui::Text("Diffuse:  %s", node.material->diffuseMap.valid() ? "loaded" : "fallback");
    ImGui::Text("Specular: %s", node.material->specularMap.valid() ? "loaded" : "fallback");

    ImGui::Separator();
    ImGui::Text("Rigid Body");
    if (node.rbIndex < 0 || node.rbIndex >= static_cast<int>(scene.rbs.size())) {
        ImGui::TextDisabled("(no rigid body)");
        return;
    }

    RigidBody& rb = scene.rbs[node.rbIndex];
    ImGui::Checkbox("Static##rb", &rb.isStatic);
    ImGui::Checkbox("Use Gravity##rb", &rb.useGravity);
}

void EditorUI::drawCameraSection(Scene& scene, float pi) {
    float cameraYawDeg = scene.camera.yaw * (180.f / pi);
    ImGui::DragFloat3("Position##cam", &scene.camera.position.x, 0.01f, -100.f, 100.f);
    if (ImGui::DragFloat("Yaw (deg)##cam", &cameraYawDeg, 1.f, -180.f, 180.f))
        scene.camera.yaw = cameraYawDeg * (pi / 180.f);
    if (ImGui::Button("Reset Camera##cam")) {
        scene.camera.position = {0.f, 0.f, 0.f};
        scene.camera.yaw = 0.f;
    }
}

void EditorUI::drawLightingSection(Scene& scene, float pi) {
    ImGui::Text("Directional (Sun)");
    ImGui::DragFloat3("Direction##sun", scene.lights.sun.direction, 0.01f, -1.f, 1.f);
    ImGui::ColorEdit3("Ambient##sun", scene.lights.sun.ambient);
    ImGui::ColorEdit3("Diffuse##sun", scene.lights.sun.diffuse);
    ImGui::ColorEdit3("Specular##sun", scene.lights.sun.specular);

    ImGui::Separator();
    ImGui::Text("Point Lights (%d / %d)", scene.lights.numPointLights, MAX_POINT_LIGHTS);
    for (int i = 0; i < scene.lights.numPointLights; ++i) {
        PointLight& pl = scene.lights.pointLights[i];
        const std::string tag = "Point " + std::to_string(i);
        if (ImGui::TreeNode(tag.c_str())) {
            ImGui::Checkbox("Enabled##pl", &pl.enabled);
            ImGui::DragFloat3("Position##pl", pl.position, 0.05f, -100.f, 100.f);
            ImGui::ColorEdit3("Ambient##pl", pl.ambient);
            ImGui::ColorEdit3("Diffuse##pl", pl.diffuse);
            ImGui::ColorEdit3("Specular##pl", pl.specular);
            ImGui::DragFloat("Constant##pl", &pl.constant, 0.001f, 0.f, 2.f, "%.4f");
            ImGui::DragFloat("Linear##pl", &pl.linear, 0.001f, 0.f, 1.f, "%.4f");
            ImGui::DragFloat("Quadratic##pl", &pl.quadratic, 0.001f, 0.f, 0.5f, "%.4f");
            ImGui::TreePop();
        }
    }
    if (scene.lights.numPointLights < MAX_POINT_LIGHTS &&
        ImGui::Button("Add Point Light##ui"))
        scene.lights.addPointLight(PointLight{});

    ImGui::Separator();
    ImGui::Text("Spot Lights (%d / %d)", scene.lights.numSpotLights, MAX_SPOT_LIGHTS);
    for (int i = 0; i < scene.lights.numSpotLights; ++i) {
        SpotLight& sl = scene.lights.spotLights[i];
        const std::string tag = "Spot " + std::to_string(i);
        if (ImGui::TreeNode(tag.c_str())) {
            ImGui::Checkbox("Enabled##sl", &sl.enabled);
            ImGui::DragFloat3("Position##sl", sl.position, 0.05f, -100.f, 100.f);
            ImGui::DragFloat3("Direction##sl", sl.direction, 0.01f, -1.f, 1.f);
            ImGui::ColorEdit3("Ambient##sl", sl.ambient);
            ImGui::ColorEdit3("Diffuse##sl", sl.diffuse);
            ImGui::ColorEdit3("Specular##sl", sl.specular);
            ImGui::DragFloat("Constant##sl", &sl.constant, 0.001f, 0.f, 2.f, "%.4f");
            ImGui::DragFloat("Linear##sl", &sl.linear, 0.001f, 0.f, 1.f, "%.4f");
            ImGui::DragFloat("Quadratic##sl", &sl.quadratic, 0.001f, 0.f, 0.5f, "%.4f");
            float innerDeg = acosf(sl.cutOff) * (180.f / pi);
            float outerDeg = acosf(sl.outerCutOff) * (180.f / pi);
            if (ImGui::DragFloat("Inner Angle##sl", &innerDeg, 0.5f, 1.f, 45.f))
                sl.cutOff = cosf(innerDeg * (pi / 180.f));
            if (ImGui::DragFloat("Outer Angle##sl", &outerDeg, 0.5f, 1.f, 60.f))
                sl.outerCutOff = cosf(outerDeg * (pi / 180.f));
            ImGui::TreePop();
        }
    }
    if (scene.lights.numSpotLights < MAX_SPOT_LIGHTS &&
        ImGui::Button("Add Spot Light##ui"))
        scene.lights.addSpotLight(SpotLight{});
}

void EditorUI::drawPhysicsSection(Scene& scene, bool* drawBoundingBox) {
    if (drawBoundingBox)
        ImGui::Checkbox("Show Bounding Boxes", drawBoundingBox);

    ImGui::Separator();

    if (selectedNode_ < 0 || selectedNode_ >= static_cast<int>(scene.nodes.size())) {
        ImGui::TextDisabled("Select a node in the Scene Graph.");
    } else {
        SceneNode& node = scene.nodes[selectedNode_];
        if (node.rbIndex < 0 || node.rbIndex >= static_cast<int>(scene.rbs.size())) {
            ImGui::TextDisabled("Selected node has no rigid body.");
        } else {
            RigidBody& rb = scene.rbs[node.rbIndex];
            ImGui::Text("Node: %s", node.name.c_str());
            ImGui::Checkbox("Static##phys", &rb.isStatic);
            ImGui::Checkbox("Use Gravity##phys", &rb.useGravity);
        }
    }
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("SPH (preview)", ImGuiTreeNodeFlags_None)) {
        ImGui::TextDisabled("Parameters reserved for future SPH integration.");
        ImGui::Text("Particle count: %d", PARTICLE_COUNT);
    }
}
