#include <UI/UIElements.h>

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb-ft.h>
#include <hb.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <string_view>

namespace {

constexpr GLint ATLAS_SIZE = 1024;
constexpr GLint ATLAS_LAYERS = 16;
constexpr GLint MAX_FONT_SIZE = 512;
constexpr uint16_t GLYPH_RASTER_SCALE = 2;

struct FontFace {
    std::string path;
    FT_Face face = nullptr;
};

struct Codepoint {
    uint32_t value;
    size_t byteStart;
    size_t byteEnd;
};

struct AtlasPage {
    GLint x = 1;
    GLint y = 1;
    GLint rowHeight = 0;
};

struct GlyphKey {
    uint32_t faceIndex;
    uint32_t glyphIndex;
    uint16_t pixelSize;

    bool operator==(const GlyphKey &other) const {
        return faceIndex == other.faceIndex && glyphIndex == other.glyphIndex &&
               pixelSize == other.pixelSize;
    }
};

struct GlyphKeyHash {
    size_t operator()(const GlyphKey &key) const {
        size_t result = key.faceIndex;
        result = result * 0x9E3779B1u + key.glyphIndex;
        return result * 0x9E3779B1u + key.pixelSize;
    }
};

class FontSystem {
  public:
    FontSystem() {
        if (FT_Init_FreeType(&library) != 0)
            throw std::runtime_error("Failed to initialize FreeType.");
        if (!FcInit())
            throw std::runtime_error("Failed to initialize Fontconfig.");
    }

    ~FontSystem() {
        for (FontFace &font : fonts) {
            if (font.face)
                FT_Done_Face(font.face);
        }
        if (library)
            FT_Done_FreeType(library);
    }

    void Shape(TextElement &element, GeometryStoreState &geometry) {
        if (element.fontSize < 1.0f ||
            element.fontSize > static_cast<float>(MAX_FONT_SIZE) ||
            !std::isfinite(element.fontSize)) {
            throw std::invalid_argument("Text font size must be between 1 and " +
                                        std::to_string(MAX_FONT_SIZE) + ".");
        }
        if (element.text.size() >
            static_cast<size_t>(std::numeric_limits<int>::max())) {
            throw std::invalid_argument("Text exceeds the HarfBuzz size limit.");
        }

        element.glyphs.clear();
        const std::vector<std::string> &fontPaths =
            FindFonts(element.fontFamily);
        if (fontPaths.empty())
            throw std::runtime_error("Fontconfig found no usable system fonts.");

        const uint16_t pixelSize =
            static_cast<uint16_t>(std::lround(element.fontSize));
        uint32_t primaryFace = UINT32_MAX;
        for (const std::string &path : fontPaths) {
            primaryFace = LoadFace(path);
            if (primaryFace != UINT32_MAX)
                break;
        }
        if (primaryFace == UINT32_MAX)
            throw std::runtime_error("Fontconfig found no usable system fonts.");
        SetSize(primaryFace, pixelSize);
        const float ascender = fonts[primaryFace].face->size->metrics.ascender / 64.0f;
        const float lineHeight =
            std::max(element.fontSize,
                     fonts[primaryFace].face->size->metrics.height / 64.0f);

        float maxLineWidth = 0.0f;
        size_t lineStart = 0;
        size_t lineNumber = 0;
        while (lineStart <= element.text.size()) {
            size_t lineEnd = element.text.find_first_of("\r\n", lineStart);
            if (lineEnd == std::string::npos)
                lineEnd = element.text.size();
            ShapeLine(element, fontPaths, primaryFace, lineStart, lineEnd, pixelSize,
                      static_cast<float>(lineNumber) * lineHeight + ascender,
                      maxLineWidth);
            ++lineNumber;
            if (lineEnd == element.text.size())
                break;
            lineStart = lineEnd + 1;
            if (element.text[lineEnd] == '\r' &&
                lineStart < element.text.size() &&
                element.text[lineStart] == '\n') {
                ++lineStart;
            }
        }

        element.measuredTextWidth = maxLineWidth;
        element.measuredTextHeight = static_cast<float>(lineNumber) * lineHeight;
        geometry.measuredWidth = element.measuredTextWidth;
        geometry.measuredHeight = element.measuredTextHeight;
        element.textLayoutDirty = false;
    }

