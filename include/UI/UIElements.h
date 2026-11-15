#ifndef UI_ELEMENT_H
#define UI_ELEMENT_H

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <openglBasics/EBO.h>
#include <openglBasics/VAO.h>
#include <openglBasics/VBO.h>
#include <openglBasics/shaderclass.h>
#include <stdexcept>
#include <string>
#include <vector>

class UIElement;
class LayoutContext;
class Hierarchy;
class LayoutManager;
class RenderBatcher;
class Renderer;

constexpr float F_UNSET = -1.0f;
constexpr int I_UNSET = -1;
constexpr GLsizei MAX_CLIP_DEPTH = 16;
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

struct ElementInstance {
    uint32_t hardwareID = 0;
    GLfloat transform[4]{};
    GLuint payload[7]{};
};

static_assert(sizeof(ElementInstance) == 48);
static_assert(offsetof(ElementInstance, hardwareID) == 0);
static_assert(offsetof(ElementInstance, transform) == 4);
static_assert(offsetof(ElementInstance, payload) == 20);

struct ClipGeometry {
    GLfloat rect[4]{};
    GLfloat cornerRadii[4]{};
};

static_assert(sizeof(ClipGeometry) == 32);

struct ScissorRect {
    GLint x, y;
    GLsizei w, h;
};

struct DrawCommand {
    size_t instanceOffset = 0;
    GLsizei instanceCount = 0;
    size_t clipOffset = 0;
    GLsizei clipCount = 0;
    bool useScissor = false;
    ScissorRect scissorBox;
};

struct Color {
    float r, g, b, a;
    bool operator!=(const Color &other) const {
        return r != other.r || g != other.g || b != other.b || a != other.a;
    }
};

using PackedColor = uint32_t;

struct borderInfo {
    uint32_t border;
};

enum class ScreenLayoutMode {
    Mobile, // Width < 600px
    Tablet, // Width between 600px and 1024px
    Desktop // Width > 1024px
};

enum class RenderOpType { DrawElement, PushClip, PopClip };

struct RenderOp {
    RenderOpType type;
    int elementId;
};

struct RenderData {
    const int defaultReserve = 100;
    std::vector<ElementInstance> instances;
    std::vector<ClipGeometry> clips;
    std::vector<DrawCommand> commands;

    bool empty() const { return (instances.empty() || commands.empty()); }
};

struct TextGlyph {
    uint32_t glyphIndex = 0;
    uint32_t faceIndex = 0;
    float fontSize = 0.0f;
    float penX = 0.0f;
    float baseline = 0.0f;
    float xOffset = 0.0f;
    float yOffset = 0.0f;
};

struct GlyphAtlasEntry {
    float bitmapLeft = 0.0f;
    float bitmapTop = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 0.0f;
    float v1 = 0.0f;
    uint32_t layer = 0;
};

void InitializeGlyphAtlas();
GLuint GetGlyphAtlasTexture();
GlyphAtlasEntry ResolveGlyphAtlasEntry(const TextGlyph &glyph);

struct TraversalData {
    std::vector<int> drawOrder;
    std::vector<RenderOp> displayList;
    std::vector<int> parentIdLookup;
    bool empty() const {
        return drawOrder.empty() || displayList.empty();
    }
};

struct GeometryStoreState {
    float localX = F_UNSET;
    float localY = F_UNSET;
    float width = F_UNSET;
    float height = F_UNSET;
    float measuredWidth = F_UNSET;
    float measuredHeight = F_UNSET;

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

    PackedColor color = 0xFFFFFFFFu;
    PackedColor borderColor = 0x000000FFu;
    PackedColor shadowColor = 0x000000FFu;

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

    virtual void MeasureLayout(UIStateTables &data, LayoutContext &context);
    virtual void ArrangeLayout(UIStateTables &data, LayoutContext &context);
};

class ContainerElement : public UIElement {
  protected:
    std::array<float, 2> ResolveChildSize(
        float computedWidth, float computedHeight, float parentWidth,
        float parentHeight, const StyleStoreState &childStyle) const;
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
    friend class UIScene;

    struct LayoutSnapshot {
        float localX;
        float localY;
        float width;
        float height;
        float absoluteX;
        float absoluteY;
    };

    int rootId = I_UNSET;
    bool debug = false;
    std::vector<LayoutSnapshot> previous;
    std::vector<int> dirtyElementIds;
    std::vector<uint32_t> dirtyMarks;
    uint32_t dirtyGeneration = 0;
    const std::vector<int> &RunInto(const TraversalData &traversal,
                                    pointerVector &elementPointers,
                                    UIStateTables &dataTables);
    // bool initialized = false;

