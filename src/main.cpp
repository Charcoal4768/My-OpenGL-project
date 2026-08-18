#include <GLFW/glfw3.h>
#include <UI/UIElements.h>
#include <cmath>
#include <glad/glad.h>
#include <iostream>
#include <stdio.h>

// this file is used purely as a showcase for the framework
// this is not part of the UI framework

std::array<float, 2> resolution{800.0f, 800.0f};

const char *TITLE = "Index buffer testing";

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    glViewport(0, 0, width, height);
    resolution = {static_cast<float>(width), static_cast<float>(height)};
}

int main() {

    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window =
        glfwCreateWindow(resolution[0], resolution[1], TITLE, NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to make GLFW window." << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glViewport(0, 0, resolution[0], resolution[1]);

    UIManager uIManager;

    auto rect2 = uIManager.AddElement<UIRect>();
    auto rect3 = uIManager.AddElement<UIRect>();
    auto rect4 = uIManager.AddElement<UIRect>();
    auto root = uIManager.AddElement<UIRect>();
    auto container2 = uIManager.AddElement<VerticalContainer>();
    auto container = uIManager.AddElement<VerticalContainer>();
    auto rect1 = uIManager.AddElement<UIRect>();

    uIManager.SetRoot(root);
    uIManager.AddChild(root, container);
    uIManager.AddChild(root, container2);
    uIManager.AddChild(container2, rect1);
    uIManager.AddChild(container, rect2);
    uIManager.AddChild(container2, rect3);
    uIManager.AddChild(container, rect4);

    uIManager.EditElement(
        root,
        {0.0f, 0.0f, resolution[0], resolution[1], 0.2f, 0.2f, 0.6f, 1.0f},
        true);
    uIManager.EditElement(
        container, {200.0f, 20.0f, 30.0f, 30.0f, 0.5f, 0.1f, 0.5f, 1.0f}, true);
    uIManager.EditElement(container2,
                          {180.0f, 40.0f, 10.0f, 20.0f, 1.0f, 0.5f, 0.1f, 1.0f},
                          true);
    uIManager.EditElement(
        rect1, {0.0f, 0.0f, 60.0f, 40.0f, 0.3f, 1.0f, 0.4f, 1.0f}, true);
    uIManager.EditElement(
        rect2, {0.0f, 0.0f, 120.0f, 80.0f, 0.2f, 0.8f, 0.2f, 1.0f}, true);
    uIManager.EditElement(
        rect3, {0.0f, 0.0f, 40.0f, 60.0f, 0.2f, 0.4f, 1.0f, 1.0f}, true);
    uIManager.EditElement(
        rect4, {0.0f, 0.0f, 90.0f, 90.0f, 1.0f, 0.8f, 0.2f, 1.0f}, true);

    uIManager.SetRoot(root);

    auto &c = uIManager.Get<VerticalContainer>(container);
    auto &c2 = uIManager.Get<VerticalContainer>(container2);
    c.padding = 15.00f;
    c.centerHorizontally = true;
    c.fitContentHeight = true;
    c.fitContentWidth = true;
    c.resizeChildren = true;
    c.clipChildren = true;
    c2.padding = 5.50f;
    c2.centerHorizontally = true;
    c2.fitContentHeight = true;
    c2.fitContentWidth = true;
    c2.resizeChildren = false;
    c2.clipChildren = true;

    double lastTime = glfwGetTime();
    int frameCount = 0;

    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        frameCount++;
        if (currentTime - lastTime >= 1.0) {
            std::string title =
                "UI Engine - " + std::to_string(frameCount) + " FPS";
            glfwSetWindowTitle(window, title.c_str());
            frameCount = 0;
            lastTime = currentTime;
        }

        glfwPollEvents();
        glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        uIManager.StepFrame(resolution);
        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
