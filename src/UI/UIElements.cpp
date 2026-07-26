#include <UI/UIElements.h>
#include <vector>
#include <algorithm>
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

std::array<float,2> ApplyConstraints(float computedWidth, float computedHeight, 
float parentWidth, float parentHeight, const ConstraintStoreState& constraints){
    //we want final size to be basically css clamp() type stuff
    //preferredWidth/Height are percentages: 0.0 -> 1.0
    //also, either Percentages or absolute pixel sizing, both = bug in the app using this framework
    float prefferedWidth;
    float prefferedHeight;

    prefferedWidth = (constraints.prefferedWidthPercent > F_UNSET) ? (constraints.prefferedWidthPercent * parentWidth) : computedWidth;
    prefferedHeight = (constraints.prefferedHeightPercent > F_UNSET) ? (constraints.prefferedHeightPercent * parentHeight) : computedHeight;

    if (constraints.maxWidth >= F_UNSET) prefferedWidth = std::min(prefferedWidth, constraints.maxWidth);
    if (constraints.maxHeight >= F_UNSET) prefferedHeight = std::min(prefferedHeight, constraints.maxHeight);

    float finalWidth = prefferedWidth;

    if (constraints.minWidth != F_UNSET)
        finalWidth = std::max(finalWidth, constraints.minWidth);

    float finalHeight = prefferedHeight;

    if (constraints.minHeight != F_UNSET)
        finalHeight = std::max(finalHeight, constraints.minHeight);

    return {finalWidth, finalHeight};
}