    void InitializeAtlas() {
        if (atlasTexture != 0)
            return;

        glGenTextures(1, &atlasTexture);
        if (atlasTexture == 0)
            throw std::runtime_error("Failed to create the glyph atlas texture.");
        glBindTexture(GL_TEXTURE_2D_ARRAY, atlasTexture);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_R8, ATLAS_SIZE, ATLAS_SIZE,
                     ATLAS_LAYERS, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        CheckGlError("initializing the glyph atlas");
        atlasInitialized = true;
    }

    GLuint AtlasTexture() const { return atlasTexture; }

    GlyphAtlasEntry Resolve(const TextGlyph &glyph) {
        if (!atlasInitialized)
            throw std::runtime_error("Glyph atlas has not been initialized.");
        if (glyph.faceIndex >= fonts.size())
            throw std::runtime_error("Text glyph refers to an unknown font face.");

        const uint16_t pixelSize =
            static_cast<uint16_t>(std::lround(glyph.fontSize));
        const GlyphKey key{glyph.faceIndex, glyph.glyphIndex, pixelSize};
        const auto cached = glyphs.find(key);
        if (cached != glyphs.end())
            return cached->second;

        const uint16_t rasterScale =
            pixelSize <= ATLAS_SIZE / GLYPH_RASTER_SCALE
                ? GLYPH_RASTER_SCALE
                : 1;
        const uint16_t rasterPixelSize = pixelSize * rasterScale;
        SetSize(glyph.faceIndex, rasterPixelSize);
        FT_Face face = fonts[glyph.faceIndex].face;
        if (FT_Load_Glyph(face, glyph.glyphIndex, FT_LOAD_DEFAULT) != 0 ||
            FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
            throw std::runtime_error("Failed to rasterize a system font glyph.");
        }

        const FT_Bitmap &bitmap = face->glyph->bitmap;
        GlyphAtlasEntry entry;
        const float rasterScaleF = static_cast<float>(rasterScale);
        entry.bitmapLeft =
            static_cast<float>(face->glyph->bitmap_left) / rasterScaleF;
        entry.bitmapTop =
            static_cast<float>(face->glyph->bitmap_top) / rasterScaleF;
        entry.width = static_cast<float>(bitmap.width) / rasterScaleF;
        entry.height = static_cast<float>(bitmap.rows) / rasterScaleF;
        if (bitmap.width == 0 || bitmap.rows == 0) {
            glyphs.emplace(key, entry);
            return entry;
        }

        if (bitmap.width + 2 > ATLAS_SIZE || bitmap.rows + 2 > ATLAS_SIZE)
            throw std::runtime_error("A rasterized glyph exceeds the atlas page size.");

        GLint layer = static_cast<GLint>(pages.size()) - 1;
        if (layer < 0 || !Fits(pages[static_cast<size_t>(layer)], bitmap.width,
                              bitmap.rows)) {
            if (pages.size() >= ATLAS_LAYERS)
                throw std::runtime_error("The glyph atlas is full.");
            pages.emplace_back();
            layer = static_cast<GLint>(pages.size()) - 1;
        }

        AtlasPage &page = pages[static_cast<size_t>(layer)];
        if (page.x + static_cast<GLint>(bitmap.width) + 1 > ATLAS_SIZE) {
            page.x = 1;
            page.y += page.rowHeight + 1;
            page.rowHeight = 0;
        }

        const GLint x = page.x;
        const GLint y = page.y;
        page.x += static_cast<GLint>(bitmap.width) + 1;
        page.rowHeight = std::max(page.rowHeight, static_cast<GLint>(bitmap.rows));

        std::vector<uint8_t> pixels(static_cast<size_t>(bitmap.width) *
                                    bitmap.rows);
        const int pitch = bitmap.pitch;
        for (unsigned int row = 0; row < bitmap.rows; ++row) {
            const unsigned int sourceRow =
                pitch >= 0 ? row : bitmap.rows - 1 - row;
            const uint8_t *source =
                bitmap.buffer + static_cast<ptrdiff_t>(sourceRow) * std::abs(pitch);
            uint8_t *destination =
                pixels.data() + static_cast<size_t>(row) * bitmap.width;
            if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
                if (bitmap.num_grays == 256) {
                    std::memcpy(destination, source, bitmap.width);
                } else if (bitmap.num_grays <= 1) {
                    for (unsigned int column = 0; column < bitmap.width; ++column)
                        destination[column] = source[column] == 0 ? 0 : 255;
                } else {
                    for (unsigned int column = 0; column < bitmap.width; ++column) {
                        destination[column] = static_cast<uint8_t>(
                            static_cast<unsigned int>(source[column]) * 255 /
                            (bitmap.num_grays - 1));
                    }
                }
            } else if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO) {
                for (unsigned int column = 0; column < bitmap.width; ++column) {
                    const uint8_t mask =
                        static_cast<uint8_t>(0x80u >> (column % 8));
                    destination[column] =
                        (source[column / 8] & mask) != 0 ? 255 : 0;
                }
            } else if (bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
                for (unsigned int column = 0; column < bitmap.width; ++column)
                    destination[column] = source[column * 4 + 3];
            } else {
                throw std::runtime_error("Unsupported FreeType glyph bitmap format.");
            }
        }

