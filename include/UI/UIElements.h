#ifndef UI_ELEMENT_H
#define UI_ELEMENT_H

#include <array>
#include <cassert>
#include <memory>
#include <openglBasics/EBO.h>
#include <openglBasics/VAO.h>
#include <openglBasics/VBO.h>
#include <openglBasics/shaderclass.h>
#include <vector>

class UIElement;
class LayoutContext;
class Hierarchy;
class LayoutManager;
class RenderBatcher;
class Renderer;

constexpr float F_UNSET = -1.0f;
constexpr int I_UNSET = -1;
typedef std::vector<std::unique_ptr<UIElement>> pointerVector;

// Vertex Struct no longer neeced
// struct Vertex {
//     GLfloat pos[3];
//     GLfloat color[4];
//     GLfloat uv[2];
//     GLfloat styleParams[2]; // corner radius, border width
// };

struct __attribute__((packed)) ElementInstance {
    GLfloat transform[4];
    GLuint packedColor;
    GLuint borderInfo[2]; // packed 4 edge toggles and 4 color bits, space for more
    GLuint cornerInfo;    // packed 4 corner raddii, space for future values
};

struct ScissorRect {
    GLint x, y;
    GLsizei w, h;
};

struct DrawCommand {
    size_t instanceOffset = I_UNSET;
    GLsizei instanceCount = I_UNSET;
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
    std::vector<ElementInstance> instances;
    std::vector<DrawCommand> commands;
    bool empty() const { return (instances.empty() || commands.empty()); }
};

struct TraversalData {
    std::vector<int> drawOrder;
    std::vector<RenderOp> displayList;
    std::vector<int> parentIdLookup;
    bool empty() const {
        return (drawOrder.empty() || displayList.empty() || parentIdLookup.empty());
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
    float borderWidth = 0.0f;
    Color borderColor = {0.0f, 0.0f, 0.0f, 1.0f};
    float cornerRadiusTopLeft = 0.0f;
    float cornerRadiusTopRight = 0.0f;
    float cornerRadiusBottomLeft = 0.0f;
    float cornerRadiusBottomRight = 0.0f;
    float padding = 0.0f; // padding is applied to children, not self, so it does not
                          // affect own size
    bool hidden = false;

    Color color;
};

struct UIStateTables {
    std::vector<GeometryStoreState> geometry;
    std::vector<StyleStoreState> style;
};

// ui element dosent function without a manager
class UIElement {
  public:
    bool isDirty = true;
    bool fitContentHeight = false;
    bool fitContentWidth = false;
    bool clipChildren = false;

    int id = I_UNSET;
    int parentId = I_UNSET;

    float minWidth = F_UNSET;
    float maxWidth = 9000.0f;

    float minHeight = F_UNSET;
    float maxHeight = 9000.0f;

    float widthPercent = F_UNSET; // b/w 0.0f and 1.0f
    float heightPercent = F_UNSET;

    // float padding = 5.0f;
    // float borderWidth = 0.0f;
    // float cornerRadiusTopLeft = 0.0f;
    // float cornerRadiusTopRight = 0.0f;
    // float cornerRadiusBottomLeft = 0.0f;
    // float cornerRadiusBottomRight = 0.0f;
    // float borderWidth = 0.0f;

    std::vector<int> childIds;

    virtual ~UIElement() = default;

    virtual bool UpdateLayout(UIStateTables &data, LayoutContext &context);
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

class Hierarchy {
  private:
    TraversalData traversal;
    const pointerVector *currentElementReferences = nullptr;
    int rootId = I_UNSET;

  public:
    bool hirearchyDirty = true;
    bool debug = false;
    const TraversalData &RebuildTraversal(pointerVector &elementPointers);
    const TraversalData &ReturnCurrentTraversal() const;
    void RecursiveDownTraversal(int elementId);
    void RecursiveUpTraversal(int elementId);
    void SetRoot(int id);
    // access treversal without needing to rebuild
};

class LayoutManager {
    // manage abs pos and layout
    // take ptrStore to chase pointers...
    // set abs positions from locals
    // communicate if something was found dirty
    // update layout of "dirty" element
  private:
    int rootId = I_UNSET;
    bool debug = false;
    // bool initialized = false;