bool LayoutPass(const std::vector<int>& drawOrder){
    for (int elementId : drawOrder) { //use drawOrder made by traversal pass
        if (!ptrStore[elementId]) continue;
        float parentAbsX = 0.0f;
        float parentAbsY = 0.0f;
        int pId = ptrStore[elementId]->parentId;
        GeometryStoreState& elementShape = dataTables.geometry[elementId];
        //{ compute absolute pos
        if (pId != -1) {
            GeometryStoreState& parentShape = dataTables.geometry[pId];
            parentAbsX = parentShape.absoluteX;
            parentAbsY = parentShape.absoluteY;
        }
        //}
        elementShape.absoluteX = elementShape.localX + parentAbsX;
        elementShape.absoluteY = elementShape.localY + parentAbsY;
        if (ptrStore[elementId]->isDirty) { //actual layout update
            ptrStore[elementId]->UpdateLayout(dataTables);
            frameDataRebuild = true; //flag to say rebuild frame
            // we will make this function
            //a bool and based on its output, StepFrame will
            //set frameDataRebuild
            return true;
        }
        return false;
    }
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

    Color& elementColor = dataTables.colors[id];

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

const ConstraintStoreState& UIScene::GetElementConstraints(int id) const{
    assert(id >= 0);
    assert(id < dataTables.constraints.size());

    return dataTables.constraints[id];
}

const Color& UIScene::GetElementColor(int id) const {
    assert(id >= 0);
    assert(id < dataTables.colors.size());

    return dataTables.colors[id];
}

void UIScene::RebuildHierarchy(){
    if (!hirearchyDirty) return;

    displayList.clear();

    if (!ptrStore.empty() && ptrStore[rootId]){
        drawOrder.clear();
        drawOrder.reserve(ptrStore.size());
        CompileDisplayList(rootId);
    }

    hirearchyDirty = false;   
}

void UIScene::CompileDisplayList(int elementId){
    UIElement* el = ptrStore[elementId].get();

    if (!el) return;

    bool useScissor = el->clipChildren;

    if(useScissor) {
        displayList.push_back({RenderOpType::PushScissor, elementId});
    }

    displayList.push_back({RenderOpType::DrawElement, elementId});
    drawOrder.emplace_back(elementId);

    for (int childId : el->childIds){
        CompileDisplayList(childId);
    }

    if (useScissor){
        displayList.push_back({RenderOpType::PopScissor, elementId});
    }
    
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

    if (hirearchyDirty) {
        RebuildHierarchy(); //update treversal
    }

    bool frameDataRebuild = false;

    // if (!ptrStore.empty() && ptrStore[rootId]) { //sync UI root to window size
    //     float newWidth  = resolution[0];
    //     float newHeight = resolution[1];
    //     GeometryStoreState& rootShape = dataTables.geometry[rootId];

    //     if (rootShape.width != newWidth || rootShape.height != newHeight) {
    //         rootShape.width = newWidth;
    //         rootShape.height = newHeight;
    //         ptrStore[rootId]->isDirty = true; 
    //         frameDataRebuild = true;
    //     }
    // }

    size_t elementCount = drawOrder.size(); //need to fetch this from traversal
    //need to fetch displayList too

    if (elementCount > 0){//layout pass
        //layout stuff will happen here
    }

    if (frameDataRebuild) { //renderBatcher == this block
        globalVertices.clear();
        globalIndices.clear(); //reset all buffers
        drawCommands.clear();

        DrawCommand currentBatch;
        currentBatch.indexOffset = 0; //we make a new batch
        currentBatch.indexCount = 0;

        std::vector<ScissorRect> scissorRects;

        for (const RenderOp& op : displayList){ //unpack the displayList
            if (op.type == RenderOpType::PushScissor){
                if (currentBatch.indexCount > 0){
                    drawCommands.push_back(currentBatch);
                    currentBatch.indexOffset = globalIndices.size();
                    currentBatch.indexCount = 0;
                }

                float windowHeight = GetHeight(rootId);
                float elementWdith = GetWidth(op.elementId);
                float elementHeight = GetHeight(op.elementId);
                float elementY = GetAbsoluteY(op.elementId);
                float elementX = GetAbsoluteX(op.elementId);
                ScissorRect newRect = {
                    static_cast<GLint>(elementX),
                    static_cast<GLint>(windowHeight - (elementY + elementHeight)),
                    static_cast<GLsizei>(elementWdith),
                    static_cast<GLsizei>(elementHeight),
                };
                
                if (!scissorRects.empty()){
                    newRect = IntersectRects(scissorRects.back(), newRect);
                }

                scissorRects.push_back(newRect);
                currentBatch.useScissor = true;
                currentBatch.scissorBox = newRect;
            }

            else if (op.type == RenderOpType::PopScissor){
                if (currentBatch.indexCount > 0){
                    drawCommands.push_back(currentBatch);
                    currentBatch.indexOffset = globalIndices.size();
                    currentBatch.indexCount = 0;  
                }

                scissorRects.pop_back();

                //try to use parent scissor
                //or if no scissor Rects remain, turn off the flag
                if (scissorRects.empty()){
                    currentBatch.useScissor = false;
                } else {
                    currentBatch.useScissor = true;
                    currentBatch.scissorBox = scissorRects.back();
                }
            }

            else if (op.type == RenderOpType::DrawElement){
                UIElement* element = ptrStore[op.elementId].get();
                if (element){
                    GeometryView view = element->Draw(dataTables);//get element geometry
                    GLuint baseVertexOffset = static_cast<GLuint>(globalVertices.size());
                    
                    //put geometry into appropriate group
                    globalVertices.insert(globalVertices.end(), view.verticesPtr, view.verticesPtr + view.vertexCount);

                    for (size_t i = 0; i < view.indexCount; i++) {
                        globalIndices.push_back(baseVertexOffset + view.indicesPtr[i]);
                    }
                    currentBatch.indexCount += view.indexCount;//batch book-keep
                }
            }
        }
        if (currentBatch.indexCount > 0) {
            drawCommands.push_back(currentBatch);
        }
    }
}

GeometryView UIElement::Draw(const UIStateTables& dataTables){
    GeometryStoreState elementShape = dataTables.geometry[id];
    Color col = dataTables.colors[id];

    float absX = elementShape.absoluteX; float absY = elementShape.absoluteY;
    float width = elementShape.width; float height = elementShape.height;

    drawRect.localVertices[0] = {{absX,         absY,         0.0f},{col.r,col.g,col.b,col.a}};
    drawRect.localVertices[1] = {{absX + width, absY,         0.0f},{col.r,col.g,col.b,col.a}};
    drawRect.localVertices[2] = {{absX + width, absY + width, 0.0f},{col.r,col.g,col.b,col.a}};
    drawRect.localVertices[3] = {{absX,         absY + width, 0.0f},{col.r,col.g,col.b,col.a}};

    return { drawRect.localVertices.data(), drawRect.localVertices.size(), drawRect.localIndices.data(), drawRect.localIndices.size() };
}

GeometryView AnchorElement::Draw(const UIStateTables& dataTables) {
    return { nullptr, 0, nullptr, 0 };
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
        ConstraintStoreState& childConstraints = data.constraints[childId];
        if (resizeChildren){
            float bigger = std::max(childData.width, childData.height);
            auto size = ApplyConstraints(bigger, bigger, myData.width, myData.height, childConstraints);
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