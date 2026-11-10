#ifndef UI_ELEMENT_H
#define UI_ELEMENT_H

#include <array>
#include <cassert>
#include <cstddef>
#include <memory>
#include <openglBasics/EBO.h>
#include <openglBasics/VAO.h>
#include <openglBasics/VBO.h>
#include <openglBasics/shaderclass.h>
#include <unordered_map>
#include <unordered_set>
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

// Major refactor, switched from standard drawing + batching to:
// instanced drawing + batching

// Vertex Struct no longer neeced
// struct Vertex {
//     GLfloat pos[3];
//     GLfloat color[4];
//     GLfloat uv[2];
//     GLfloat styleParams[2]; // corner radius, border width
// };

struct __attribute__((packed)) ElementInstance {
    GLfloat transform[4];
    GLuint colorInfo;           // RGBA8
    GLuint borderColor;         // RGBA8
    GLuint borderAndCornerInfo; // all edge border, all 4 corner roundness
    GLuint shadowInfo;          // shadow blur, x & y offset
    GLuint shadowColor;         // RGBA8
};

static_assert(sizeof(ElementInstance) == 36);
static_assert(offsetof(ElementInstance, transform) == 0);
static_assert(offsetof(ElementInstance, colorInfo) == 16);
static_assert(offsetof(ElementInstance, borderColor) == 20);
static_assert(offsetof(ElementInstance, borderAndCornerInfo) == 24);
static_assert(offsetof(ElementInstance, shadowInfo) == 28);
static_assert(offsetof(ElementInstance, shadowColor) == 32);

// struct __attribute__((packed)) TextInstance {};

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

struct colorInfo {
    uint32_t RGBA8 = 0xFFFFFFFFu;
    uint8_t toUInt8(float val);
    void setR(uint8_t r);
    void setG(uint8_t g);
    void setB(uint8_t b);
    void setA(uint8_t a);
    void setRf(float r);
    void setGf(float g);
    void setBf(float b);
    void setAf(float a);
    void setRGBA8F(float r, float g, float b, float a);
    void setRGBA88(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void setRGBA832(uint32_t color);
    void getR(uint8_t &out);
    void getG(uint8_t &out);
    void getB(uint8_t &out);
    void getA(uint8_t &out);
    void getRf(float &out);
    void getGf(float &out);
    void getBf(float &out);
    void getAf(float &out);
};

struct borderInfo {
    uint32_t border;
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
    const int defaultReserve = 100;
    std::vector<ElementInstance> instances;
    std::vector<DrawCommand> commands;

    std::unordered_map<int, size_t> indexByInstanceId;
    std::unordered_set<int> dirtyElements;
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

    colorInfo color;
    colorInfo borderColor;
    colorInfo shadowColor;

    float prefferedWidthPercent = F_UNSET;
    float prefferedHeightPercent = F_UNSET;
    float borderTop = 0.0f;
    float borderBottom = 0.0f;
    float borderRight = 0.0f;
    float borderLeft = 0.0f;
    float cornerRadiusTopLeft = 0.0f;
    float cornerRadiusTopRight = 0.0f;
    float cornerRadiusBottomLeft = 0.0f;
    float cornerRadiusBottomRight = 0.0f;
    float shadowBlur = 0.0f;
    float shadowOffsetX = 0.0f;
    float shadowOffsetY = 0.0f;
    float padding = 0.0f; // padding is applied to children, not self, so it does not
                          // affect own size
    bool hidden = false;
};

struct UIStateTables {
    std::vector<GeometryStoreState> geometry;
    std::vector<StyleStoreState> style;
};

std::array<float, 2> ApplyStyle(float computedWidth, float computedHeight,
                                float parentWidth, float parentHeight,
                                const StyleStoreState &style);

// ui element dosent function without a manager
class UIElement {
  public:
    bool isDirty = true;
    bool fitContentHeight = false;
    bool fitContentWidth = false;
    bool clipChildren = false;

    uint32_t lastLayoutRunFrame;

    int id = I_UNSET;
    int parentId = I_UNSET;

    float minWidth = F_UNSET;
    float maxWidth = 9000.0f;

    float minHeight = F_UNSET;
    float maxHeight = 9000.0f;

