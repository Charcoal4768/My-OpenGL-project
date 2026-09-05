#include <UI/UIElements.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

void UIScene::SetRootViewport(float width, float height) {
    // meant to be called by
    // the StepFrame function at the very start
    viewportWidth = width;
    viewportHeight = height;
    if (!ptrStore.empty() && ptrStore[rootId]) { // sync UI root to window size
        GeometryStoreState &rootShape = dataTables.geometry[rootId];
        if (rootShape.width != width || rootShape.height != height) {
            rootShape.width = width;
            rootShape.height = height;
            ptrStore[rootId]->isDirty = true;
            frameDataRebuild = true;
        }
    }
}

std::array<float, 2> ApplyStyle(float computedWidth, float computedHeight,
                                float parentWidth, float parentHeight,
                                const StyleStoreState &style) {
    // we want final size to be basically css clamp() type stuff
    // preferredWidth/Height are percentages: 0.0 -> 1.0
    // also, either Percentages or absolute pixel sizing, both = bug in the app
    // using this framework
    float prefferedWidth;
    float prefferedHeight;

    prefferedWidth = (style.prefferedWidthPercent != F_UNSET)
                         ? (style.prefferedWidthPercent * parentWidth)
                         : computedWidth;
    prefferedHeight = (style.prefferedHeightPercent != F_UNSET)
                          ? (style.prefferedHeightPercent * parentHeight)
                          : computedHeight;

    // // print preferred size
    // std::cout << "Preferred Size: " << prefferedWidth << " x "
    //           << prefferedHeight << '\n';

    if (style.maxWidth != F_UNSET)
        prefferedWidth = std::min(prefferedWidth, style.maxWidth);
    if (style.maxHeight != F_UNSET)
        prefferedHeight = std::min(prefferedHeight, style.maxHeight);

    float finalWidth = prefferedWidth;

    if (style.minWidth != F_UNSET)
        finalWidth = std::max(finalWidth, style.minWidth);

    float finalHeight = prefferedHeight;

    if (style.minHeight != F_UNSET)
        finalHeight = std::max(finalHeight, style.minHeight);

    return {finalWidth, finalHeight};
}

// void LayoutManager::Init(pointerVector &elementPointers) {
//     // assert(elementPointers.back() != nullptr);
//     if (initialized == true) {
//         std::cout << "WARNING:: Layout Manager Initialized when references"
//                      " already populated"
//                   << std::endl;
//         return;
//     }
//     context.currentElementReferences = &elementPointers;
// }

