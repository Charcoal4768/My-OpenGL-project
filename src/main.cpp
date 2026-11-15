#include "demo_scenes.h"

#include <GLFW/glfw3.h>
#include <iostream>
#include <string>

namespace {

std::array<float, 2> resolution{1200.0f, 800.0f};

void framebuffer_size_callback(GLFWwindow *, int width, int height) {
    glViewport(0, 0, width, height);
    resolution = {static_cast<float>(width), static_cast<float>(height)};
}

void PrintUsage(const char *program) {
    std::cout << "Usage: " << program << " [--scene debug|minimal]\n"
              << "  debug     Run the original engine test scene\n"
              << "  minimal   Run the minimal engine demo (default)\n";
}

} // namespace

int main(int argc, char **argv) {
    std::string sceneName = "minimal";
    if (argc == 3 && std::string(argv[1]) == "--scene") {
        sceneName = argv[2];
    } else if (argc == 2 && std::string(argv[1]) == "--help") {
        PrintUsage(argv[0]);
        return 0;
    } else if (argc != 1) {
        PrintUsage(argv[0]);
        return 1;
    }

    if (sceneName != "debug" && sceneName != "minimal") {
        PrintUsage(argv[0]);
        return 1;
    }
    if (sceneName == "debug") {
        resolution = {800.0f, 800.0f};
    }

    if (glfwInit() != GLFW_TRUE) {
        std::cerr << "Failed to initialize GLFW.\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    const std::string title = "UI Engine - " + sceneName;
    GLFWwindow *window = glfwCreateWindow(static_cast<int>(resolution[0]),
                                          static_cast<int>(resolution[1]),
                                          title.c_str(), nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create GLFW window.\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "Failed to initialize GLAD.\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    std::cout << "GPU Vendor: " << glGetString(GL_VENDOR) << '\n'
              << "GPU Renderer: " << glGetString(GL_RENDERER) << '\n'
              << "OpenGL Version: " << glGetString(GL_VERSION) << '\n';

    glViewport(0, 0, static_cast<int>(resolution[0]),
               static_cast<int>(resolution[1]));

    UIScene uiScene;
    uiScene.Init();
    if (sceneName == "debug") {
        BuildDebugScene(uiScene, resolution);
    } else {
        BuildMinimalScene(uiScene, resolution);
    }

    double lastTime = glfwGetTime();
    int frameCount = 0;
    while (!glfwWindowShouldClose(window)) {
        const double currentTime = glfwGetTime();
        ++frameCount;
        if (currentTime - lastTime >= 1.0) {
            const std::string fpsTitle =
                title + " - " + std::to_string(frameCount) + " FPS";
            glfwSetWindowTitle(window, fpsTitle.c_str());
            frameCount = 0;
            lastTime = currentTime;
        }

        glfwPollEvents();
        glClearColor(0.018f, 0.024f, 0.038f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        uiScene.StepFrame(resolution);
        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
