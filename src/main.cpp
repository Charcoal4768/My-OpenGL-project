// THIS FILE IS JUST A TEST/DEMO.
// THIS IS NOT PART OF THE UI ENGINE
// THIS FILE IS ONLY A DEMONSTRATION OF THE ENGINE's CAPABILITIES

#include <UI/UIElements.h> //includes glad.h

#include <GLFW/glfw3.h>
#include <cmath>
#include <iostream>
#include <stdio.h>

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

    UIScene uiScene;
    uiScene.Init();

    auto rect2 = uiScene.AddElement<UIRect>();
    auto rect3 = uiScene.AddElement<UIRect>();
    auto rect4 = uiScene.AddElement<UIRect>();
    auto root = uiScene.AddElement<UIRect>();
    auto container2 = uiScene.AddElement<VerticalContainer>();
    auto container = uiScene.AddElement<VerticalContainer>();
    auto rect1 = uiScene.AddElement<UIRect>();

    uiScene.SetRoot(root);
    uiScene.AddChild(root, container);
    uiScene.AddChild(root, container2);
    uiScene.AddChild(container2, rect1);
    uiScene.AddChild(container, rect2);
    uiScene.AddChild(container2, rect3);
    uiScene.AddChild(container, rect4);

    uiScene.EditElementShape(root, {0.0f, 0.0f, resolution[0], resolution[1]},
                             true);
    uiScene.EditElementColor(root, {0.2f, 0.2f, 0.6f, 1.0f}, true);

    uiScene.EditElementShape(container, {200.0f, 20.0f, 30.0f, 30.0f}, true);
    uiScene.EditElementColor(container, {0.5f, 0.1f, 0.5f, 1.0f}, true);

    uiScene.EditElementShape(container2, {180.0f, 40.0f, 10.0f, 20.0f}, true);
    uiScene.EditElementColor(container2, {1.0f, 0.5f, 0.1f, 1.0f}, true);

    uiScene.EditElementShape(rect1, {0.0f, 0.0f, 60.0f, 40.0f}, true);
    uiScene.EditElementColor(rect1, {0.3f, 1.0f, 0.4f, 1.0f}, true);

    uiScene.EditElementShape(rect2, {0.0f, 0.0f, 120.0f, 80.0f}, true);
    uiScene.EditElementColor(rect2, {0.2f, 0.8f, 0.2f, 1.0f}, true);

    uiScene.EditElementShape(rect3, {0.0f, 0.0f, 40.0f, 60.0f}, true);
    uiScene.EditElementColor(rect3, {0.2f, 0.4f, 1.0f, 1.0f}, true);

    uiScene.EditElementShape(rect4, {0.0f, 0.0f, 90.0f, 90.0f}, true);
    uiScene.EditElementColor(rect4, {1.0f, 0.8f, 0.2f, 1.0f}, true);

    auto &c = uiScene.Get<VerticalContainer>(container);
    auto &c2 = uiScene.Get<VerticalContainer>(container2);
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

        uiScene.StepFrame(resolution);
        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}