  public:
    LayoutContext context;
    bool Run(const TraversalData &traversal, pointerVector &elementPointers,
             UIStateTables &dataTables);
    void SetRoot(int id);
    void SetDebug(bool enabled);
};

class RenderBatcher {
  private:
    RenderData FrameData;
    bool debug = false;

  public:
    const RenderData &ReBuildFrameData(UIStateTables &dataTables,
                                       const std::vector<RenderOp> &displayList,
                                       float viewportHeight);
    const RenderData &GetFrameData() const;
    // prepare Verticies, indicies, cmd list
    void SetDebug(bool enabled);
};

class Renderer {
    // only one aware of opengl
  private:
    VAO MainVAO;
    VBO MainVBO;
    // EBO MainEBO; no longer required
    Shader DefaultShader;
    GLint resolutionUniform = I_UNSET;

    Layout TransformLayout = {0,
                              4,
                              GL_FLOAT,
                              GL_FALSE,
                              sizeof(ElementInstance),
                              static_cast<uintptr_t>(0 * sizeof(float)),
                              1,
                              GL_FALSE};

    Layout PackedColor = {1,
                          4,
                          GL_UNSIGNED_BYTE,
                          GL_TRUE,
                          sizeof(ElementInstance),
                          static_cast<uintptr_t>(4 * sizeof(float)),
                          1,
                          GL_FALSE};

    Layout BorderStyle = {
        2,
        2,
        GL_UNSIGNED_INT,
        GL_FALSE,
        sizeof(ElementInstance),
        static_cast<uintptr_t>((4 * sizeof(float)) + (1 * sizeof(uint32_t))),
        1,
        GL_TRUE};

    Layout CornerStyle = {
        3,
        1,
        GL_UNSIGNED_INT,
        GL_FALSE,
        sizeof(ElementInstance),
        static_cast<uintptr_t>((4 * sizeof(float)) + (3 * sizeof(uint32_t))),
        1,
        GL_TRUE};

    bool initialized = false;
    bool debug = false;

  public:
    void Init();
    void UploadFrame(const RenderData &frameData);
    void DrawFrame(const std::vector<DrawCommand> &commandsData,
                   const std::array<float, 2> &resolution);
    bool IsInitialized() { return initialized; }
    void SetDebug(bool enabled);
};

class AnchorElement : public UIElement {
  public:
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
    bool debug = false;

  public:
    int defaultCapacity = 100; // for ptrStore

    float viewportWidth = F_UNSET;
    float viewportHeight = F_UNSET;

    void Init();
    void SetDebug(bool enabled);
    void SetRoot(int id);
    void MarkDirty(int id);
    void MarkParentChainDirty(int id);
    void EditElementShape(int id, const GeometryStoreState &props, bool dirtyChain);
    void EditElementColor(int id, const Color &props, bool dirtyChain);
    void EditElementBorder(int id, float borderWidth, bool dirtyChain);
    void EditElementBorderColor(int id, const Color &borderColor, bool dirtyChain);
    void EditElementCornerRadius(int id, float topLeft, float topRight,
                                 float bottomLeft, float bottomRight,
                                 bool dirtyChain);
    void EditElementPadding(int id, float padding, bool dirtyChain);
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
        elementAppearance.cornerRadiusTopLeft = 0.0f;
        elementAppearance.cornerRadiusTopRight = 0.0f;
        elementAppearance.cornerRadiusBottomLeft = 0.0f;
        elementAppearance.cornerRadiusBottomRight = 0.0f;
        elementAppearance.borderWidth = 0.0f;
        elementAppearance.borderColor = {0.0f, 0.0f, 0.0f, 1.0f};
        elementAppearance.padding = 0.0f;

        dataTables.geometry.push_back(elementShape);
        dataTables.style.push_back(elementAppearance);
        return newID;
    }

    template <typename T> T &Get(int id) {
        assert(id >= 0 && id < ptrStore.size());
        T *ptr = dynamic_cast<T *>(ptrStore[id].get());
        assert(ptr != nullptr);
        return *ptr;
    }
};

class UIRect : public UIElement {
  public:
};

class VerticalContainer : public UIElement {
  public:
    bool centerHorizontally = true;
    bool resizeChildren = false;
    float r = 0.6f, g = 0.1f, b = 0.8f;
    bool UpdateLayout(UIStateTables &data, LayoutContext &context) override;
};

#endif