#include <UI/UIElements.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <iostream>

void UIScene::SetRootViewport(float width, float height){
    viewportWidth = width;
    viewportHeight = height;
//we just store the new size data
//we let StepFrame handle the checks
//and comparisons and setting bools and stuff
//this old stuff:
// if (!ptrStore.empty() && ptrStore[rootId]) { //sync UI root to window size
//     GeometryStoreState& rootShape = dataTables.geometry[rootId];
//     if (rootShape.width != newWidth || rootShape.height != newHeight) {
//         rootShape.width = newWidth;
//         rootShape.height = newHeight;
//         ptrStore[rootId]->isDirty = true; 
//         frameDataRebuild = true;
//     }
// }
}

std::array<float,2> ApplyStyle(float computedWidth, float computedHeight, 
float parentWidth, float parentHeight, const StyleStoreState& style){
    //we want final size to be basically css clamp() type stuff
    //preferredWidth/Height are percentages: 0.0 -> 1.0
    //also, either Percentages or absolute pixel sizing, both = bug in the app using this framework
    float prefferedWidth;
    float prefferedHeight;

    prefferedWidth = (style.prefferedWidthPercent > F_UNSET) ? (style.prefferedWidthPercent * parentWidth) : computedWidth;
    prefferedHeight = (style.prefferedHeightPercent > F_UNSET) ? (style.prefferedHeightPercent * parentHeight) : computedHeight;

    if (style.maxWidth >= F_UNSET) prefferedWidth = std::min(prefferedWidth, style.maxWidth);
    if (style.maxHeight >= F_UNSET) prefferedHeight = std::min(prefferedHeight, style.maxHeight);

    float finalWidth = prefferedWidth;

    if (style.minWidth != F_UNSET)
        finalWidth = std::max(finalWidth, style.minWidth);

    float finalHeight = prefferedHeight;

    if (style.minHeight != F_UNSET)
        finalHeight = std::max(finalHeight, style.minHeight);

    return {finalWidth, finalHeight};
}

bool LayoutManager::Run(const TraversalData& traversal, 
pointerVector& elementPointers, UIStateTables& dataTables){
    bool rebuildRender = false;
    for (int elementId : traversal.drawOrder)
    {
        int parentId = traversal.parentIdLookup[elementId];
        if (parentId == I_UNSET) continue;
        UIElement* el = elementPointers[elementId].get();
        if (!el) continue;
        GeometryStoreState& elementShape = dataTables.geometry[elementId];
        GeometryStoreState& parentShape = dataTables.geometry[parentId];
        elementShape.absoluteX = parentShape.absoluteX + elementShape.localX;
        elementShape.absoluteY = parentShape.absoluteY + elementShape.localY;
        if (el->isDirty)
        {
            el->UpdateLayout(dataTables);
            rebuildRender = true; //render commands need to be rebuilt
        }        
    }
    return rebuildRender;
}

ScissorRect IntersectRects(const ScissorRect& a, const ScissorRect& b){
    GLint x1 = std::max(a.x, b.x);
    GLint y1 = std::max(a.y, b.y);
    GLint x2 = std::min(a.x + a.w, b.x + b.w);
    GLint y2 = std::min(a.y + a.h, b.y + b.h);

    if (x2 < x1 || y2 < y1) {
        return {0,0,0,0};
    }
    return {x1, y1, x2 - x1, y2 - y1};
}

void UIScene::SetRoot(int id){
    UIElement* ptr = ptrStore[id].get();
    assert(ptr != nullptr);//checks if AddElement was even called for this, if it was then id automatically is valid and > -1
    assert(ptr->parentId == -1); //root cannot have a parent
    rootId = id;
}