    float widthPercent = F_UNSET; // b/w 0.0f and 1.0f
    float heightPercent = F_UNSET;

    std::vector<int> childIds;

    virtual ~UIElement() = default;

    virtual bool UpdateLayout(UIStateTables &data, LayoutContext &context);
};

class LayoutContext {
  private:
    const pointerVector *currentElementReferences = nullptr;
    const std::vector<int> *parentIdLookup = nullptr;
    friend class LayoutManager;

  public:
    void MarkDirty(int targetId, int callerId);
    void MarkParentChainDirty(int targetId, int callerId);
    bool IsChildOf(int childId, int parentId) const;
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
    std::vector<int> Run(const TraversalData &traversal,
                         pointerVector &elementPointers, UIStateTables &dataTables);
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
                                       const std::vector<int> &dirtyElements,
                                       float viewportHeight);
    const RenderData &SmartReBuildFrameData(UIStateTables &dataTables,
                                            const std::vector<RenderOp> &displayList,
                                            const std::vector<int> &dirtyElements,
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
                              offsetof(ElementInstance, transform),
                              1,
                              GL_FALSE};

    Layout colorInfo = {1,
                        1,
                        GL_UNSIGNED_INT,
                        GL_FALSE,
                        sizeof(ElementInstance),
                        offsetof(ElementInstance, colorInfo),
                        1,
                        GL_TRUE};

    Layout BorderColor = {
        2,
        1,
        GL_UNSIGNED_INT,
        GL_FALSE,
        sizeof(ElementInstance),
        offsetof(ElementInstance, borderColor),
        1,
        GL_TRUE};

    Layout BorderAndCornerStyle = {
        3,
        1,
        GL_UNSIGNED_INT,
        GL_FALSE,
        sizeof(ElementInstance),
        offsetof(ElementInstance, borderAndCornerInfo),
        1,
        GL_TRUE};

    Layout ShadowStyle = {
        4,
        1,
        GL_UNSIGNED_INT,
        GL_FALSE,
        sizeof(ElementInstance),
        offsetof(ElementInstance, shadowInfo),
        1,
        GL_TRUE};

    Layout ShadowColor = {
        5,
        1,
        GL_UNSIGNED_INT,
        GL_FALSE,
        sizeof(ElementInstance),
        offsetof(ElementInstance, shadowColor),
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
    void EditElementColor(int id, Color props, bool dirtyChain);
    void EditElementBorder(int id, float borderTop, float borderRight,
                           float borderBottom, float borderLeft,
                           bool dirtyChain = true);
    void EditElementBorderColor(int id, Color borderColor, bool dirtyChain);
    void EditElementCornerRadius(int id, float topLeft, float topRight,
                                 float bottomLeft, float bottomRight,
                                 bool dirtyChain = true);
    void EditElementPadding(int id, float padding, bool dirtyChain);
    void EditElementShadow(int id, float blur, float offsetX, float offsetY,
                           Color shadowColor, bool dirtyChain);
    void EditElementVisibility(int id, bool hidden, bool dirtyChain);
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

        elementAppearance;
        elementAppearance.hidden = false;
        elementAppearance.maxHeight = F_UNSET;
        elementAppearance.maxWidth = F_UNSET;
        elementAppearance.minHeight = F_UNSET;
        elementAppearance.minWidth = F_UNSET;
        elementAppearance.cornerRadiusTopLeft = 0.0f;
        elementAppearance.cornerRadiusTopRight = 0.0f;
        elementAppearance.cornerRadiusBottomLeft = 0.0f;
        elementAppearance.cornerRadiusBottomRight = 0.0f;
        elementAppearance.borderBottom = 0.0f;
        elementAppearance.borderLeft = 0.0f;
        elementAppearance.borderRight = 0.0f;
        elementAppearance.borderTop = 0.0f;
        elementAppearance.padding = 0.0f;

        elementAppearance.color.setRGBA8F(1.0f, 1.0f, 1.0f, 1.0f);
        elementAppearance.borderColor.setRGBA8F(0.0f, 0.0f, 0.0f, 1.0f);
        elementAppearance.shadowColor.setRGBA8F(0.0f, 0.0f, 0.0f, 1.0f);

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