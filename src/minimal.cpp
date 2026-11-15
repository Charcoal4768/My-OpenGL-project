#include "demo_scenes.h"

#include <algorithm>

namespace {

constexpr Color background{0.018f, 0.024f, 0.038f, 1.0f};
constexpr Color panel{0.045f, 0.059f, 0.084f, 1.0f};
constexpr Color panelRaised{0.071f, 0.091f, 0.126f, 1.0f};
constexpr Color outline{0.13f, 0.17f, 0.23f, 1.0f};
constexpr Color muted{0.24f, 0.30f, 0.39f, 1.0f};
constexpr Color blue{0.22f, 0.49f, 0.89f, 1.0f};
constexpr Color cyan{0.17f, 0.70f, 0.73f, 1.0f};
constexpr Color violet{0.48f, 0.37f, 0.79f, 1.0f};

int AddShape(UIScene &scene, int parent, float x, float y, float width,
             float height, Color color, float radius = 10.0f) {
    const int id = scene.AddElement<UIRect>();
    scene.AddChild(parent, id);
    scene.EditElementShape(id, {x, y, width, height}, true);
    scene.EditElementColor(id, color, true);
    scene.EditElementCornerRadius(id, radius, radius, radius, radius, true);
    return id;
}

void AddFrame(UIScene &scene, int id) {
    scene.EditElementBorder(id, 1.0f, 1.0f, 1.0f, 1.0f, true);
    scene.EditElementBorderColor(id, outline, true);
    scene.EditElementShadow(id, 12.0f, 0.0f, 4.0f,
                            {0.0f, 0.0f, 0.0f, 0.22f}, true);
}

} // namespace

void BuildMinimalScene(UIScene &scene,
                       const std::array<float, 2> &resolution) {
    const float width = resolution[0];
    const float height = resolution[1];
    const float panelWidth = std::min(780.0f, width - 48.0f);
    const float panelHeight = std::min(480.0f, height - 48.0f);
    const float panelX = (width - panelWidth) * 0.5f;
    const float panelY = (height - panelHeight) * 0.5f;

    const int root = scene.AddElement<UIRect>();
    scene.SetRoot(root);
    scene.EditElementShape(root, {0.0f, 0.0f, width, height}, true);
    scene.EditElementColor(root, background, true);

    const int card =
        AddShape(scene, root, panelX, panelY, panelWidth, panelHeight, panel,
                 15.0f);
    AddFrame(scene, card);

    AddShape(scene, card, 24.0f, 25.0f, 7.0f, 7.0f, cyan, 4.0f);
    AddShape(scene, card, 43.0f, 27.0f, 92.0f, 4.0f,
             {0.63f, 0.70f, 0.80f, 1.0f}, 2.0f);
    AddShape(scene, card, panelWidth - 86.0f, 27.0f, 5.0f, 5.0f, cyan, 3.0f);
    AddShape(scene, card, panelWidth - 72.0f, 28.0f, 48.0f, 3.0f, muted, 2.0f);

    const float contentY = 62.0f;
    const float contentHeight = panelHeight - contentY - 24.0f;
    const float contentWidth = panelWidth - 48.0f;
    const float gap = 20.0f;
    const float artworkWidth = contentWidth * 0.52f;
    const float detailsX = 24.0f + artworkWidth + gap;
    const float detailsWidth = panelWidth - detailsX - 24.0f;

    const int artwork = AddShape(scene, card, 24.0f, contentY, artworkWidth,
                                 contentHeight, panelRaised, 15.0f);
    scene.Get<UIRect>(artwork).clipChildren = true;
    scene.EditElementBorder(artwork, 1.0f, 1.0f, 1.0f, 1.0f, true);
    scene.EditElementBorderColor(artwork, outline, true);

    const int blueForm =
        AddShape(scene, artwork, -artworkWidth * 0.16f, contentHeight * 0.12f,
                 artworkWidth * 0.78f, contentHeight * 0.66f, blue, 15.0f);
    scene.EditElementShadow(blueForm, 8.0f, 0.0f, 3.0f,
                            {blue.r, blue.g, blue.b, 0.28f}, true);

    const int violetForm =
        AddShape(scene, artwork, artworkWidth * 0.43f, -contentHeight * 0.14f,
                 artworkWidth * 0.76f, contentHeight * 0.75f, violet, 15.0f);
    scene.EditElementShadow(violetForm, 8.0f, 0.0f, 3.0f,
                            {violet.r, violet.g, violet.b, 0.24f}, true);

    AddShape(scene, artwork, artworkWidth * 0.16f, contentHeight * 0.48f,
             artworkWidth * 0.90f, contentHeight * 0.62f, cyan, 15.0f);

    const int details = scene.AddElement<VerticalContainer>();
    scene.AddChild(card, details);
    scene.EditElementShape(details,
                           {detailsX, contentY, detailsWidth, contentHeight},
                           true);
    scene.EditElementColor(details, panelRaised, true);
    scene.EditElementCornerRadius(details, 15.0f, 15.0f, 15.0f, 15.0f, true);
    scene.EditElementBorder(details, 1.0f, 1.0f, 1.0f, 1.0f, true);
    scene.EditElementBorderColor(details, outline, true);
    scene.EditElementPadding(details, 9.0f, true);

    auto &list = scene.Get<VerticalContainer>(details);
    list.centerHorizontally = true;
    list.clipChildren = true;

    const Color accents[] = {blue, cyan, violet};
    const float rowWidth = detailsWidth - 36.0f;
    const float rowHeight = std::min(72.0f, (contentHeight - 36.0f) / 3.0f);
    for (int i = 0; i < 3; ++i) {
        const int row = AddShape(scene, details, 0.0f, 0.0f, rowWidth,
                                 rowHeight, panel, 10.0f);
        scene.EditElementBorder(row, 1.0f, 1.0f, 1.0f, 1.0f, true);
        scene.EditElementBorderColor(row, outline, true);
        AddShape(scene, row, 12.0f, (rowHeight - 10.0f) * 0.5f, 10.0f, 10.0f,
                 accents[i], 5.0f);
        AddShape(scene, row, 32.0f, 20.0f, rowWidth * 0.46f, 5.0f,
                 {0.60f, 0.68f, 0.78f, 1.0f}, 2.0f);
        AddShape(scene, row, 32.0f, 34.0f, rowWidth * 0.27f, 4.0f, muted,
                 2.0f);
        AddShape(scene, row, rowWidth - 31.0f, 26.0f, 18.0f, 5.0f,
                 accents[i], 3.0f);
    }

    const float footerY = panelHeight - 17.0f;
    AddShape(scene, card, 24.0f, footerY, contentWidth, 2.0f, outline, 1.0f);
    AddShape(scene, card, 24.0f, footerY, contentWidth * 0.38f, 2.0f, cyan,
             1.0f);
}
