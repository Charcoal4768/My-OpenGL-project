#include <UI/UIElements.h>

#include <algorithm>
#include <vector>

std::array<float, 2> ApplyStyle(float computedWidth, float computedHeight,
                                float parentWidth, float parentHeight,
                                const StyleStoreState &style) {
    float preferredWidth =
        style.prefferedWidthPercent != F_UNSET
            ? style.prefferedWidthPercent * parentWidth
            : computedWidth;
    float preferredHeight =
        style.prefferedHeightPercent != F_UNSET
            ? style.prefferedHeightPercent * parentHeight
            : computedHeight;

    if (style.maxWidth != F_UNSET)
        preferredWidth = std::min(preferredWidth, style.maxWidth);
    if (style.maxHeight != F_UNSET)
        preferredHeight = std::min(preferredHeight, style.maxHeight);

    if (style.minWidth != F_UNSET)
        preferredWidth = std::max(preferredWidth, style.minWidth);
    if (style.minHeight != F_UNSET)
        preferredHeight = std::max(preferredHeight, style.minHeight);

    return {preferredWidth, preferredHeight};
}

std::vector<int> LayoutManager::Run(const TraversalData &traversal,
                                    pointerVector &elementPointers,
                                    UIStateTables &dataTables) {
    return RunInto(traversal, elementPointers, dataTables);
}

const std::vector<int> &LayoutManager::RunInto(
    const TraversalData &traversal, pointerVector &elementPointers,
    UIStateTables &dataTables) {
    dirtyElementIds.clear();
    if (elementPointers.empty())
        return dirtyElementIds;

    context.currentElementReferences = &elementPointers;
    context.parentIdLookup = &traversal.parentIdLookup;

    previous.resize(dataTables.geometry.size());
    dirtyMarks.resize(dataTables.geometry.size(), 0);
    dirtyElementIds.reserve(traversal.drawOrder.size());
    if (++dirtyGeneration == 0) {
        std::fill(dirtyMarks.begin(), dirtyMarks.end(), 0);
        dirtyGeneration = 1;
    }

    const auto markDirty = [this](int elementId) {
        const size_t index = static_cast<size_t>(elementId);
        if (dirtyMarks[index] != dirtyGeneration) {
            dirtyMarks[index] = dirtyGeneration;
            dirtyElementIds.push_back(elementId);
        }
    };

    bool hasDirtyElement = false;
    for (int elementId : traversal.drawOrder) {
        if (elementId < 0 ||
            static_cast<size_t>(elementId) >= elementPointers.size() ||
            !elementPointers[elementId]) {
            continue;
        }

        const GeometryStoreState &geometry = dataTables.geometry[elementId];
        previous[elementId] = {geometry.localX, geometry.localY, geometry.width,
                               geometry.height, geometry.absoluteX,
                               geometry.absoluteY};
        UIElement *element = elementPointers[elementId].get();
        if (element->isDirty) {
            hasDirtyElement = true;
            markDirty(elementId);
        }
    }

    if (!hasDirtyElement) {
        context.currentElementReferences = nullptr;
        context.parentIdLookup = nullptr;
        return dirtyElementIds;
    }

    for (auto it = traversal.drawOrder.rbegin(); it != traversal.drawOrder.rend();
         ++it) {
        const int elementId = *it;
        if (elementId < 0 ||
            static_cast<size_t>(elementId) >= elementPointers.size() ||
            !elementPointers[elementId]) {
            continue;
        }
        elementPointers[elementId]->MeasureLayout(dataTables, context);
    }

    for (int elementId : traversal.drawOrder) {
        if (elementId < 0 ||
            static_cast<size_t>(elementId) >= elementPointers.size() ||
            !elementPointers[elementId]) {
            continue;
        }

        UIElement *element = elementPointers[elementId].get();
        GeometryStoreState &geometry = dataTables.geometry[elementId];
        const int parentId =
            static_cast<size_t>(elementId) < traversal.parentIdLookup.size()
                ? traversal.parentIdLookup[elementId]
                : I_UNSET;

        float parentWidth = 800.0f;
        float parentHeight = 800.0f;
        if (parentId >= 0 &&
            static_cast<size_t>(parentId) < dataTables.geometry.size()) {
            const GeometryStoreState &parent = dataTables.geometry[parentId];
            parentWidth = parent.width;
            parentHeight = parent.height;
        }

        const auto finalSize =
            ApplyStyle(geometry.measuredWidth, geometry.measuredHeight,
                       parentWidth, parentHeight, dataTables.style[elementId]);
        geometry.width = finalSize[0];
        geometry.height = finalSize[1];

        if (parentId >= 0 &&
            static_cast<size_t>(parentId) < dataTables.geometry.size()) {
            const GeometryStoreState &parent = dataTables.geometry[parentId];
            geometry.absoluteX = parent.absoluteX + geometry.localX;
            geometry.absoluteY = parent.absoluteY + geometry.localY;
        } else {
            geometry.absoluteX = geometry.localX;
            geometry.absoluteY = geometry.localY;
        }

        element->ArrangeLayout(dataTables, context);
        element->isDirty = false;
    }

    for (int elementId : traversal.drawOrder) {
        if (elementId < 0 ||
            static_cast<size_t>(elementId) >= dataTables.geometry.size()) {
            continue;
        }
        const GeometryStoreState &geometry = dataTables.geometry[elementId];
        const LayoutSnapshot &old = previous[elementId];
        if (geometry.localX != old.localX || geometry.localY != old.localY ||
            geometry.width != old.width || geometry.height != old.height ||
            geometry.absoluteX != old.absoluteX ||
            geometry.absoluteY != old.absoluteY) {
            markDirty(elementId);
        }
    }

    context.currentElementReferences = nullptr;
    context.parentIdLookup = nullptr;
    return dirtyElementIds;
}

