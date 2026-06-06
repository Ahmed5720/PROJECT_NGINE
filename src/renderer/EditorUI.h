#pragma once
#include "Scene.h"

struct ImGuiIO;

// Fixed editor layout: scene graph (left), viewport (center), properties (right), stats (bottom).
class EditorUI {
public:
    struct Layout {
        float leftW   = 260.f;
        float rightW  = 300.f;
        float bottomH = 72.f;
        int   sceneX  = 0;
        int   sceneY  = 0;
        int   sceneW  = 0;
        int   sceneH  = 0;
    };

    static void applyDarkTheme();

    void beginFrame();
    Layout computeLayout(int framebufferW, int framebufferH) const;
    void draw(Scene& scene, unsigned int viewportColorTex, const Layout& layout, bool* drawBoundingBox);

    int selectedNodeIndex() const { return selectedNode_; }
    bool takeShootRequest();

private:
    void drawSceneGraphPanel(Scene& scene, const Layout& layout);
    void drawPropertiesPanel(Scene& scene, const Layout& layout, bool* drawBoundingBox);
    void drawViewportPanel(unsigned int viewportColorTex, const Layout& layout);
    void drawStatsPanel(Scene& scene, const Layout& layout);

    void drawLightingSection(Scene& scene, float pi);
    void drawCameraSection(Scene& scene, float pi);
    void drawPhysicsSection(Scene& scene, bool* drawBoundingBox);
    void drawSelectedNodeSection(Scene& scene);

    int selectedNode_ = -1;
    bool shootRequested_ = false;
};
