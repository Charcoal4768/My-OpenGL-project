#ifndef UI_ELEMENT_H
#define UI_ELEMENT_H

#include <array>
#include <cassert>
#include <glad/glad.h>
#include <memory>
#include <openglBasics/EBO.h>
#include <openglBasics/VAO.h>
#include <openglBasics/VBO.h>
#include <openglBasics/shaderclass.h>
#include <vector>

// DOES NOT WORK RN, MASSIVE REFACTOR CURRENTLY HAPPENING

constexpr float F_UNSET = -1.0f;
constexpr int I_UNSET = -1;
typedef std::vector<std::unique_ptr<UIElement>> pointerVector;

struct Vertex {
    GLfloat pos[3];
    GLfloat color[4];
};

struct RectShape {
    std::array<Vertex, 4> localVertices;
    std::array<GLuint, 6> localIndices = {0, 1, 2, 0, 2, 3};
};

struct ScissorRect {
    GLint x, y;
    GLsizei w, h;
};

struct DrawCommand {
    size_t indexOffset = I_UNSET;
    GLsizei indexCount = I_UNSET;
    bool useScissor = false;
    ScissorRect scissorBox;
};

struct Color {
    float r, g, b, a;
    bool operator!=(const Color &other) const {
        return r != other.r || g != other.g || b != other.b || a != other.a;
    }
};

enum class ScreenLayoutMode {
    Mobile, // Width < 600px
    Tablet, // Width between 600px and 1024px
    Desktop // Width > 1024px
};

enum class RenderOpType { DrawElement, PushScissor, PopScissor };

struct RenderOp {
    RenderOpType type;
    int elementId;
};

struct RenderData {
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    std::vector<DrawCommand> commands;
    bool empty() const {
        return (vertices.empty() || indices.empty() || commands.empty());
    }
};

struct TraversalData {
    std::vector<int> drawOrder;
    std::vector<RenderOp> displayList;
    std::vector<int> parentIdLookup;
    bool empty() const {
        return (drawOrder.empty() || displayList.empty() ||
                parentIdLookup.empty());
    }
};

struct GeometryStoreState {
    float localX = F_UNSET;
    float localY = F_UNSET;
    float width = F_UNSET;
    float height = F_UNSET;

    float absoluteX = F_UNSET;
    float absoluteY = F_UNSET;
    bool operator!=(const GeometryStoreState &other) const {
        return localX != other.localX || localY != other.localY ||
               width != other.width || height != other.height;
    } // abs comparison delibirately ignored
};

struct StyleStoreState {
    float minWidth = F_UNSET;
    float minHeight = F_UNSET;

    float maxWidth = F_UNSET;
    float maxHeight = F_UNSET;

    float prefferedWidthPercent = F_UNSET;
    float prefferedHeightPercent = F_UNSET;

    bool hidden = false;

    Color color;
};

struct UIStateTables {
    std::vector<GeometryStoreState> geometry;
    std::vector<StyleStoreState> style;
};

// ui element dosent know who it is without a manager
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

    float widthPercent = F_UNSET; // b/w 0.0f and 1.0f
    float heightPercent = F_UNSET;

    float padding = 5.0f;

    std::vector<int> childIds;

    virtual ~UIElement() = default;

    virtual void UpdateLayout(UIStateTables &data, LayoutContext &context);
};

class UIScene {
  private:
    pointerVector ptrStore;
    UIStateTables dataTables;

    bool frameDataRebuild = true;
    int rootId = I_UNSET;

    Hierarchy MainHierarchy;
    LayoutManager MainLayout;
    RenderBatcher MainBatcher;
    Renderer MainRenderer;

  public:
    int defaultCapacity = 100; // for ptrStore

    float viewportWidth = F_UNSET;
    float viewportHeight = F_UNSET;

    void Init();
    void SetRoot(int id);
    void EditElementShape(int id, const GeometryStoreState &props,
                          bool dirtyChain);
    void EditElementColor(int id, const Color &props, bool dirtyChain);
    void StepFrame(std::array<float, 2> &resolution);
    void AddChild(int parentId, int childId);
    void RemoveChild(int childId);
    void SetRootViewport(float width, float height);

    // UIElement* GetElement(int id) const;
    const GeometryStoreState &GetElementShape(int id) const;
    const StyleStoreState &GetElementStyle(int id) const;