bool LayoutContext::IsChildOf(int childId, int parentId) const {
    assert(currentElementReferences != nullptr);
    assert(parentId >= 0 && parentId < parentIdLookup->size());
    assert(childId >= 0 && childId < currentElementReferences->size());

    if (childId == parentId)
        return true;

    int currentParent = (*parentIdLookup)[childId];
    while (currentParent != I_UNSET) {
        if (currentParent == parentId)
            return true;
        currentParent = (*parentIdLookup)[currentParent];
    }
    return false;
}

void LayoutContext::MarkDirty(int id, int callerId) {
    assert(currentElementReferences != nullptr);
    assert(id == callerId || IsChildOf(id, callerId));
    if (id == I_UNSET || id >= currentElementReferences->size())
        return;

    UIElement *element = (*currentElementReferences)[id].get();
    if (element)
        element->isDirty = true;
}

void LayoutContext::MarkParentChainDirty(int targetId, int callerId) {
    assert(currentElementReferences != nullptr);
    assert(targetId == callerId || IsChildOf(targetId, callerId));

    int currentId = targetId;
    while (currentId != I_UNSET &&
           currentId < static_cast<int>(currentElementReferences->size())) {
        UIElement *element = (*currentElementReferences)[currentId].get();
        if (!element)
            break;

        element->isDirty = true;
        currentId = element->parentId;
    }
}

void UIElement::MeasureLayout(UIStateTables &data, LayoutContext &) {
    if (id == I_UNSET || static_cast<size_t>(id) >= data.geometry.size())
        return;

    GeometryStoreState &geometry = data.geometry[id];
    geometry.measuredWidth = geometry.width;
    geometry.measuredHeight = geometry.height;
}

void UIElement::ArrangeLayout(UIStateTables &, LayoutContext &) {}

std::array<float, 2> ContainerElement::ResolveChildSize(
    float computedWidth, float computedHeight, float parentWidth,
    float parentHeight, const StyleStoreState &childStyle) const {
    return ApplyStyle(computedWidth, computedHeight, parentWidth, parentHeight,
                      childStyle);
}

void VerticalContainer::MeasureLayout(UIStateTables &data,
                                      LayoutContext &context) {
    UIElement::MeasureLayout(data, context);
    if (childIds.empty())
        return;

    GeometryStoreState &geometry = data.geometry[id];
    const float padding = data.style[id].padding;
    float childLargestWidth = 0.0f;
    float contentHeight = (childIds.size() + 1) * padding;

    for (int childId : childIds) {
        const GeometryStoreState &child = data.geometry[childId];
        childLargestWidth = std::max(childLargestWidth, child.measuredWidth);
        contentHeight += child.measuredHeight;
    }

    if (fitContentWidth) {
        geometry.measuredWidth =
            std::max(geometry.width, childLargestWidth + 2.0f * padding);
    }
    if (fitContentHeight)
        geometry.measuredHeight = contentHeight;
}

void VerticalContainer::ArrangeLayout(UIStateTables &data, LayoutContext &) {
    if (childIds.empty())
        return;

    GeometryStoreState &container = data.geometry[id];
    const float padding = data.style[id].padding;
    float targetY = padding;

    for (int childId : childIds) {
        GeometryStoreState &child = data.geometry[childId];
        const StyleStoreState &childStyle = data.style[childId];
        float targetWidth = child.measuredWidth;
        float targetHeight = child.measuredHeight;

        if (resizeChildren) {
            const float bigger = std::max(targetWidth, targetHeight);
            const auto size = ResolveChildSize(
                bigger, bigger, container.width, container.height, childStyle);
            child.measuredWidth = targetWidth = size[0];
            child.measuredHeight = targetHeight = size[1];
        } else {
            const auto size = ResolveChildSize(
                targetWidth, targetHeight, container.width, container.height,
                childStyle);
            targetWidth = size[0];
            targetHeight = size[1];
        }

        child.width = targetWidth;
        child.height = targetHeight;
        child.localX = centerHorizontally
                           ? (container.width - targetWidth) * 0.5f
                           : padding;
        child.localY = targetY;
        targetY += padding + targetHeight;
    }
}
