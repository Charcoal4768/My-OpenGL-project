#include <UI/UIElements.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

void UIScene::SetRootViewport(float width, float height) {
    // meant to be called by
    // the StepFrame function at the very start
    viewportWidth = width;
    viewportHeight = height;
    if (rootId >= 0 && static_cast<size_t>(rootId) < ptrStore.size() &&
        ptrStore[rootId]) { // sync UI root to window size
        GeometryStoreState &rootShape = dataTables.geometry[rootId];
        if (rootShape.width != width || rootShape.height != height) {
            rootShape.width = width;
            rootShape.height = height;
            ptrStore[rootId]->isDirty = true;
            for (const auto &element : ptrStore) {
                if (element)
                    element->isDirty = true;
            }
        }
    }
}

void UIScene::MarkDirty(int id) {
    assert(ptrStore.back() != nullptr);
    if (id == I_UNSET || id >= ptrStore.size())
        return;

    UIElement *el = ptrStore[id].get();
    if (el)
        el->isDirty = true;
}

void UIScene::MarkParentChainDirty(int elementId) {
    assert(ptrStore.back() != nullptr);

    if (elementId == I_UNSET || elementId >= ptrStore.size())
        return;

    UIElement *el = ptrStore[elementId].get();

    if (!el)
        return;

    int parentId = el->parentId;
    el->isDirty = true;

    if (parentId == I_UNSET)
        return;

    MarkParentChainDirty(parentId);
}

ScissorRect IntersectRects(const ScissorRect &a, const ScissorRect &b) {
    GLint x1 = std::max(a.x, b.x);
    GLint y1 = std::max(a.y, b.y);
    GLint x2 = std::min(a.x + a.w, b.x + b.w);
    GLint y2 = std::min(a.y + a.h, b.y + b.h);

    if (x2 < x1 || y2 < y1) {
        return {0, 0, 0, 0};
    }
    return {x1, y1, x2 - x1, y2 - y1};
}

void UIScene::SetRoot(int id) {
    UIElement *ptr = ptrStore[id].get();
    assert(ptr != nullptr);      // checks if AddElement was even called for this, if
                                 // it was then id automatically is valid and > -1
    assert(ptr->parentId == -1); // root cannot have a parent
    rootId = id;
    MainHierarchy.SetRoot(id);
    MainLayout.SetRoot(id);
}

void Hierarchy::SetRoot(int id) {
    rootId = id;
    hirearchyDirty = true;
}

void LayoutManager::SetRoot(int id) { rootId = id; }

void UIScene::EditElementShape(int id, const GeometryStoreState &newShape,
                               bool dirtyChain) {
    assert(id >= 0);
    assert(id < ptrStore.size());
    assert(ptrStore[id] != nullptr);
    UIElement *el = ptrStore[id].get();

    GeometryStoreState &elementShape = dataTables.geometry[id];

    bool positionOrSizeChanged = (elementShape != newShape);

    if (!positionOrSizeChanged)
        return;

    elementShape.localX = newShape.localX;
    elementShape.localY = newShape.localY;
    elementShape.width = newShape.width;
    elementShape.height = newShape.height;

    // The layout pass owns absolute positioning. Preserve the new local shape,
    // but force the absolute values to be recalculated from the parent chain so
    // we don't keep stale screen-space coordinates around between edits.
    elementShape.absoluteX = F_UNSET;
    elementShape.absoluteY = F_UNSET;


    el->isDirty = true;
    if (dirtyChain && el->parentId != -1 && ptrStore[el->parentId]) {
        MarkParentChainDirty(id);
    }
}

