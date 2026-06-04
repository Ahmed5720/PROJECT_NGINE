#include "Application.h"
#include "Config.h"
#include <iostream>

int main(int argc, char** argv) {
    AppArgs args = parseArgs(argc, argv);

    Config config;
    config.windowWidth  = 1920;
    config.windowHeight = 1080;
    config.fovDeg      = 90.0f;
    config.zNear       = 0.1f;
    config.zFar        = 1000.0f;
    // Base path to src folder (OBJ, textures, shaders).
    config.basePath    = "C:/Dev/PROJECT_NGINE/src";
    config.resolvePaths();

    Application app(config, args);
    if (!app.init()) {
        std::cerr << "Application init failed.\n";
        return -1;
    }
    app.run();
    return 0;
}