        GLint previousUnpackAlignment = 4;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glBindTexture(GL_TEXTURE_2D_ARRAY, atlasTexture);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, x, y, layer,
                        static_cast<GLsizei>(bitmap.width),
                        static_cast<GLsizei>(bitmap.rows), 1, GL_RED,
                        GL_UNSIGNED_BYTE, pixels.data());
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
        CheckGlError("uploading a glyph to the atlas");

        entry.u0 = (static_cast<float>(x) + 0.5f) / ATLAS_SIZE;
        entry.v0 = (static_cast<float>(y) + 0.5f) / ATLAS_SIZE;
        entry.u1 = (static_cast<float>(x + bitmap.width) - 0.5f) / ATLAS_SIZE;
        entry.v1 = (static_cast<float>(y + bitmap.rows) - 0.5f) / ATLAS_SIZE;
        entry.layer = static_cast<uint32_t>(layer);
        glyphs.emplace(key, entry);
        return entry;
    }

  private:
    FT_Library library = nullptr;
    GLuint atlasTexture = 0;
    bool atlasInitialized = false;
    std::vector<FontFace> fonts;
    std::vector<AtlasPage> pages;
    std::unordered_map<std::string, std::vector<std::string>> familyFonts;
    std::unordered_map<std::string, uint32_t> faceIndicesByPath;
    std::unordered_map<GlyphKey, GlyphAtlasEntry, GlyphKeyHash> glyphs;

    static void CheckGlError(const char *operation) {
        const GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            throw std::runtime_error(std::string("OpenGL error while ") +
                                     operation + ": " + std::to_string(error));
        }
    }

    static bool Fits(const AtlasPage &page, unsigned int width,
                     unsigned int height) {
        GLint x = page.x;
        GLint y = page.y;
        if (x + static_cast<GLint>(width) + 1 > ATLAS_SIZE) {
            x = 1;
            y += page.rowHeight + 1;
        }
        return y + static_cast<GLint>(height) + 1 <= ATLAS_SIZE;
    }

    void SetSize(uint32_t faceIndex, uint16_t pixelSize) {
        if (FT_Set_Pixel_Sizes(fonts[faceIndex].face, 0, pixelSize) != 0)
            throw std::runtime_error("Failed to set the requested font size.");
    }

    uint32_t LoadFace(const std::string &path) {
        const auto cached = faceIndicesByPath.find(path);
        if (cached != faceIndicesByPath.end())
            return cached->second;

        FT_Face face = nullptr;
        if (FT_New_Face(library, path.c_str(), 0, &face) != 0) {
            faceIndicesByPath.emplace(path, UINT32_MAX);
            return UINT32_MAX;
        }
        fonts.push_back({path, face});
        const uint32_t index = static_cast<uint32_t>(fonts.size() - 1);
        faceIndicesByPath.emplace(path, index);
        return index;
    }

    const std::vector<std::string> &FindFonts(const std::string &family) {
        const std::string cacheKey = family.empty() ? "sans-serif" : family;
        const auto cached = familyFonts.find(cacheKey);
        if (cached != familyFonts.end())
            return cached->second;

        FcPattern *pattern = FcPatternCreate();
        if (!pattern)
            throw std::runtime_error("Failed to create a Fontconfig query.");
        FcPatternAddString(
            pattern, FC_FAMILY,
            reinterpret_cast<const FcChar8 *>(cacheKey.c_str()));
        FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
        FcDefaultSubstitute(pattern);
        FcResult result = FcResultNoMatch;
        FcFontSet *fontSet = FcFontSort(nullptr, pattern, FcTrue, nullptr, &result);
        FcPatternDestroy(pattern);
        if (!fontSet)
            throw std::runtime_error("Fontconfig could not resolve a system font.");

        std::vector<std::string> resultPaths;
        std::unordered_set<std::string_view> seen;
        for (int i = 0; i < fontSet->nfont; ++i) {
            FcChar8 *file = nullptr;
            if (FcPatternGetString(fontSet->fonts[i], FC_FILE, 0, &file) !=
                FcResultMatch) {
                continue;
            }
            const std::string_view path(
                reinterpret_cast<const char *>(file));
            if (seen.insert(path).second)
                resultPaths.emplace_back(path);
        }
        FcFontSetDestroy(fontSet);
        return familyFonts.emplace(cacheKey, std::move(resultPaths))
            .first->second;
    }

    static std::vector<Codepoint> DecodeUtf8(const std::string &text,
                                             size_t start, size_t end) {
        std::vector<Codepoint> result;
        for (size_t i = start; i < end;) {
            const size_t byteStart = i;
            const uint8_t first = static_cast<uint8_t>(text[i]);
            uint32_t value = 0xFFFD;
            uint32_t minimumValue = 0;
            size_t length = 1;
            if (first < 0x80) {
                value = first;
            } else if ((first & 0xE0) == 0xC0 && i + 1 < end) {
                value = first & 0x1F;
                minimumValue = 0x80;
                length = 2;
            } else if ((first & 0xF0) == 0xE0 && i + 2 < end) {
                value = first & 0x0F;
                minimumValue = 0x800;
                length = 3;
            } else if ((first & 0xF8) == 0xF0 && i + 3 < end) {
                value = first & 0x07;
                minimumValue = 0x10000;
                length = 4;
            }

            bool valid = length > 1 || first < 0x80;
            for (size_t j = 1; valid && j < length; ++j) {
                const uint8_t continuation = static_cast<uint8_t>(text[i + j]);
                if ((continuation & 0xC0) != 0x80) {
                    valid = false;
                    break;
                }
                value = (value << 6) | (continuation & 0x3F);
            }
            if (!valid || value < minimumValue || value > 0x10FFFF ||
                (value >= 0xD800 && value <= 0xDFFF)) {
                value = 0xFFFD;
                length = 1;
            }
            result.push_back({value, byteStart, byteStart + length});
            i += length;
        }
        return result;
    }

    uint32_t FindFaceForCodepoint(const std::vector<std::string> &fontPaths,
                                  uint32_t fallbackFace, uint32_t codepoint) {
        for (const std::string &path : fontPaths) {
            const uint32_t index = LoadFace(path);
            if (index == UINT32_MAX)
                continue;
            SetSize(index, activePixelSize);
            if (FT_Get_Char_Index(fonts[index].face, codepoint) != 0)
                return index;
        }
        return fallbackFace;
    }

    void ShapeLine(TextElement &element,
                   const std::vector<std::string> &fontPaths,
                   uint32_t fallbackFace, size_t lineStart, size_t lineEnd,
                   uint16_t pixelSize, float baseline, float &maxLineWidth) {
        std::vector<Codepoint> codepoints =
            DecodeUtf8(element.text, lineStart, lineEnd);
        if (codepoints.empty())
            return;

        activePixelSize = pixelSize;
        float penX = 0.0f;
        float minX = 0.0f;
        float maxX = 0.0f;
        size_t runStart = 0;
        while (runStart < codepoints.size()) {
            const uint32_t faceIndex =
                FindFaceForCodepoint(fontPaths, fallbackFace,
                                     codepoints[runStart].value);
            size_t runEnd = runStart + 1;
            while (runEnd < codepoints.size() &&
                   FindFaceForCodepoint(fontPaths, fallbackFace,
                                        codepoints[runEnd].value) == faceIndex) {
                ++runEnd;
            }

            SetSize(faceIndex, pixelSize);
            hb_font_t *font = hb_ft_font_create_referenced(fonts[faceIndex].face);
            if (!font)
                throw std::runtime_error("HarfBuzz could not create a font.");
            hb_buffer_t *buffer = hb_buffer_create();
            if (!buffer) {
                hb_font_destroy(font);
                throw std::runtime_error("HarfBuzz could not create a shaping buffer.");
            }
            const size_t byteStart = codepoints[runStart].byteStart;
            const size_t byteEnd = codepoints[runEnd - 1].byteEnd;
            hb_buffer_add_utf8(buffer, element.text.data(),
                               static_cast<int>(element.text.size()),
                               static_cast<unsigned int>(byteStart),
                               static_cast<int>(byteEnd - byteStart));
            hb_buffer_guess_segment_properties(buffer);
            hb_shape(font, buffer, nullptr, 0);

            unsigned int glyphCount = 0;
            const hb_glyph_info_t *infos =
                hb_buffer_get_glyph_infos(buffer, &glyphCount);
            const hb_glyph_position_t *positions =
                hb_buffer_get_glyph_positions(buffer, &glyphCount);
            for (unsigned int i = 0; i < glyphCount; ++i) {
                const float xOffset = positions[i].x_offset / 64.0f;
                const float yOffset = positions[i].y_offset / 64.0f;
                element.glyphs.push_back(
                    {infos[i].codepoint, faceIndex, static_cast<float>(pixelSize),
                     penX, baseline, xOffset, yOffset});
                penX += positions[i].x_advance / 64.0f;
                minX = std::min(minX, penX);
                maxX = std::max(maxX, penX);
            }
            hb_buffer_destroy(buffer);
            hb_font_destroy(font);
            runStart = runEnd;
        }

        if (minX < 0.0f) {
            for (TextGlyph &glyph : element.glyphs) {
                if (glyph.baseline == baseline)
                    glyph.penX -= minX;
            }
        }
        maxLineWidth = std::max(maxLineWidth, maxX - minX);
    }

    uint16_t activePixelSize = 24;
};

FontSystem &GetFontSystem() {
    static FontSystem system;
    return system;
}

} // namespace

void InitializeGlyphAtlas() { GetFontSystem().InitializeAtlas(); }

GLuint GetGlyphAtlasTexture() { return GetFontSystem().AtlasTexture(); }

GlyphAtlasEntry ResolveGlyphAtlasEntry(const TextGlyph &glyph) {
    return GetFontSystem().Resolve(glyph);
}

void TextElement::MeasureLayout(UIStateTables &data, LayoutContext &) {
    if (id == I_UNSET || static_cast<size_t>(id) >= data.geometry.size())
        return;

    GeometryStoreState &geometry = data.geometry[id];
    if (textLayoutDirty) {
        GetFontSystem().Shape(*this, geometry);
    } else {
        geometry.measuredWidth = measuredTextWidth;
        geometry.measuredHeight = measuredTextHeight;
    }
}