    float GetAbsoluteX(int id) const {
        return (id >= 0 && id < dataTables.geometry.size())
                   ? dataTables.geometry[id].absoluteX
                   : 0.0f;
    }
    float GetAbsoluteY(int id) const {
        return (id >= 0 && id < dataTables.geometry.size())
                   ? dataTables.geometry[id].absoluteY
                   : 0.0f;
    }
    void SetAbsoluteX(int id, float val) {
        if (id >= 0 && id < dataTables.geometry.size())
            dataTables.geometry[id].absoluteX = val;
    }
    void SetAbsoluteY(int id, float val) {
        if (id >= 0 && id < dataTables.geometry.size())
            dataTables.geometry[id].absoluteY = val;
    }

    float GetLocalX(int id) const { return dataTables.geometry[id].localX; }
    float GetLocalY(int id) const { return dataTables.geometry[id].localY; }
    float GetWidth(int id) const { return dataTables.geometry[id].width; }
    float GetHeight(int id) const { return dataTables.geometry[id].height; }

    ScreenLayoutMode GetScreenLayoutMode() const;
    template <typename T> int AddElement() {
        auto element = std::make_unique<T>();
        int newID = static_cast<int>(ptrStore.size());
        element->id = newID;
        ptrStore.push_back(std::move(element));

        GeometryStoreState elementShape;
        StyleStoreState elementAppearance;

        elementShape.localX = F_UNSET;
        elementShape.localY = F_UNSET;
        elementShape.width = F_UNSET;
        elementShape.height = F_UNSET;

        elementShape.absoluteX = 0.0f;
        elementShape.absoluteY = 0.0f;

        elementAppearance.color.r = 1.0f;
        elementAppearance.color.g = 1.0f;
        elementAppearance.color.b = 1.0f;
        elementAppearance.color.a = 1.0f;
        elementAppearance.hidden = false;
        elementAppearance.maxHeight = F_UNSET;
        elementAppearance.maxWidth = F_UNSET;
        elementAppearance.minHeight = F_UNSET;
        elementAppearance.minWidth = F_UNSET;

        dataTables.geometry.push_back(elementShape);
        dataTables.style.push_back(elementColor);
        return newID;
    }

    template <typename T> T &Get(int id) {
        assert(id >= 0 && id < ptrStore.size());
        T *ptr = dynamic_cast<T *>(ptrStore[id].get());
        assert(ptr != nullptr);
        return *ptr;
    }
};

class Hierarchy {
  private:
    TraversalData traversal;
    const pointerVector *currentElementReferences = nullptr;
    int rootId = I_UNSET;

  public:
    bool hirearchyDirty = true;
    const TraversalData &RebuildTraversal(pointerVector &elementPointers);
    const TraversalData &ReturnCurrentTraversal() const;
    void RecursiveDownTraversal(int elementId);
    void RecursiveUpTraversal(int elementId);
    // access treversal without needing to rebuild
};

class LayoutContext {
  private:
    const pointerVector *currentElementReferences = nullptr;
    // layout engine will set this
    // and it will clear and update this
    friend class LayoutManager;

  public:
    void MarkDirty(int id);
    void MarkParentChainDirty(int id);
};

class LayoutManager {
    // manage abs pos and layout
    // take ptrStore to chase pointers...
    // set abs positions from locals
    // communicate if something was found dirty
    // update layout of "dirty" element
  private:
    LayoutContext context;

  public:
    bool Run(const TraversalData &traversal, pointerVector &elementPointers,
             UIStateTables &dataTables);
};

class RenderBatcher {
  private:
    RenderData FrameData;

  public:
    const RenderData &ReBuildFrameData(UIStateTables &dataTables,
                                       const std::vector<RenderOp> &displayList,
                                       float viewportHeight);
    const RenderData &GetFrameData() const;
    // prepare Verticies, indicies, cmd list
};

class Renderer {
    // only one aware of opengl
  private:
    VAO MainVAO;
    VBO MainVBO;
    EBO MainEBO;
    Shader DefaultShader;
    GLint resolutionUniform = I_UNSET;
    // 0th layout: 3 floats each
    // not normalized, 7 floats apart
    // 0 offset from the start
    Layout VertexLayout = {0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), 0};
    // 1st layout: 4 floats each
    // not normalized, 7 floats apart
    // 3 offset from the start
    Layout FragmentLayout = {1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), 3};
    bool initialized = false;

  public:
    void Init();
    void UploadFrame(const RenderData &frameData);
    void DrawFrame(const std::vector<DrawCommand> &commandsData,
                   const std::array<float, 2> &resolution);
    bool IsInitialized() { return initialized; }
};

class AnchorElement : public UIElement {
  public:
};

class UIRect : public UIElement {
  public:
};

class VerticalContainer : public UIElement {
  public:
    bool centerHorizontally = true;
    bool resizeChildren = false;
    float r = 0.6f, g = 0.1f, b = 0.8f;
    void UpdateLayout(UIStateTables &data, LayoutContext &context) override;
};

#endif