void UIScene::EditElementShape(int id, const GeometryStoreState& newShape, bool dirtyChain){
    assert(id >= 0);
    assert(id < ptrStore.size());
    assert(ptrStore[id] != nullptr);
    UIElement* el = ptrStore[id].get();

    GeometryStoreState& elementShape = dataTables.geometry[id];

    float oldAbsX = elementShape.absoluteX;
    float oldAbsY = elementShape.absoluteY;

    bool positionOrSizeChanged = (elementShape != newShape);

    if (!positionOrSizeChanged) return;

    elementShape = newShape;
    elementShape.absoluteX = oldAbsX; elementShape.absoluteY = oldAbsY;

    if (dirtyChain) {
        el->isDirty = true;
        if (el->parentId != -1 && ptrStore[el->parentId]) {
            ptrStore[el->parentId]->isDirty = true; // Mark parent dirty
        }
    }
}

void UIScene::EditElementColor(int id, const Color& newColor, bool dirtyChain){
    assert(id >= 0);
    assert(id < ptrStore.size());
    assert(ptrStore[id] != nullptr);

    UIElement* el = ptrStore[id].get();

    Color& elementColor = dataTables.style[id].color;

    bool colorChanged = (elementColor != newColor);

    if (!colorChanged) return;

    elementColor = newColor;

    if (dirtyChain && colorChanged) {
        el->isDirty = true;
        if (el->parentId != -1 && ptrStore[el->parentId]) {
            ptrStore[el->parentId]->isDirty = true; // Mark parent dirty
        }
    }
}

const GeometryStoreState& UIScene::GetElementShape(int id) const
{
    assert(id >= 0);
    assert(id < dataTables.geometry.size());

    return dataTables.geometry[id];
}

const StyleStoreState& UIScene::GetElementStyle(int id) const{
    assert(id >= 0);
    assert(id < dataTables.style.size());

    return dataTables.style[id];
}

