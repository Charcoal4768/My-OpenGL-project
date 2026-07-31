#ifndef UI_ELEMENT_H
#define UI_ELEMENT_H

#include <vector>
#include <array>
#include <memory>
#include <glad/glad.h>
#include <cassert>
#include <openglBasics/shaderclass.h>
#include <openglBasics/VAO.h>
#include <openglBasics/VBO.h>
#include <openglBasics/EBO.h>

//DOES NOT WORK RN, MASSIVE REFACTOR CURRENTLY HAPPENING

constexpr float F_UNSET = -1.0f;
constexpr int I_UNSET = -1;
typedef std::vector<std::unique_ptr<UIElement>> pointerVector;

struct Vertex
{
    GLfloat pos[3];
    GLfloat color[4];
};

struct RectShape{
    std::array<Vertex, 4> localVertices;
    std::array<GLuint, 6> localIndices = {0, 1, 2, 0, 2, 3};
};

struct ScissorRect{
    GLint x, y;
    GLsizei w, h;
};

struct DrawCommand{
    size_t indexOffset = I_UNSET;
    GLsizei indexCount = I_UNSET;
    bool useScissor = false;
    ScissorRect scissorBox;
};

struct ElementGeometry {
    float x, y;
    float width, height;
    float r, g, b, a;
};

struct Color{
    float r, g, b, a;
    bool operator!=(const Color& other) const {
        return r != other.r || g != other.g || b != other.b || a != other.a;
    }
};

enum class ScreenLayoutMode {
    Mobile,   // Width < 600px
    Tablet,   // Width between 600px and 1024px
    Desktop   // Width > 1024px
};

enum class RenderOpType{
    DrawElement,
    PushScissor,
    PopScissor
};

struct RenderOp {
    RenderOpType type;
    int elementId;
};

struct RenderData
{
    const std::vector<Vertex>& vertices;
    const std::vector<GLuint>& indices;
    const std::vector<DrawCommand>& commands;
};

struct TraversalData
{
    std::vector<int> drawOrder;
    std::vector<RenderOp> displayList;
    std::vector<int> parentIdLookup;
    bool empty() const{
        return (
            drawOrder.empty() ||
            displayList.empty() ||
            parentIdLookup.empty()
        );
    }
};

struct GeometryView {
    const Vertex* verticesPtr;
    size_t vertexCount;
    const GLuint* indicesPtr;
    size_t indexCount;
};

struct GeometryStoreState{
    float localX = F_UNSET;
    float localY = F_UNSET;
    float width = F_UNSET;
    float height = F_UNSET;

    float absoluteX = F_UNSET;
    float absoluteY = F_UNSET;
    bool operator!=(const GeometryStoreState& other) const {
        return localX != other.localX || localY != other.localY ||
        width != other.width || height != other.height;
    } //abs comparison delibirately ignored
};

struct ConstraintStoreState{
    float minWidth = F_UNSET;
    float minHeight = F_UNSET;

    float maxWidth = F_UNSET;
    float maxHeight = F_UNSET;

    float prefferedWidthPercent = F_UNSET;
    float prefferedHeightPercent = F_UNSET;
};

struct UIStateTables {
    std::vector<GeometryStoreState> geometry;
    std::vector<ConstraintStoreState> constraints;
    std::vector<Color> colors;
};

//ui element dosent know who it is without a manager
class UIElement {
private:
    RectShape drawRect;
public:
    bool isDirty = true;
    bool fitContentHeight = false;
    bool fitContentWidth = false;
    bool clipChildren = false;

    int id = I_UNSET;
    int parentId = I_UNSET;

    float minWidth = F_UNSET;
    float maxWidth = F_UNSET;

    float minHeight = 9000.0f;
    float maxHeight = 9000.0f;

    float widthPercent = F_UNSET; //b/w 0.0f and 1.0f
    float heightPercent = F_UNSET;

    std::vector<int> childIds;

    virtual ~UIElement() = default;

    virtual GeometryView Draw(const UIStateTables& data);
    virtual void UpdateLayout(UIStateTables& data);
};

class UIScene {
private:
    pointerVector ptrStore;
    std::vector<int> drawOrder;
    std::vector<RenderOp> displayList;

    UIStateTables dataTables;

    bool hirearchyDirty = true;
    bool frameDataRebuild = true;
    int rootId = I_UNSET;

    void MarkParentChainDirty(int startingId);
    void CompileDisplayList(int elementId);

    std::vector<Vertex> globalVertices;
    std::vector<GLuint> globalIndices;
    std::vector<DrawCommand> drawCommands;

public:
    int defaultCapacity = 100; //for ptrStore