bool LayoutManager::Run(const TraversalData &traversal,
                        pointerVector &elementPointers, UIStateTables &dataTables) {
    bool rebuildRender = false;
    if (elementPointers.back() == nullptr)
        return rebuildRender;
    context.currentElementReferences = &elementPointers;
    for (int elementId : traversal.drawOrder) {
        int parentId = (elementId < traversal.parentIdLookup.size())
                           ? traversal.parentIdLookup[elementId]
                           : I_UNSET;
        if (parentId == I_UNSET && (elementId != rootId))
            continue;
        UIElement *el = elementPointers[elementId].get();
        if (!el)
            continue;
        if (el->isDirty) {
            if (el->UpdateLayout(dataTables, context) && (el->id != rootId)) {
                rebuildRender = true; // render commands need to be rebuilt
            }
        }
        GeometryStoreState &elementShape = dataTables.geometry[elementId];
        if (parentId != I_UNSET) {
            GeometryStoreState &parentShape = dataTables.geometry[parentId];
            elementShape.absoluteX = parentShape.absoluteX + elementShape.localX;
            elementShape.absoluteY = parentShape.absoluteY + elementShape.localY;
        } else {
            elementShape.absoluteX =
                elementShape.localX != F_UNSET ? elementShape.localX : 0.0f;
            elementShape.absoluteY =
                elementShape.localY != F_UNSET ? elementShape.localY : 0.0f;
        }
    }
    context.currentElementReferences = nullptr;
    return rebuildRender;
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

void LayoutContext::MarkDirty(int id) {
    assert(currentElementReferences != nullptr);
    if (id == I_UNSET || id >= currentElementReferences->size())
        return;

    UIElement *el = (*currentElementReferences)[id].get();
    if (el)
        el->isDirty = true;
}

void LayoutContext::MarkParentChainDirty(int elementId) {
    assert(currentElementReferences != nullptr);

    if (elementId == I_UNSET || elementId >= currentElementReferences->size())
        return;

    UIElement *el = (*currentElementReferences)[elementId].get();

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

    elementShape = newShape;
    // elementShape.absoluteX = oldAbsX;
    // elementShape.absoluteY = oldAbsY;

    if (dirtyChain) {
        el->isDirty = true;
        if (el->parentId != -1 && ptrStore[el->parentId]) {
            MarkParentChainDirty(id);
        }
    }
}

void UIScene::EditElementColor(int id, const Color &newColor, bool dirtyChain) {
    assert(id >= 0);
    assert(id < ptrStore.size());
    assert(ptrStore[id] != nullptr);

    UIElement *el = ptrStore[id].get();

    Color &elementColor = dataTables.style[id].color;

    bool colorChanged = (elementColor != newColor);

    if (!colorChanged)
        return;

    elementColor = newColor;

    if (dirtyChain && colorChanged) {
        el->isDirty = true;
        if (el->parentId != -1 && ptrStore[el->parentId]) {
            MarkParentChainDirty(id);
        }
    }
}

void UIScene::EditElementBorder(int id, float borderWidth, bool dirtyChain) {
    assert(id >= 0);
    assert(id < ptrStore.size());
    assert(ptrStore[id] != nullptr);

    UIElement *el = ptrStore[id].get();

    float &elementBorderWidth = dataTables.style[id].borderWidth;

    bool borderChanged = (elementBorderWidth != borderWidth);

    if (!borderChanged)
        return;

    elementBorderWidth = borderWidth;

    if (dirtyChain && borderChanged) {
        el->isDirty = true;
        if (el->parentId != -1 && ptrStore[el->parentId]) {
            MarkParentChainDirty(id);
        }
    }
}

void UIScene::EditElementBorderColor(int id, const Color &borderColor,
                                     bool dirtyChain) {
    assert(id >= 0);
    assert(id < ptrStore.size());
    assert(ptrStore[id] != nullptr);

    UIElement *el = ptrStore[id].get();

    Color &elementBorderColor = dataTables.style[id].borderColor;

    bool borderColorChanged = (elementBorderColor != borderColor);

    if (!borderColorChanged)
        return;

    elementBorderColor = borderColor;

    if (dirtyChain && borderColorChanged) {
        el->isDirty = true;
        if (el->parentId != -1 && ptrStore[el->parentId]) {
            MarkParentChainDirty(id);
        }
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

    if (dirtyChain && cornerChanged) {
        el->isDirty = true;
        if (el->parentId != -1 && ptrStore[el->parentId]) {
            MarkParentChainDirty(id);
        }
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

    if (dirtyChain && paddingChanged) {
        el->isDirty = true;
        if (el->parentId != -1 && ptrStore[el->parentId]) {
            MarkParentChainDirty(id);
        }
    }
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

    if (useScissor) {
        traversal.displayList.push_back({RenderOpType::PushScissor, elementId});
    }

    traversal.displayList.push_back({RenderOpType::DrawElement, elementId});
    traversal.drawOrder.emplace_back(elementId);
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
        traversal.displayList.push_back({RenderOpType::PopScissor, elementId});
    }
}

void UIScene::Init() {
    // what do i do here?
    // set Renderer honestly for now, nothing else...
    if (!MainRenderer.IsInitialized())
        MainRenderer.Init();

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
    }
    MainHierarchy.hirearchyDirty = true;
}

bool UIElement::UpdateLayout(UIStateTables &data, LayoutContext &context) {
    bool changed = false;

    if (id == I_UNSET || id >= data.geometry.size())
        return changed;

    GeometryStoreState &myGeometry = data.geometry[id];
    StyleStoreState &myStyle = data.style[id];

    // Get parent dimensions for reference
    float parentWidth = 800.0f;  // default fallback
    float parentHeight = 800.0f; // default fallback

    if (parentId != I_UNSET && parentId < data.geometry.size()) {
        parentWidth = data.geometry[parentId].width;
        parentHeight = data.geometry[parentId].height;
    }

    // Apply style to get final size
    auto finalSize = ApplyStyle(myGeometry.width, myGeometry.height, parentWidth,
                                parentHeight, myStyle);

    if (finalSize[0] != myGeometry.width || finalSize[1] != myGeometry.height) {
        myGeometry.width = finalSize[0];
        myGeometry.height = finalSize[1];
        changed = true;
    }

    isDirty = false;
    return changed;
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

    TraversalData frameTraversalData;
    if (MainHierarchy.hirearchyDirty == true) {
        frameTraversalData = MainHierarchy.RebuildTraversal(ptrStore);
        if (debug)
            std::cout << "Building Traversal" << '\n';
    } else {
        frameTraversalData = MainHierarchy.ReturnCurrentTraversal();
        if (debug)
            std::cout << "Skipping Traversal" << '\n';
    }

    const TraversalData &currentTraversalData = frameTraversalData;
    if (currentTraversalData.empty()) {
        if (debug)
            std::cout << "Scene Empty" << '\n';
        return;
    }

    bool rebuildFrameData =
        MainLayout.Run(currentTraversalData, ptrStore, dataTables);

    RenderData frameGraphicalData = MainBatcher.GetFrameData();
    bool dataEmpty = frameGraphicalData.empty();
    if (dataEmpty || rebuildFrameData) {
        frameGraphicalData = MainBatcher.ReBuildFrameData(
            dataTables, currentTraversalData.displayList, resolution[1]);
        if (debug)
            std::cout << "Rebuilding Frame" << "| Layout dirty |" << rebuildFrameData
                      << "| Data Empty | " << dataEmpty << '\n';
    }

    const RenderData &currentGraphicalData = frameGraphicalData;
    if (currentGraphicalData.empty()) {
        if (debug)
            std::cout << "Data Empty" << '\n';
        return;
    }

    if (!MainRenderer.IsInitialized())
        MainRenderer.Init();

    if (debug)
        std::cout << "Vertices: " << currentGraphicalData.instances.size() * 6
                  << " | instances: " << currentGraphicalData.instances.size()
                  << " | Commands: " << currentGraphicalData.commands.size() << '\n';

    MainRenderer.UploadFrame(currentGraphicalData);
    MainRenderer.DrawFrame(currentGraphicalData.commands, resolution);
}

uint32_t PackColor(const Color &color) {
    uint32_t packedColor;
    unsigned int rBits =
        static_cast<unsigned int>(std::clamp(color.r * 255.0f, 0.0f, 255.0f));
    unsigned int gBits =
        static_cast<unsigned int>(std::clamp(color.g * 255.0f, 0.0f, 255.0f)) << 8;
    unsigned int bBits =
        static_cast<unsigned int>(std::clamp(color.b * 255.0f, 0.0f, 255.0f)) << 16;
    unsigned int aBits =
        static_cast<unsigned int>(std::clamp(color.a * 255.0f, 0.0f, 255.0f)) << 24;
    packedColor = rBits | gBits | bBits | aBits;
    return packedColor;
}

static void AppendQuad(RenderData &frame, const GeometryStoreState &geometry,
                       const StyleStoreState &style) {
    float x = geometry.absoluteX;
    float y = geometry.absoluteY;
    float w = geometry.width;
    float h = geometry.height;
    float borderWidth = style.borderWidth;
    float cornerRadiusTopLeft = style.cornerRadiusTopLeft;
    float cornerRadiusTopRight = style.cornerRadiusTopRight;
    float cornerRadiusBottomLeft = style.cornerRadiusBottomLeft;
    float cornerRadiusBottomRight = style.cornerRadiusBottomRight;

    // packing color
    uint32_t packedColor = PackColor(style.color);
    ElementInstance newInstance;

    frame.instances.push_back(newInstance);
}

static ScissorRect BuildScissorRect(const GeometryStoreState &geometry,
                                    float viewportHeight) {
    return {
        static_cast<GLint>(geometry.absoluteX),
        static_cast<GLint>(viewportHeight - (geometry.absoluteY + geometry.height)),
        static_cast<GLsizei>(geometry.width), static_cast<GLsizei>(geometry.height)};
}

const RenderData &
RenderBatcher::ReBuildFrameData(UIStateTables &dataTables,
                                const std::vector<RenderOp> &displayList,
                                float viewportHeight) {
    FrameData.instances.clear();
    FrameData.commands.clear();

    DrawCommand currentBatch; // we make a new batch
    currentBatch.instanceOffset = 0;
    currentBatch.instanceCount = 0;

    std::vector<ScissorRect> scissorRects;

    for (const RenderOp &op : displayList) {
        switch (op.type) {
        case RenderOpType::PushScissor: {
            if (currentBatch.instanceCount > 0) {
                FrameData.commands.push_back(currentBatch);
                currentBatch.instanceOffset = FrameData.instances.size();
                currentBatch.instanceCount = 0;
            }

            ScissorRect rect =
                BuildScissorRect(dataTables.geometry[op.elementId], viewportHeight);

            if (!scissorRects.empty()) {
                rect = IntersectRects(scissorRects.back(), rect);
            }

            scissorRects.push_back(rect);

            currentBatch.useScissor = true;
            currentBatch.scissorBox = rect;

            break;
        }

        case RenderOpType::PopScissor: {
            if (currentBatch.instanceCount > 0) {
                FrameData.commands.push_back(currentBatch);
                currentBatch.instanceOffset = FrameData.instances.size();
                currentBatch.instanceCount = 0;
            }

            scissorRects.pop_back();

            if (scissorRects.empty()) {
                currentBatch.useScissor = false;
            } else {
                currentBatch.useScissor = true;
                currentBatch.scissorBox = scissorRects.back();
            }
            break;
        }

        case RenderOpType::DrawElement: {
            const GeometryStoreState &geometry = dataTables.geometry[op.elementId];
            const StyleStoreState &style = dataTables.style[op.elementId];
            if (style.hidden)
                break;
            AppendQuad(FrameData, geometry, style);
            currentBatch.instanceCount += 1;
            break;
        }

        default:
            break;
        }
    }

    if (currentBatch.instanceCount > 0) {
        FrameData.commands.push_back(currentBatch);
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
    MainVAO.Bind();
    MainVBO.Bind();
    // MainEBO.Bind();

    MainVAO.LinkAttrib(MainVBO, TransformLayout);
    MainVAO.LinkAttrib(MainVBO, PackedColor);
    MainVAO.LinkAttrib(MainVBO, BorderStyle);
    MainVAO.LinkAttrib(MainVBO, CornerStyle);

    MainVAO.Unbind();
    MainVBO.Unbind();
    // MainEBO.Unbind();
    resolutionUniform = glGetUniformLocation(DefaultShader.ID, "u_resolution");
    initialized = true;
    if (debug)
        std::cout << "VAO ID: " << MainVAO.ID << " | VBO ID: " << MainVBO.ID << '\n';
}

void Renderer::DrawFrame(const std::vector<DrawCommand> &commandsData,
                         const std::array<float, 2> &resolution) {
    if (!initialized) {
        if (debug)
            std::cout << "WARNING: RENDERER NOT INITIALIZED BUT "
                         "Renderer::DrawFrame() "
                         "WAS CALLED"
                      << '\n';
        return;
    }
    while (glGetError() != GL_NO_ERROR)
        ;

    DefaultShader.Activate();

    if (resolutionUniform != -1) {
        glUniform2f(resolutionUniform, static_cast<float>(resolution[0]),
                    static_cast<float>(resolution[1]));

        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            if (debug)
                std::cout << "Error in glUniform2f (loc=" << resolutionUniform
                          << "): 0x" << std::hex << err << std::dec << '\n';
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

    for (const DrawCommand &cmd : commandsData) {
        if (cmd.instanceCount == 0)
            continue;
        if (cmd.useScissor) {
            glEnable(GL_SCISSOR_TEST);
            glScissor(cmd.scissorBox.x, cmd.scissorBox.y, cmd.scissorBox.w,
                      cmd.scissorBox.h);

            GLenum err = glGetError();
            if (err != GL_NO_ERROR) {
                if (debug)
                    std::cout << "Error in glScissor (" << cmd.scissorBox.w << "x"
                              << cmd.scissorBox.h << "): 0x" << std::hex << err
                              << std::dec << '\n';
            }
        } else {
            glDisable(GL_SCISSOR_TEST);
        }

        // glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(cmd.instanceCount),
        //                GL_UNSIGNED_INT, (void *)(cmd.instanceOffset *
        //                sizeof(GLuint)));
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6,
                              static_cast<GLsizei>(cmd.instanceCount));

        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            if (debug)
                std::cout << "Error in glDrawElements: 0x" << std::hex << err
                          << std::dec << '\n';
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

bool VerticalContainer::UpdateLayout(UIStateTables &data, LayoutContext &context) {
    bool changed = false;

    if (childIds.empty()) {
        isDirty = false;
        return changed;
    }

    float targetX = 0, targetY = data.style[id].padding;
    float childLargestWidth = 0.0f;

    for (int childId : childIds) {
        GeometryStoreState &childData = data.geometry[childId];
        StyleStoreState &childstyle = data.style[childId];
        if (resizeChildren) {
            GeometryStoreState &myData = data.geometry[id];
            float bigger = std::max(childData.width, childData.height);
            auto size =
                ApplyStyle(bigger, bigger, myData.width, myData.height, childstyle);
            childData.width = size[0];
            childData.height = size[1];
        }
        childLargestWidth = std::max(childLargestWidth, childData.width);
    }

    if (fitContentWidth) {
        float fitWidth = childLargestWidth + 2 * data.style[id].padding;
        float oldWidth = data.geometry[id].width;

        data.geometry[id].width = std::max(oldWidth, fitWidth);

        if (data.geometry[id].width != oldWidth) {
            context.MarkParentChainDirty(id);
            changed = true;
        }
    }

    for (int childId : childIds) {
        if (centerHorizontally) {
            targetX =
                (data.geometry[id].width - data.geometry[childId].width) * 0.5f;
        } else {
            targetX = data.style[id].padding;
        }

        float oldChildX = data.geometry[childId].localX;
        float oldChildY = data.geometry[childId].localY;

        data.geometry[childId].localX = targetX;
        data.geometry[childId].localY = targetY;

        if (oldChildX != data.geometry[childId].localX ||
            oldChildY != data.geometry[childId].localY) {
            context.MarkDirty(childId);
            changed = true;
        }

        targetY += data.style[id].padding + data.geometry[childId].height;
    }

    if (fitContentHeight) {
        float oldHeight = data.geometry[id].height;
        data.geometry[id].height = targetY;
        if (data.geometry[id].height != oldHeight) {
            context.MarkParentChainDirty(id);
            changed = true;
        }
    }

    isDirty = false;
    return changed;
}

// goal: split UI manager
// give UpdateLayout access to *LayoutManager instead of
// the entire UIScene
// GOAL COMPLETELED!!!