const TraversalData& Hierarchy::RebuildTraversal(pointerVector& elementPointers){
    //needs access to: ptrStore, rootId
    if (!hirearchyDirty) return traversal;

    traversal.displayList.clear();
    traversal.drawOrder.clear();
    traversal.parentIdLookup.clear();
    currentElementReferences = &elementPointers;

    if (currentElementReferences == nullptr) return traversal;
    
    size_t targetCapacity = currentElementReferences->size() * 3 / 2;
    
    if (!currentElementReferences->empty() && rootId < currentElementReferences->size() && (*currentElementReferences)[rootId]){
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

const TraversalData& Hierarchy::ReturnCurrentTraversal() const{
    //redundant function but... good idea maybe
    if (traversal.empty())
    {
        std::cout << "WARNING: Traversal Data has not yet been generated but was still requested." << std::endl;
    }
    return traversal;
}

void Hierarchy::RecursiveDownTraversal(int elementId){
    //expects traversal vectors to be fully empty
    //ill change it to not assume that in the future
    assert(currentElementReferences != nullptr);

    if (elementId == I_UNSET || elementId >= currentElementReferences->size()) return;

    UIElement* el = (*currentElementReferences)[elementId].get();

    if (!el) return;

    bool useScissor = el->clipChildren;
    int parentId = el->parentId;

    if (parentId == I_UNSET) return;

    if(useScissor) {
        traversal.displayList.push_back({RenderOpType::PushScissor, elementId});
    }

    traversal.displayList.push_back({RenderOpType::DrawElement, elementId});
    traversal.drawOrder.emplace_back(elementId);
    if (parentId!=I_UNSET)
    {
        //index = element id, value = parent id
        //sanity checks:
        //elementId should exist in drawOrder | Done
        //elementId should not be I_UNSET | Done
        //Delete Function (to be implimented) will handle
        //setting parentIdLookup[deletedId] = I_UNSET; | Pending
        //every subsystem already ignores orphaned elements | Done
        if (elementId >= traversal.parentIdLookup.size())
        {
            traversal.parentIdLookup.resize(elementId+1, I_UNSET);
        }
    }

    for (int childId : el->childIds){
        RecursiveDownTraversal(childId);
    }

    if (useScissor){
        traversal.displayList.push_back({RenderOpType::PopScissor, elementId});
    }
    
}

void LayoutContext::MarkParentChainDirty(int elementId){
    assert(currentElementReferences != nullptr);

    if (elementId == I_UNSET || elementId >= currentElementReferences->size()) return;

    UIElement* el = (*currentElementReferences)[elementId].get();

    if (!el) return;

    int parentId = el->parentId;
    el->isDirty=true;

    if (parentId == I_UNSET) return;

    MarkParentChainDirty(parentId);
}

void UIScene::AddChild(int parentId, int childId) {
    if (parentId < 0 || parentId >= ptrStore.size() || !ptrStore[parentId]) return;
    if (childId < 0 || childId >= ptrStore.size() || !ptrStore[childId]) return;

    UIElement* parent = ptrStore[parentId].get();
    UIElement* child = ptrStore[childId].get();

    parent->childIds.push_back(child->id);
    child->parentId = parent->id;
    parent->isDirty = true;
    hirearchyDirty = true; //gonna make this something that takes
    //element ID which triggered rebuild
    //into account
}

void UIScene::RemoveChild(int childId) {
    if (childId < 0 || childId >= ptrStore.size() || !ptrStore[childId]) return;
    UIElement* child = ptrStore[childId].get();
    int parentId = child->parentId;
    if (parentId < 0 || parentId >= ptrStore.size() || !ptrStore[parentId]) return;
    UIElement* parent = ptrStore[childId].get();
    std::vector<int>& childIds = parent->childIds;
    auto it = std::find(childIds.begin(), childIds.end(), childId);

    if (it != childIds.end()){
        int index = it - childIds.begin();
        std::swap(childIds[index], childIds.back());
        childIds.pop_back();
    }
    hirearchyDirty = true;
}

void UIElement::UpdateLayout(UIStateTables& data) {
    isDirty = false;
}

ScreenLayoutMode UIScene::GetScreenLayoutMode() const{
    if (dataTables.geometry[rootId].width < 600.0f) return ScreenLayoutMode::Mobile;
    return ScreenLayoutMode::Desktop;
}

void UIScene::StepFrame(std::array<float,2>& resolution){

}

static void AppendQuad(
    RenderData& frame,
    const GeometryStoreState& geometry,
    const Color& color)
{
    GLuint baseVertex = static_cast<GLuint>(frame.vertices.size());

    float x = geometry.absoluteX;
    float y = geometry.absoluteY;
    float w = geometry.width;
    float h = geometry.height;

    frame.vertices.push_back({{x,     y,     0.0f}, {color.r,color.g,color.b,color.a}});
    frame.vertices.push_back({{x + w, y,     0.0f}, {color.r,color.g,color.b,color.a}});
    frame.vertices.push_back({{x + w, y + h, 0.0f}, {color.r,color.g,color.b,color.a}});
    frame.vertices.push_back({{x,     y + h, 0.0f}, {color.r,color.g,color.b,color.a}});

    constexpr GLuint quadIndices[6] = {0,1,2,0,2,3};

    for (GLuint index : quadIndices)
    {
        frame.indices.push_back(baseVertex + index);
    }
}

static ScissorRect BuildScissorRect(
    const GeometryStoreState& geometry,
    float viewportHeight)
{
    return {
        static_cast<GLint>(geometry.absoluteX),
        static_cast<GLint>(viewportHeight - (geometry.absoluteY + geometry.height)),
        static_cast<GLsizei>(geometry.width),
        static_cast<GLsizei>(geometry.height)
    };
}

const RenderData& RenderBatcher::ReBuildFrameData(UIStateTables& dataTables, const std::vector<RenderOp>& displayList, float viewportHeight){
    FrameData.vertices.clear();
    FrameData.indices.clear();
    FrameData.commands.clear();

    DrawCommand currentBatch; //we make a new batch
    currentBatch.indexOffset = 0;
    currentBatch.indexCount = 0;

    std::vector<ScissorRect> scissorRects;

    for (const RenderOp& op : displayList)
    {
        switch (op.type){
            case RenderOpType::PushScissor:{
                if (currentBatch.indexCount > 0){
                    FrameData.commands.push_back(currentBatch);
                    currentBatch.indexOffset = FrameData.indices.size();
                    currentBatch.indexCount = 0;
                }

                ScissorRect rect = BuildScissorRect(
                    dataTables.geometry[op.elementId],
                    viewportHeight);

                if (!scissorRects.empty()){
                    rect = IntersectRects(scissorRects.back(), rect);
                }

                scissorRects.push_back(rect);

                currentBatch.useScissor = true;
                currentBatch.scissorBox = rect;
                
                break;
            }
            
            case RenderOpType::PopScissor:{
                if (currentBatch.indexCount > 0){
                    FrameData.commands.push_back(currentBatch);
                    currentBatch.indexOffset = FrameData.indices.size();
                    currentBatch.indexCount = 0;
                }

                scissorRects.pop_back();

                if (scissorRects.empty()){
                    currentBatch.useScissor = false;
                }
                else{
                    currentBatch.useScissor = true;
                    currentBatch.scissorBox = scissorRects.back();
                }
                break;
            }

            case RenderOpType::DrawElement:{
                const GeometryStoreState& geometry = dataTables.geometry[op.elementId];
                const StyleStoreState& style = dataTables.style[op.elementId];
                if (style.hidden) break;
                AppendQuad(FrameData, geometry, style.color);
                currentBatch.indexCount += 6;
                break;
            }
            
            default:
                break;
        }
    }

    if (currentBatch.indexCount > 0)
    {
        FrameData.commands.push_back(currentBatch);
    }
    return FrameData;
}

const RenderData& RenderBatcher::GetFrameData() const{
    return FrameData;
}

void Renderer::Init(){
    if (initialized) return;
    DefaultShader.Load("default.vert", "default.frag");
    MainVAO.Bind();
    MainVBO.Bind();
    MainEBO.Bind();

    //set layouts 1 and 0 on VAO
    MainVAO.LinkAttrib(MainVBO, VertexLayout);
    MainVAO.LinkAttrib(MainVBO, FragmentLayout);

    MainVAO.Unbind();
    MainVBO.Unbind();
    MainEBO.Unbind();
    initialized = true;
}

void VerticalContainer::UpdateLayout(UIStateTables& data){
    if (childIds.empty()) {
        isDirty = false;
        return;
    }

    GeometryStoreState& myData = data.geometry[id];

    float targetX = 0, targetY = padding;
    float childLargestWidth = 0.0f;

    for (int childId : childIds){
        GeometryStoreState& childData = data.geometry[childId];
        StyleStoreState& childstyle = data.style[childId];
        if (resizeChildren){
            float bigger = std::max(childData.width, childData.height);
            auto size = ApplyStyle(bigger, bigger, myData.width, myData.height, childstyle);
            childData.width = size[0];
            childData.height = size[1];
        }
        childLargestWidth = std::max(childLargestWidth, childData.width);
    }

    if (fitContentWidth){
        float fitWidth = childLargestWidth + 2 * padding;
        myData.width = std::max(myData.width, fitWidth);
        UIScene->EditElement(this->id, myData, false);
    }

    for (int childId : childIds){
        GeometryStoreState& childData = data.geometry[childId];
        if(centerHorizontally){
            targetX = (myData.width - childData.width) * 0.5f;
        } else{
            targetX = padding;
        }

        childData.x = targetX;
        childData.y = targetY;

        UIScene->EditElement(childId, childData, false);

        targetY += padding + childData.height;
    }

    if (fitContentHeight){
        myData.height = targetY;
        UIScene->EditElement(this->id, myData, false);
    }
    
    isDirty = false;
}
//goal: split UI manager
//give UpdateLayout access to *LayoutManager instead of
//the entire UIScene
//