    float viewportWidth = F_UNSET;
    float viewportHeight = F_UNSET;

    void Init();
    void SetRoot(int id);
    void EditElementShape(int id, const GeometryStoreState& props, bool dirtyChain);
    void EditElementColor(int id, const Color& props, bool dirtyChain);
    void StepFrame(std::array<float, 2>& resolution);//first element will always be the root make it the size of resolution
    void AddChild(int parentId, int childId);
    void RemoveChild(int childId);
    void RebuildHierarchy();
    void SetRootViewport(float width, float height);

    UIElement* GetElement(int id) const;
    const GeometryStoreState& GetElementShape(int id) const;
    const ConstraintStoreState& GetElementConstraints(int id) const;
    const Color& GetElementColor(int id) const;
    
    float GetAbsoluteX(int id) const { return (id >= 0 && id < dataTables.geometry.size()) ? dataTables.geometry[id].absoluteX : 0.0f; }
    float GetAbsoluteY(int id) const { return (id >= 0 && id < dataTables.geometry.size()) ? dataTables.geometry[id].absoluteY : 0.0f; }
    void SetAbsoluteX(int id, float val) { if (id >= 0 && id < dataTables.geometry.size()) dataTables.geometry[id].absoluteX = val; }
    void SetAbsoluteY(int id, float val) { if (id >= 0 && id < dataTables.geometry.size()) dataTables.geometry[id].absoluteY = val; }

    float GetLocalX(int id) const { return dataTables.geometry[id].localX; }
    float GetLocalY(int id) const { return dataTables.geometry[id].localY; }
    float GetWidth(int id) const  { return dataTables.geometry[id].width; }
    float GetHeight(int id) const { return dataTables.geometry[id].height; }

    ScreenLayoutMode GetScreenLayoutMode() const;
    template <typename T>
    int AddElement(){
        auto element = std::make_unique<T>();
        int newID = static_cast<int>(ptrStore.size());
        element -> id = newID;
        ptrStore.push_back(std::move(element));

        GeometryStoreState elementShape;
        Color elementColor;

        elementShape.localX = F_UNSET;
        elementShape.localY = F_UNSET;
        elementShape.width = F_UNSET;
        elementShape.height = F_UNSET;

        elementShape.absoluteX = 0.0f;
        elementShape.absoluteY = 0.0f;

        elementColor.r = 1.0f;
        elementColor.g = 1.0f;
        elementColor.b = 1.0f;
        elementColor.a = 1.0f;

        dataTables.geometry.push_back(elementShape);
        dataTables.geometry.push_back(elementColor);
        return newID;
    }

    template <typename T>
    T& Get(int id){
        assert(id >= 0 && id < ptrStore.size());
        T* ptr = dynamic_cast<T*>(ptrStore[id].get());
        assert(ptr != nullptr);
        return *ptr;
    }
};

class Hierarchy{
private:
    TraversalData traversal;
    const pointerVector* currentElementReferences;
    int rootId = I_UNSET;
    void CompileDisplayList(int elementId);
public:
    bool hirearchyDirty = true;
    const TraversalData& RebuildTraversal(pointerVector& elementPointers);
    const TraversalData& ReturnCurrentTraversal() const;
    // access treversal without needing to rebuild
};

class LayoutEngine{
    //manage abs pos and layout
    //take ptrStore to chase pointers...
    //set abs positions from locals
    //communicate if something was found dirty
    //update layout of "dirty" element
private:
public:
};

class RenderBatcher{
    //prepare Verticies, indicies, cmd list
};

class Renderer{
    //only one aware of opengl
private:
    VAO MainVAO;
    VBO MainVBO;
    EBO MainEBO;
    Shader DefaultShader;
    GLint resolutionUniform = I_UNSET;
public:
    void Init();
    // void UploadData();
    void DrawFrame(const RenderData);
};

class AnchorElement : public UIElement {
public:
    GeometryView Draw(const UIStateTables& data) override;
};

class PaddedElement : public UIElement {
    public:
    float padding = 5.0f;
};

class UIRect : public PaddedElement {
public:
    // GeometryView Draw(const UIScene& manager) override;
};

class VerticalContainer : public PaddedElement {
public:
    bool centerHorizontally = true;
    bool resizeChildren = false;
    float r = 0.6f, g = 0.1f, b = 0.8f;
    // GeometryView Draw(const UIScene& manager) override;
    void UpdateLayout(UIStateTables& data) override;
};

#endif
