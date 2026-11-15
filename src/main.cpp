#include "RamMonitor.h"

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <array>
#include <charconv>
#include <exception>
#include <iostream>
#include <limits>
#include <string>
#include <unistd.h>

namespace {

std::array<float, 2> resolution{1200.0f, 800.0f};

void framebuffer_size_callback(GLFWwindow *, int width, int height) {
    glViewport(0, 0, width, height);
    resolution = {static_cast<float>(width), static_cast<float>(height)};
}

void scroll_callback(GLFWwindow *window, double, double yOffset) {
    auto *monitor =
        static_cast<RamMonitor *>(glfwGetWindowUserPointer(window));
    if (monitor)
        monitor->HandleScroll(yOffset);
}

void PrintUsage(const char *program) {
    std::cout << "Usage: " << program << " [--pid PROCESS_ID]\n"
              << "Monitor system memory and a process (default: this app).\n"
              << "Press T to switch between dark and light themes.\n";
}

} // namespace

int main(int argc, char **argv) {
    pid_t monitoredProcessId = getpid();
    if (argc == 2 && std::string(argv[1]) == "--help") {
        PrintUsage(argv[0]);
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--pid") {
        long long parsedProcessId = 0;
        const std::string value = argv[2];
        const auto result =
            std::from_chars(value.data(), value.data() + value.size(),
                            parsedProcessId);
        if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
            parsedProcessId <= 0 ||
            parsedProcessId > std::numeric_limits<pid_t>::max()) {
            std::cerr << "Invalid process ID: " << value << '\n';
            PrintUsage(argv[0]);
            return 2;
        }
        monitoredProcessId = static_cast<pid_t>(parsedProcessId);
    } else if (argc != 1) {
        PrintUsage(argv[0]);
        return 2;
    }

    if (glfwInit() != GLFW_TRUE) {
        std::cerr << "Failed to initialize GLFW.\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    const std::string title = "RAM Monitor - PID " +
                              std::to_string(monitoredProcessId);
    GLFWwindow *window = glfwCreateWindow(
        static_cast<int>(resolution[0]), static_cast<int>(resolution[1]),
        title.c_str(), nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create the RAM monitor window.\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "Failed to initialize GLAD.\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    glViewport(0, 0, static_cast<int>(resolution[0]),
               static_cast<int>(resolution[1]));

    int exitCode = 0;
    try {
        UIScene scene;
        scene.Init();
        RamMonitor monitor;
        monitor.Build(scene, resolution, monitoredProcessId);
        glfwSetWindowUserPointer(window, &monitor);
        glfwSetScrollCallback(window, scroll_callback);

        bool themeKeyWasDown = false;
        bool mouseButtonWasDown = false;
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            const bool mouseButtonIsDown =
                glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            if (mouseButtonIsDown && !mouseButtonWasDown) {
                double cursorX = 0.0;
                double cursorY = 0.0;
                int windowWidth = 0;
                int windowHeight = 0;
                glfwGetCursorPos(window, &cursorX, &cursorY);
                glfwGetWindowSize(window, &windowWidth, &windowHeight);
                if (windowWidth > 0 && windowHeight > 0) {
                    cursorX *= resolution[0] / windowWidth;
                    cursorY *= resolution[1] / windowHeight;
                    monitor.HandleMouseClick(cursorX, cursorY);
                }
            }
            mouseButtonWasDown = mouseButtonIsDown;

            const bool themeKeyIsDown =
                glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS;
            if (themeKeyIsDown && !themeKeyWasDown)
                monitor.ToggleTheme();
            themeKeyWasDown = themeKeyIsDown;

            monitor.Update(glfwGetTime());
            glClearColor(0.04f, 0.05f, 0.07f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            scene.StepFrame(resolution);
            glfwSwapBuffers(window);
        }
    } catch (const std::exception &error) {
        std::cerr << "RAM monitor failed: " << error.what() << '\n';
        exitCode = 1;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return exitCode;
}
