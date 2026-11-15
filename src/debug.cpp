#include "demo_scenes.h"

#include <iostream>

void BuildDebugScene(UIScene &uiScene,
                     const std::array<float, 2> &resolution) {
    auto root = uiScene.AddElement<UIRect>();
    auto rect2 = uiScene.AddElement<UIRect>();
    auto rect3 = uiScene.AddElement<UIRect>();
    auto rect4 = uiScene.AddElement<UIRect>();
    auto container2 = uiScene.AddElement<VerticalContainer>();
    auto container = uiScene.AddElement<VerticalContainer>();
    auto rect1 = uiScene.AddElement<UIRect>();
    auto rect5 = uiScene.AddElement<UIRect>();

    uiScene.SetRoot(root);
    uiScene.AddChild(root, container);
    uiScene.AddChild(root, container2);
    uiScene.AddChild(container2, rect1);
    uiScene.AddChild(container, rect2);
    uiScene.AddChild(container2, rect3);
    uiScene.AddChild(container, rect4);
    uiScene.AddChild(root, rect5);

    uiScene.EditElementShape(root, {0.0f, 0.0f, resolution[0], resolution[1]},
                             true);
    uiScene.EditElementColor(root, {0.2f, 0.2f, 0.6f, 1.0f}, true);

    uiScene.EditElementShape(rect5, {400.0f, 400.0f, 200.0f, 100.0f}, true);
    uiScene.EditElementColor(rect5, {0.1f, 0.8f, 0.1f, 1.0f}, true);
    uiScene.EditElementBorder(rect5, 2.0f, 2.0f, 2.0f, 2.0f, true);
    uiScene.EditElementBorderColor(rect5, {1.0f, 1.0f, 1.0f, 1.0f}, true);
    uiScene.EditElementCornerRadius(rect5, 20.0f, 20.0f, 20.0f, 2.0f, true);
    uiScene.EditElementShadow(rect5, 10.0f, 5.0f, 5.0f,
                              {0.0f, 0.0f, 0.0f, 0.4f}, true);

    uiScene.EditElementShape(container, {100.0f, 20.0f, 30.0f, 30.0f}, true);
    uiScene.EditElementColor(container, {0.5f, 0.1f, 0.5f, 1.0f}, true);
    uiScene.EditElementPadding(container, 0.0f, true);
    uiScene.EditElementCornerRadius(container, 5.0f, 5.0f, 5.0f, 5.0f, true);
    uiScene.EditElementBorder(container, 20.0f, 2.0f, 2.0f, 2.0f, true);
    uiScene.EditElementBorderColor(container, {1.0f, 1.0f, 1.0f, 1.0f}, true);

    uiScene.EditElementShape(container2, {180.0f, 40.0f, 10.0f, 20.0f}, true);
    uiScene.EditElementColor(container2, {1.0f, 0.5f, 0.1f, 1.0f}, true);
    uiScene.EditElementPadding(container2, 5.0f, true);

    uiScene.EditElementShape(rect1, {0.0f, 0.0f, 60.0f, 40.0f}, true);
    uiScene.EditElementColor(rect1, {0.3f, 1.0f, 0.4f, 1.0f}, true);
    uiScene.EditElementCornerRadius(rect1, 20.0f, 20.0f, 20.0f, 20.0f, true);

    uiScene.EditElementShape(rect2, {0.0f, 0.0f, 120.0f, 80.0f}, true);
    uiScene.EditElementColor(rect2, {0.2f, 0.8f, 0.2f, 1.0f}, true);
    uiScene.EditElementShadow(rect2, 5.0f, 0.0f, 0.0f,
                              {0.0f, 0.0f, 0.0f, 0.4f}, true);

    uiScene.EditElementShape(rect3, {0.0f, 0.0f, 40.0f, 60.0f}, true);
    uiScene.EditElementColor(rect3, {0.2f, 0.4f, 1.0f, 1.0f}, true);

    uiScene.EditElementShape(rect4, {0.0f, 0.0f, 90.0f, 90.0f}, true);
    uiScene.EditElementColor(rect4, {1.0f, 0.8f, 0.2f, 1.0f}, true);

    auto &firstContainer = uiScene.Get<VerticalContainer>(container);
    auto &secondContainer = uiScene.Get<VerticalContainer>(container2);
    firstContainer.centerHorizontally = true;
    firstContainer.fitContentHeight = true;
    firstContainer.fitContentWidth = true;
    firstContainer.resizeChildren = true;
    firstContainer.clipChildren = true;

    secondContainer.centerHorizontally = true;
    secondContainer.fitContentHeight = true;
    secondContainer.fitContentWidth = true;
    secondContainer.resizeChildren = false;
    secondContainer.clipChildren = true;

    std::cout << "Size of ElementInstance: " << sizeof(ElementInstance)
              << " bytes\n"
              << "Size of CPU side struct: "
              << sizeof(UIElement) + sizeof(StyleStoreState) +
                     sizeof(GeometryStoreState)
              << " bytes\n";
}