static PackedColor PackColor(const Color &color) {
    const auto channel = [](float value) {
        return static_cast<uint32_t>(
            std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
    };
    return (channel(color.r) << 24) | (channel(color.g) << 16) |
           (channel(color.b) << 8) | channel(color.a);
}

void UIScene::EditElementColor(int id, Color newColor, bool dirtyChain) {
    assert(id >= 0);
    assert(id < ptrStore.size());
    assert(ptrStore[id] != nullptr);

    UIElement *el = ptrStore[id].get();

    PackedColor &elementColor = dataTables.style[id].color;
    const PackedColor packedColor = PackColor(newColor);

    bool colorChanged = elementColor != packedColor;

    if (!colorChanged)
        return;

    elementColor = packedColor;


    el->isDirty = true;
    if (dirtyChain && el->parentId != -1 && ptrStore[el->parentId]) {
        MarkParentChainDirty(id);
    }
}

void UIScene::EditElementBorder(int id, float borderTop = 0.0f,
                                float borderRight = 0.0f, float borderBottom = 0.0f,
                                float borderLeft = 0.0f, bool dirtyChain) {
    assert(id >= 0);
    assert(id < ptrStore.size());
    assert(ptrStore[id] != nullptr);

    UIElement *el = ptrStore[id].get();

    StyleStoreState &elementStyle = dataTables.style[id];

    bool borderChanged = (elementStyle.borderTop != borderTop ||
                          elementStyle.borderRight != borderRight ||
                          elementStyle.borderBottom != borderBottom ||
                          elementStyle.borderLeft != borderLeft);

    if (!borderChanged)
        return;

    elementStyle.borderTop = borderTop;
    elementStyle.borderRight = borderRight;
    elementStyle.borderBottom = borderBottom;
    elementStyle.borderLeft = borderLeft;


    el->isDirty = true;
    if (dirtyChain && el->parentId != -1 && ptrStore[el->parentId]) {
        MarkParentChainDirty(id);
    }
}

void UIScene::EditElementBorderColor(int id, Color borderColor, bool dirtyChain) {
    assert(id >= 0);
    assert(id < ptrStore.size());
    assert(ptrStore[id] != nullptr);

    UIElement *el = ptrStore[id].get();

    PackedColor &elementBorderColor = dataTables.style[id].borderColor;
    const PackedColor packedBorderColor = PackColor(borderColor);

    bool borderColorChanged = elementBorderColor != packedBorderColor;

    if (!borderColorChanged)
        return;

    elementBorderColor = packedBorderColor;


    el->isDirty = true;
    if (dirtyChain && el->parentId != -1 && ptrStore[el->parentId]) {
        MarkParentChainDirty(id);
    }
}

void UIScene::EditElementCornerRadius(int id, float topLeft, float topRight,
                                      float bottomLeft, float bottomRight,
                                      bool dirtyChain) {
    assert(id >= 0);
    assert(id < ptrStore.size());
    assert(ptrStore[id] != nullptr);

    UIElement *el = ptrStore[id].get();

    StyleStoreState &elementStyle = dataTables.style[id];

    bool cornerChanged = (elementStyle.cornerRadiusTopLeft != topLeft ||
                          elementStyle.cornerRadiusTopRight != topRight ||
                          elementStyle.cornerRadiusBottomLeft != bottomLeft ||
                          elementStyle.cornerRadiusBottomRight != bottomRight);

    if (!cornerChanged)
        return;

    elementStyle.cornerRadiusTopLeft = topLeft;
    elementStyle.cornerRadiusTopRight = topRight;
    elementStyle.cornerRadiusBottomLeft = bottomLeft;
    elementStyle.cornerRadiusBottomRight = bottomRight;


    el->isDirty = true;
    if (dirtyChain && el->parentId != -1 && ptrStore[el->parentId]) {
        MarkParentChainDirty(id);
    }
}

void UIScene::EditElementPadding(int id, float padding, bool dirtyChain) {
    assert(id >= 0);
    assert(id < ptrStore.size());
    assert(ptrStore[id] != nullptr);

    UIElement *el = ptrStore[id].get();

    float &elementPadding = dataTables.style[id].padding;

    bool paddingChanged = (elementPadding != padding);

    if (!paddingChanged)
        return;

    elementPadding = padding;


    el->isDirty = true;
    if (dirtyChain && el->parentId != -1 && ptrStore[el->parentId]) {
        MarkParentChainDirty(id);
    }
}

void UIScene::EditElementShadow(int id, float blur, float offsetX, float offsetY,
                                Color shadowColor, bool dirtyChain) {
    assert(id >= 0);
    assert(id < ptrStore.size());
    assert(ptrStore[id] != nullptr);

    UIElement *el = ptrStore[id].get();

    StyleStoreState &elementStyle = dataTables.style[id];
    const PackedColor packedShadowColor = PackColor(shadowColor);

    bool shadowChanged =
        (elementStyle.shadowBlur != blur || elementStyle.shadowOffsetX != offsetX ||
         elementStyle.shadowOffsetY != offsetY ||
         elementStyle.shadowColor != packedShadowColor);

    if (!shadowChanged)
        return;

    elementStyle.shadowBlur = blur;
    elementStyle.shadowOffsetX = offsetX;
    elementStyle.shadowOffsetY = offsetY;
    elementStyle.shadowColor = packedShadowColor;


    el->isDirty = true;
    if (dirtyChain && el->parentId != -1 && ptrStore[el->parentId]) {
        MarkParentChainDirty(id);
    }
}

void UIScene::EditElementVisibility(int id, bool hidden, bool dirtyChain) {
    assert(id >= 0);
    assert(id < ptrStore.size());
    assert(ptrStore[id] != nullptr);

    UIElement *el = ptrStore[id].get();

    bool &elementHidden = dataTables.style[id].hidden;

    bool visibilityChanged = (elementHidden != hidden);

    if (!visibilityChanged)
        return;

    elementHidden = hidden;


    el->isDirty = true;
    if (dirtyChain && el->parentId != -1 && ptrStore[el->parentId]) {
        MarkParentChainDirty(id);
    }
}

void UIScene::EditElementText(int id, std::string text, float fontSize,
                              std::string fontFamily) {
    if (id < 0 || static_cast<size_t>(id) >= ptrStore.size() ||
        !ptrStore[id]) {
        throw std::out_of_range("EditElementText received an invalid element ID.");
    }

    TextElement *element = dynamic_cast<TextElement *>(ptrStore[id].get());
    if (!element)
        throw std::invalid_argument("EditElementText requires a TextElement.");
    if (!std::isfinite(fontSize) || fontSize < 1.0f || fontSize > 512.0f)
        throw std::invalid_argument("Text font size must be between 1 and 512.");

    if (element->text == text && element->fontSize == fontSize &&
        element->fontFamily == fontFamily) {
        return;
    }

    element->text = std::move(text);
    element->fontSize = fontSize;
    element->fontFamily = std::move(fontFamily);
    element->textLayoutDirty = true;
    element->isDirty = true;
}

int UIScene::AddRectangle(int parentId, const GeometryStoreState &shape,
                          Color color, float cornerRadius, float borderWidth,
                          Color borderColor) {
    if (!std::isfinite(cornerRadius) || cornerRadius < 0.0f ||
        !std::isfinite(borderWidth) || borderWidth < 0.0f) {
        throw std::invalid_argument(
            "Rectangle corner radius and border width must be non-negative.");
    }

    const int id = AddElementToParent<UIRect>(parentId, shape);
    EditElementColor(id, color, false);
    EditElementCornerRadius(id, cornerRadius, cornerRadius, cornerRadius,
                            cornerRadius, false);
    if (borderWidth > 0.0f) {
        EditElementBorder(id, borderWidth, borderWidth, borderWidth,
                          borderWidth, false);
        EditElementBorderColor(id, borderColor, false);
    }
    return id;
}

int UIScene::AddText(int parentId, float x, float y, float fontSize,
                     std::string text, Color color, std::string fontFamily) {
    if (!std::isfinite(fontSize) || fontSize < 1.0f || fontSize > 512.0f) {
        throw std::invalid_argument("Text font size must be between 1 and 512.");
    }

    const int id = AddElementToParent<TextElement>(
        parentId, {x, y, F_UNSET, F_UNSET});
    EditElementText(id, std::move(text), fontSize, std::move(fontFamily));
    EditElementColor(id, color, false);
    return id;
}

const GeometryStoreState &UIScene::GetElementShape(int id) const {
    assert(id >= 0);
    assert(id < dataTables.geometry.size());

    return dataTables.geometry[id];
}

const StyleStoreState &UIScene::GetElementStyle(int id) const {
    assert(id >= 0);
    assert(id < dataTables.style.size());

    return dataTables.style[id];
}

const TraversalData &Hierarchy::RebuildTraversal(pointerVector &elementPointers) {
    // needs access to: ptrStore, rootId
    if (!hirearchyDirty)
        return traversal;

    traversal.displayList.clear();
    traversal.drawOrder.clear();
    traversal.parentIdLookup.clear();
    currentElementReferences = &elementPointers;

    if (currentElementReferences == nullptr)
        return traversal;

    if (rootId < 0 || rootId >= currentElementReferences->size() ||
        !(*currentElementReferences)[rootId]) {
        if (debug)
            std::cout << "ERROR: Invalid rootId (" << rootId
                      << ") in Hierarchy::RebuildTraversal!" << '\n';
        hirearchyDirty = false;
        currentElementReferences = nullptr;
        return traversal;
    }

    size_t targetCapacity = currentElementReferences->size() * 3 / 2;

    if (!currentElementReferences->empty() &&
        rootId < currentElementReferences->size() &&
        (*currentElementReferences)[rootId]) {
        if (targetCapacity > traversal.drawOrder.capacity() ||
            targetCapacity > traversal.displayList.capacity() ||
            targetCapacity > traversal.parentIdLookup.capacity()) {
            traversal.drawOrder.reserve(targetCapacity);
            traversal.displayList.reserve(targetCapacity);
            traversal.parentIdLookup.reserve(targetCapacity);
        }
        RecursiveDownTraversal(rootId);
    }

    hirearchyDirty = false;
    currentElementReferences = nullptr;
    return traversal;
}

const TraversalData &Hierarchy::ReturnCurrentTraversal() const {
    // redundant function but... good idea maybe
    if (traversal.empty()) {
        if (debug)
            std::cout
                << "WARNING: Traversal Data has not yet been generated but was "
                   "still requested."
                << '\n';
    }
    return traversal;
}

void Hierarchy::RecursiveDownTraversal(int elementId) {
    // expects traversal vectors to be fully empty
    // ill change it to not assume that in the future
    assert(currentElementReferences != nullptr);

    if (elementId == I_UNSET || elementId >= currentElementReferences->size())
        return;

    UIElement *el = (*currentElementReferences)[elementId].get();

    if (!el)
        return;

    bool useScissor = el->clipChildren;
    int parentId = el->parentId;

    if (parentId == I_UNSET && (elementId != rootId))
        return;

    traversal.displayList.push_back({RenderOpType::DrawElement, elementId});
    traversal.drawOrder.emplace_back(elementId);
    if (useScissor) {
        traversal.displayList.push_back({RenderOpType::PushClip, elementId});
    }

    if (parentId != I_UNSET) {
        // instance = element id, value = parent id
        // sanity checks:
        // elementId should exist in drawOrder | Done
        // elementId should not be I_UNSET | Done
        // Delete Function (to be implimented) will handle
        // setting parentIdLookup[deletedId] = I_UNSET; | Pending
        // every subsystem already ignores orphaned elements | Done
        if (elementId >= traversal.parentIdLookup.size()) {
            traversal.parentIdLookup.resize(elementId + 1, I_UNSET);
        }
        traversal.parentIdLookup[elementId] = parentId;
        if (debug)
            std::cout << elementId << "=" << parentId << '\n';
    }

    for (int childId : el->childIds) {
        RecursiveDownTraversal(childId);
    }

    if (useScissor) {
        traversal.displayList.push_back({RenderOpType::PopClip, elementId});
    }
}

void UIScene::Init() {
    // what do i do here?
    // set Renderer honestly for now, nothing else...
    MainHierarchy.hirearchyDirty = true;
}

void UIScene::SetDebug(bool enabled) {
    debug = enabled;
    MainHierarchy.debug = enabled;
    MainLayout.SetDebug(enabled);
    MainBatcher.SetDebug(enabled);
    MainRenderer.SetDebug(enabled);
}

void LayoutManager::SetDebug(bool enabled) { debug = enabled; }

void RenderBatcher::SetDebug(bool enabled) { debug = enabled; }

void Renderer::SetDebug(bool enabled) { debug = enabled; }

void UIScene::AddChild(int parentId, int childId) {
    if (parentId < 0 || parentId >= ptrStore.size() || !ptrStore[parentId])
        return;
    if (childId < 0 || childId >= ptrStore.size() || !ptrStore[childId])
        return;

    UIElement *parent = ptrStore[parentId].get();
    UIElement *child = ptrStore[childId].get();

    parent->childIds.push_back(child->id);
    child->parentId = parent->id;
    parent->isDirty = true;
    MainHierarchy.hirearchyDirty = true; // gonna make this something that takes
                                         // element ID which triggered rebuild
                                         // into account
}

void UIScene::RemoveChild(int childId) {
    if (childId < 0 || childId >= ptrStore.size() || !ptrStore[childId])
        return;
    UIElement *child = ptrStore[childId].get();
    int parentId = child->parentId;
    if (parentId < 0 || parentId >= ptrStore.size() || !ptrStore[parentId])
        return;
    UIElement *parent = ptrStore[parentId].get();
    std::vector<int> &childIds = parent->childIds;
    auto it = std::find(childIds.begin(), childIds.end(), childId);

    if (it != childIds.end()) {
        int instance = it - childIds.begin();
        std::swap(childIds[instance], childIds.back());
        childIds.pop_back();
        child->parentId = I_UNSET;
        child->isDirty = true;
        parent->isDirty = true;
    } else {
        return;
    }
    MainHierarchy.hirearchyDirty = true;
}

ScreenLayoutMode UIScene::GetScreenLayoutMode() const {
    if (dataTables.geometry[rootId].width < 600.0f)
        return ScreenLayoutMode::Mobile;
    return ScreenLayoutMode::Desktop;
}

// set root element to resolution size
// check if hirearchy is dirty (hirearchy will tell us) then rebuild
// regardless of wether or not its rebuilt, if hirearchy traversal data exists
// then use it for layout
// then use it for batching and draw command creation
// parse the commands and render the frame data
void UIScene::StepFrame(std::array<float, 2> &resolution) {
    SetRootViewport(resolution[0], resolution[1]);
    if (!MainRenderer.IsInitialized())
        MainRenderer.Init();

    const bool hierarchyChanged = MainHierarchy.hirearchyDirty;
    const TraversalData *currentTraversalData = nullptr;
    if (hierarchyChanged) {
        currentTraversalData = &MainHierarchy.RebuildTraversal(ptrStore);
        if (debug)
            std::cout << "Building Traversal" << '\n';
    } else {
        currentTraversalData = &MainHierarchy.ReturnCurrentTraversal();
        if (debug)
            std::cout << "Skipping Traversal" << '\n';
    }

    if (currentTraversalData->empty()) {
        if (debug)
            std::cout << "Scene Empty" << '\n';
        return;
    }

    const std::vector<int> &dirtyElementIds =
        MainLayout.RunInto(*currentTraversalData, ptrStore, dataTables);

    const RenderData &cachedFrameGraphicalData = MainBatcher.GetFrameData();
    bool dataEmpty = cachedFrameGraphicalData.empty();
    const RenderData *currentGraphicalData = &cachedFrameGraphicalData;
    const bool frameDataUpdated =
        dataEmpty || hierarchyChanged || !dirtyElementIds.empty();

    if (frameDataUpdated) {
        if (hierarchyChanged) {
            currentGraphicalData = &MainBatcher.ReBuildFrameData(
                dataTables, currentTraversalData->displayList, dirtyElementIds,
                resolution[1], ptrStore);
        } else {
            currentGraphicalData = &MainBatcher.SmartReBuildFrameData(
                dataTables, currentTraversalData->displayList, dirtyElementIds,
                resolution[1], ptrStore);
        }
        if (debug)
            std::cout << "Rebuilding Frame" << "| Layout dirty |"
                      << !dirtyElementIds.empty() << "| Data Empty | " << dataEmpty
                      << '\n';
    }

    if (currentGraphicalData->empty()) {
        if (debug)
            std::cout << "Data Empty" << '\n';
        return;
    }

    if (!MainRenderer.IsInitialized())
        MainRenderer.Init();

    if (debug)
        std::cout << "Vertices: " << currentGraphicalData->instances.size() * 6
                  << " | instances: " << currentGraphicalData->instances.size()
                  << " | Commands: " << currentGraphicalData->commands.size()
                  << '\n';

    if (frameDataUpdated)
        MainRenderer.UploadFrame(*currentGraphicalData);
    MainRenderer.DrawFrame(*currentGraphicalData, resolution);
}

uint32_t getShadowInfo(const float &blur, const float &offsetX,
                       const float &offsetY) {
    const int32_t blurValue =
        std::clamp(static_cast<int32_t>(std::lround(blur)), 0, 63);
    const int32_t offsetXValue =
        std::clamp(static_cast<int32_t>(std::lround(offsetX)), -32, 31);
    const int32_t offsetYValue =
        std::clamp(static_cast<int32_t>(std::lround(offsetY)), -32, 31);

    const uint32_t encodedX = static_cast<uint32_t>(offsetXValue + 32);
    const uint32_t encodedY = static_cast<uint32_t>(offsetYValue + 32);
    return static_cast<uint32_t>(blurValue) | (encodedX << 6) | (encodedY << 12);
}

uint32_t getBorderAndCornerInfo(int32_t borderTop, int32_t borderRight,
                                int32_t borderBottom, int32_t borderLeft, int32_t tl,
                                int32_t tr, int32_t bl, int32_t br) {
    const auto nibble = [](int32_t value) {
        return static_cast<uint32_t>(std::clamp(value, 0, 15));
    };
    return nibble(borderTop) | (nibble(borderRight) << 4) |
           (nibble(borderBottom) << 8) | (nibble(borderLeft) << 12) |
           (nibble(tl) << 16) | (nibble(tr) << 20) | (nibble(bl) << 24) |
           (nibble(br) << 28);
}

static ElementInstance BuildElementInstance(const GeometryStoreState &geometry,
                                            const StyleStoreState &style) {
    float x = geometry.absoluteX;
    float y = geometry.absoluteY;
    float w = geometry.width;
    float h = geometry.height;
    float cornerTL = style.cornerRadiusTopLeft;
    float cornerTR = style.cornerRadiusTopRight;
    float cornerBL = style.cornerRadiusBottomLeft;
    float cornerBR = style.cornerRadiusBottomRight;

    ElementInstance newInstance{};
    newInstance.hardwareID = 0;
    newInstance.transform[0] = x;
    newInstance.transform[1] = y;
    newInstance.transform[2] = w;
    newInstance.transform[3] = h;
    newInstance.payload[0] = style.color;
    newInstance.payload[1] = style.borderColor;
    newInstance.payload[2] = getBorderAndCornerInfo(
        static_cast<int32_t>(style.borderTop),
        static_cast<int32_t>(style.borderRight),
        static_cast<int32_t>(style.borderBottom),
        static_cast<int32_t>(style.borderLeft), static_cast<int32_t>(cornerTL),
        static_cast<int32_t>(cornerTR), static_cast<int32_t>(cornerBL),
        static_cast<int32_t>(cornerBR));
    newInstance.payload[3] = getShadowInfo(style.shadowBlur, style.shadowOffsetX,
                                          style.shadowOffsetY);
    newInstance.payload[4] = style.shadowColor;
    return newInstance;
}

static void AppendQuad(RenderData &frame, const GeometryStoreState &geometry,
                       const StyleStoreState &style) {
    frame.instances.push_back(BuildElementInstance(geometry, style));
}

static uint32_t PackNormalizedUv(float first, float second) {
    const auto channel = [](float value) {
        return static_cast<uint32_t>(
            std::lround(std::clamp(value, 0.0f, 1.0f) * 65535.0f));
    };
    return channel(first) | (channel(second) << 16);
}

static void AppendText(RenderData &frame, const GeometryStoreState &geometry,
                       const StyleStoreState &style,
                       const TextElement &textElement) {
    for (const TextGlyph &glyph : textElement.glyphs) {
        const GlyphAtlasEntry atlas = ResolveGlyphAtlasEntry(glyph);
        if (atlas.width == 0.0f || atlas.height == 0.0f)
            continue;

        ElementInstance instance{};
        instance.hardwareID = 2;
        instance.transform[0] =
            geometry.absoluteX + glyph.penX + glyph.xOffset + atlas.bitmapLeft;
        instance.transform[1] =
            geometry.absoluteY + glyph.baseline - glyph.yOffset - atlas.bitmapTop;
        instance.transform[2] = atlas.width;
        instance.transform[3] = atlas.height;
        instance.payload[0] = atlas.layer;
        instance.payload[1] = PackNormalizedUv(atlas.u0, atlas.v0);
        instance.payload[2] = PackNormalizedUv(atlas.u1, atlas.v1);
        instance.payload[3] = style.color;
        frame.instances.push_back(instance);
    }
}

static ScissorRect BuildScissorRect(const GeometryStoreState &geometry,
                                    float viewportHeight) {
    return {
        static_cast<GLint>(geometry.absoluteX),
        static_cast<GLint>(viewportHeight - (geometry.absoluteY + geometry.height)),
        static_cast<GLsizei>(geometry.width), static_cast<GLsizei>(geometry.height)};
}

static ClipGeometry BuildClipGeometry(const GeometryStoreState &geometry,
                                      const StyleStoreState &style) {
    return {{geometry.absoluteX, geometry.absoluteY, geometry.width,
             geometry.height},
            {style.cornerRadiusTopLeft, style.cornerRadiusTopRight,
             style.cornerRadiusBottomLeft, style.cornerRadiusBottomRight}};
}

const RenderData &RenderBatcher::ReBuildFrameData(
    UIStateTables &dataTables, const std::vector<RenderOp> &displayList,
    const std::vector<int> &dirtyElements, float viewportHeight,
    const pointerVector &elements) {

    FrameData.instances.clear();
    FrameData.clips.clear();
    FrameData.commands.clear();
    firstInstanceByElement.assign(elements.size(),
                              std::numeric_limits<size_t>::max());
    indexedElementCount = 0;
    scissorRects.clear();
    activeClips.clear();

    DrawCommand currentBatch; // we make a new batch
    bool batchStarted = false;

    const auto beginBatch = [&]() {
        currentBatch = {};
        currentBatch.instanceOffset = FrameData.instances.size();
        currentBatch.instanceCount = 0;
        currentBatch.clipOffset = FrameData.clips.size();
        currentBatch.clipCount = static_cast<GLsizei>(activeClips.size());
        FrameData.clips.insert(FrameData.clips.end(), activeClips.begin(),
                               activeClips.end());
        if (!scissorRects.empty()) {
            currentBatch.useScissor = true;
            currentBatch.scissorBox = scissorRects.back();
        }
        batchStarted = true;
    };
    const auto flushBatch = [&]() {
        if (batchStarted && currentBatch.instanceCount > 0) {
            FrameData.commands.push_back(currentBatch);
        }
        batchStarted = false;
    };

    for (const RenderOp &op : displayList) {
        switch (op.type) {
        case RenderOpType::PushClip: {
            flushBatch();

            ScissorRect rect =
                BuildScissorRect(dataTables.geometry[op.elementId], viewportHeight);

            if (!scissorRects.empty()) {
                rect = IntersectRects(scissorRects.back(), rect);
            }

            scissorRects.push_back(rect);
            activeClips.push_back(BuildClipGeometry(
                dataTables.geometry[op.elementId], dataTables.style[op.elementId]));
            if (activeClips.size() > static_cast<size_t>(MAX_CLIP_DEPTH)) {
                throw std::runtime_error(
                    "Nested rounded clipping exceeds the renderer limit of " +
                    std::to_string(MAX_CLIP_DEPTH));
            }

            break;
        }

        case RenderOpType::PopClip: {
            flushBatch();
            scissorRects.pop_back();
            activeClips.pop_back();
            break;
        }

        case RenderOpType::DrawElement: {
            const GeometryStoreState &geometry = dataTables.geometry[op.elementId];
            const StyleStoreState &style = dataTables.style[op.elementId];
            if (style.hidden)
                break;
            if (!batchStarted)
                beginBatch();

            const size_t firstInstance = FrameData.instances.size();
            if (auto *text =
                    dynamic_cast<TextElement *>(elements[op.elementId].get())) {
                AppendText(FrameData, geometry, style, *text);
            } else {
                AppendQuad(FrameData, geometry, style);
            }
            const size_t instanceCount = FrameData.instances.size() - firstInstance;
            if (instanceCount > 0) {
                firstInstanceByElement[static_cast<size_t>(op.elementId)] =
                    firstInstance;
                ++indexedElementCount;
                currentBatch.instanceCount += static_cast<GLsizei>(instanceCount);
            }
            break;
        }

        default:
            break;
        }
    }

    flushBatch();
    return FrameData;
}

const RenderData &RenderBatcher::SmartReBuildFrameData(
    UIStateTables &dataTables, const std::vector<RenderOp> &displayList,
    const std::vector<int> &dirtyElements, float viewportHeight,
    const pointerVector &elements) {
    bool dataEmpty = FrameData.empty();

    if (dirtyElements.empty() && !dataEmpty) {
        return FrameData;
    }

    if (dataEmpty) {
        return ReBuildFrameData(dataTables, displayList, dirtyElements,
                                viewportHeight, elements);
    }

    // If the scene has become broadly dirty, the full rebuild path is cheaper
    // than a partial update. This keeps the optimization useful for sparse dirty
    // sets without turning into two full scans in heavy stress cases.
    if (dirtyElements.size() >= indexedElementCount) {
        return ReBuildFrameData(dataTables, displayList, dirtyElements,
                                viewportHeight, elements);
    }

    dirtyMarks.resize(elements.size(), 0);
    if (++dirtyGeneration == 0) {
        std::fill(dirtyMarks.begin(), dirtyMarks.end(), 0);
        dirtyGeneration = 1;
    }
    for (int elementId : dirtyElements) {
        if (elementId >= 0 &&
            static_cast<size_t>(elementId) < dirtyMarks.size()) {
            dirtyMarks[static_cast<size_t>(elementId)] = dirtyGeneration;
        }
    }
    const auto isDirty = [this](int elementId) {
        return elementId >= 0 &&
               static_cast<size_t>(elementId) < dirtyMarks.size() &&
               dirtyMarks[static_cast<size_t>(elementId)] == dirtyGeneration;
    };

    for (const RenderOp &op : displayList) {
        if (op.type == RenderOpType::PushClip && isDirty(op.elementId)) {
            return ReBuildFrameData(dataTables, displayList, dirtyElements,
                                    viewportHeight, elements);
        }
        if (op.type != RenderOpType::DrawElement)
            continue;

        if (isDirty(op.elementId) &&
            dynamic_cast<const TextElement *>(
                elements[op.elementId].get()) != nullptr) {
            return ReBuildFrameData(dataTables, displayList, dirtyElements,
                                    viewportHeight, elements);
        }

        if (op.elementId < 0 ||
            static_cast<size_t>(op.elementId) >= firstInstanceByElement.size() ||
            firstInstanceByElement[static_cast<size_t>(op.elementId)] ==
                std::numeric_limits<size_t>::max()) {
            return ReBuildFrameData(dataTables, displayList, dirtyElements,
                                    viewportHeight, elements);
        }

        if (!isDirty(op.elementId))
            continue;

        const GeometryStoreState &geometry = dataTables.geometry[op.elementId];
        const StyleStoreState &style = dataTables.style[op.elementId];

        if (style.hidden) {
            return ReBuildFrameData(dataTables, displayList, dirtyElements,
                                    viewportHeight, elements);
        }

        FrameData.instances[firstInstanceByElement[static_cast<size_t>(op.elementId)]] =
            BuildElementInstance(geometry, style);
    }

    return FrameData;
}

const RenderData &RenderBatcher::GetFrameData() const { return FrameData; }

void Renderer::Init() {
    if (initialized) {
        if (debug)
            std::cout << "WARNING: RENDERER ALREADY INITIALIZED" << '\n';
        return;
    }
    DefaultShader.Load("default.vert", "default.frag");
    resolutionUniform = glGetUniformLocation(DefaultShader.ID, "u_resolution");
    clipCountUniform = glGetUniformLocation(DefaultShader.ID, "u_clipCount");
    clipRectsUniform = glGetUniformLocation(DefaultShader.ID, "u_clipRects");
    clipRadiiUniform = glGetUniformLocation(DefaultShader.ID, "u_clipRadii");
    glyphAtlasUniform = glGetUniformLocation(DefaultShader.ID, "u_glyphAtlas");
    InitializeGlyphAtlas();
    DefaultShader.Activate();
    glUniform1i(glyphAtlasUniform, 0);
    initialized = true;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (debug)
        std::cout << "VAO ID: " << MainVAO.ID << " | VBO ID: " << MainVBO.ID << '\n';
}

void Renderer::DrawFrame(const RenderData &frameData,
                         const std::array<float, 2> &resolution) {
    if (!initialized) {
        if (debug)
            std::cout << "WARNING: RENDERER NOT INITIALIZED BUT "
                         "Renderer::DrawFrame() "
                         "WAS CALLED"
                      << '\n';
        return;
    }
    if (debug) {
        while (glGetError() != GL_NO_ERROR) {
        }
    }

    DefaultShader.Activate();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, GetGlyphAtlasTexture());

    if (resolutionUniform != -1) {
        glUniform2f(resolutionUniform, static_cast<float>(resolution[0]),
                    static_cast<float>(resolution[1]));

        if (debug) {
            GLenum err = glGetError();
            if (err != GL_NO_ERROR) {
                std::cout << "Error in glUniform2f (loc=" << resolutionUniform
                          << "): 0x" << std::hex << err << std::dec << '\n';
            }
        }
    } else {
        if (debug)
            std::cout << "WARNING: u_resolution uniform location is -1! Check "
                         "Shader::Load."
                      << '\n';
    }

    MainVAO.Bind();
    // MainEBO.Bind();
    MainVBO.Bind();

    std::array<GLfloat, MAX_CLIP_DEPTH * 4> clipRects{};
    std::array<GLfloat, MAX_CLIP_DEPTH * 4> clipRadii{};

    for (const DrawCommand &cmd : frameData.commands) {
        if (cmd.instanceCount == 0)
            continue;

        if (cmd.clipCount < 0 || cmd.clipCount > MAX_CLIP_DEPTH ||
            cmd.clipOffset + static_cast<size_t>(cmd.clipCount) >
                frameData.clips.size()) {
            throw std::runtime_error("Draw command contains an invalid clip range.");
        }

        uintptr_t baseOffset = cmd.instanceOffset * sizeof(ElementInstance);
        MainVAO.LinkAttrib(MainVBO, HardwareIDLayout, baseOffset);
        MainVAO.LinkAttrib(MainVBO, TransformLayout, baseOffset);
        MainVAO.LinkAttrib(MainVBO, Payload0Layout, baseOffset);
        MainVAO.LinkAttrib(MainVBO, Payload1Layout, baseOffset);

        glUniform1i(clipCountUniform, cmd.clipCount);
        for (GLsizei i = 0; i < cmd.clipCount; ++i) {
            const ClipGeometry &clip = frameData.clips[cmd.clipOffset + i];
            std::copy(std::begin(clip.rect), std::end(clip.rect),
                      clipRects.begin() + i * 4);
            std::copy(std::begin(clip.cornerRadii), std::end(clip.cornerRadii),
                      clipRadii.begin() + i * 4);
        }
        if (cmd.clipCount > 0) {
            glUniform4fv(clipRectsUniform, cmd.clipCount, clipRects.data());
            glUniform4fv(clipRadiiUniform, cmd.clipCount, clipRadii.data());
        }

        if (cmd.useScissor) {
            glEnable(GL_SCISSOR_TEST);
            glScissor(cmd.scissorBox.x, cmd.scissorBox.y, cmd.scissorBox.w,
                      cmd.scissorBox.h);

            if (debug) {
                GLenum err = glGetError();
                if (err != GL_NO_ERROR) {
                    std::cout << "Error in glScissor (" << cmd.scissorBox.w << "x"
                              << cmd.scissorBox.h << "): 0x" << std::hex << err
                              << std::dec << '\n';
                }
            }
        } else {
            glDisable(GL_SCISSOR_TEST);
        }

        // glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(cmd.instanceCount),
        //                GL_UNSIGNED_INT, (void *)(cmd.instanceOffset *
        //                sizeof(GLuint)));
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6,
                              static_cast<GLsizei>(cmd.instanceCount));

        if (debug) {
            GLenum err = glGetError();
            if (err != GL_NO_ERROR) {
                std::cout << "Error in glDrawElements: 0x" << std::hex << err
                          << std::dec << '\n';
            }
        }
    }

    if (debug)
        std::cout << "VAO ID: " << MainVAO.ID << " | VBO ID: " << MainVBO.ID << '\n';

    glDisable(GL_SCISSOR_TEST);
    // MainEBO.Unbind();
    MainVBO.Unbind();
    MainVAO.Unbind();
}

void Renderer::UploadFrame(const RenderData &frameData) {
    if (!initialized) {
        if (debug)
            std::cout << "WARNING: RENDERER NOT INITIALIZED BUT "
                         "Renderer::UploadFrame() WAS CALLED"
                      << '\n';
        return;
    }
    MainVAO.Bind();
    MainVBO.Data(frameData.instances.size() * sizeof(ElementInstance),
                 frameData.instances.data());
    MainVAO.Unbind();
    MainVBO.Unbind();
    // MainEBO.Unbind();
}