  public:
    LayoutContext context;
    std::vector<int> Run(const TraversalData &traversal,
                         pointerVector &elementPointers,
                         UIStateTables &dataTables);
    void SetRoot(int id);
    void SetDebug(bool enabled);
};

class RenderBatcher {
  private:
    RenderData FrameData;
    std::vector<size_t> firstInstanceByElement;
    size_t indexedElementCount = 0;
    std::vector<uint32_t> dirtyMarks;
    uint32_t dirtyGeneration = 0;
    std::vector<ScissorRect> scissorRects;
    std::vector<ClipGeometry> activeClips;
    bool debug = false;

  public:
    const RenderData &ReBuildFrameData(UIStateTables &dataTables,
                                       const std::vector<RenderOp> &displayList,
                                       const std::vector<int> &dirtyElements,
                                       float viewportHeight,
                                       const pointerVector &elements);
    const RenderData &SmartReBuildFrameData(UIStateTables &dataTables,
                                            const std::vector<RenderOp> &displayList,
                                            const std::vector<int> &dirtyElements,
                                            float viewportHeight,
                                            const pointerVector &elements);
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
    GLint clipCountUniform = I_UNSET;
    GLint clipRectsUniform = I_UNSET;
    GLint clipRadiiUniform = I_UNSET;
    GLint glyphAtlasUniform = I_UNSET;

    Layout HardwareIDLayout = {0, 1, GL_UNSIGNED_INT, GL_FALSE,
                               sizeof(ElementInstance),
                               offsetof(ElementInstance, hardwareID), 1, GL_TRUE};
    Layout TransformLayout = {1, 4, GL_FLOAT, GL_FALSE, sizeof(ElementInstance),
                              offsetof(ElementInstance, transform), 1, GL_FALSE};
    Layout Payload0Layout = {2, 4, GL_UNSIGNED_INT, GL_FALSE,
                             sizeof(ElementInstance),
                             offsetof(ElementInstance, payload), 1, GL_TRUE};
    Layout Payload1Layout = {3, 3, GL_UNSIGNED_INT, GL_FALSE,
                             sizeof(ElementInstance),
                             offsetof(ElementInstance, payload) + 4 * sizeof(GLuint),
                             1, GL_TRUE};

    bool initialized = false;
    bool debug = false;

  public:
    void Init();
    void UploadFrame(const RenderData &frameData);
    void DrawFrame(const RenderData &frameData,
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
    void EditElementText(int id, std::string text, float fontSize,
                         std::string fontFamily = {});
    void StepFrame(std::array<float, 2> &resolution);
    int AddRectangle(int parentId, const GeometryStoreState &shape, Color color,
                     float cornerRadius = 0.0f, float borderWidth = 0.0f,
                     Color borderColor = {0.0f, 0.0f, 0.0f, 1.0f});
    int AddText(int parentId, float x, float y, float fontSize,
                std::string text, Color color, std::string fontFamily = {});
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

  private:
    template <typename T>
    int AddElementToParent(int parentId, const GeometryStoreState &shape) {
        if (parentId != I_UNSET &&
            (parentId < 0 || static_cast<size_t>(parentId) >= ptrStore.size() ||
             !ptrStore[parentId])) {
            throw std::out_of_range("Cannot add an element to an invalid parent.");
        }

        const int id = AddElement<T>();
        if (parentId != I_UNSET)
            AddChild(parentId, id);
        EditElementShape(id, shape, false);
        return id;
    }
};

class UIRect : public ContainerElement {
  public:
};

class VerticalContainer : public ContainerElement {
  public:
    bool centerHorizontally = true;
    bool resizeChildren = false;
    float r = 0.6f, g = 0.1f, b = 0.8f;
    void MeasureLayout(UIStateTables &data, LayoutContext &context) override;
    void ArrangeLayout(UIStateTables &data, LayoutContext &context) override;
};

class TextElement : public UIElement {
  public:
    std::string text;
    std::string fontFamily;
    float fontSize = 24.0f;
    float measuredTextWidth = 0.0f;
    float measuredTextHeight = 0.0f;
    std::vector<TextGlyph> glyphs;
    bool textLayoutDirty = true;

    void MeasureLayout(UIStateTables &data, LayoutContext &context) override;
};

#endif