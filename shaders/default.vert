#version 330 core

layout (location = 0) in uint a_HardwareID;
layout (location = 1) in vec4 a_Transform;
layout (location = 2) in uvec4 a_Payload0;
layout (location = 3) in uvec3 a_Payload1;

uniform vec2 u_resolution;

out vec4 v_Color;
out vec2 v_PosRelToCenter;
out vec2 v_TexCoord;
flat out uint v_ElementType;
flat out float v_TextureLayer;
flat out vec4 v_BorderWidths;
flat out vec4 v_BorderColor;
flat out vec4 v_CornerRadii;
flat out vec2 v_BoxSize;
flat out float v_ShadowBlur;
flat out vec2  v_ShadowOffset;
flat out vec4  v_ShadowColor;

vec4 unpackRGBA8(uint color)
{
    return vec4(
        float((color >> 24u) & 0xFFu),
        float((color >> 16u) & 0xFFu),
        float((color >> 8u) & 0xFFu),
        float(color & 0xFFu)
    ) / 255.0;
}

vec2 unpackNormalizedUv(uint packedValue)
{
    return vec2(float(packedValue & 0xFFFFu),
                float(packedValue >> 16u)) / 65535.0;
}

const vec2 quadVertices[6] = vec2[](
    vec2(0.0, 0.0), // Triangle 1
    vec2(1.0, 0.0),
    vec2(0.0, 1.0),
    
    vec2(0.0, 1.0), // Triangle 2
    vec2(1.0, 0.0),
    vec2(1.0, 1.0)
);

void main()
{
    vec2 localCoord = quadVertices[gl_VertexID];
    uint elementType = a_HardwareID & 0xFu;
    bool isShape = elementType == 0u;
    bool isText = elementType == 2u;
    v_ElementType = elementType;
    v_TextureLayer = isText ? float(a_Payload0.x) : 0.0;
    vec2 uv0 = unpackNormalizedUv(a_Payload0.y);
    vec2 uv1 = unpackNormalizedUv(a_Payload0.z);
    v_TexCoord = isText ? mix(uv0, uv1, localCoord) : vec2(0.0);

    vec2 elementPos = a_Transform.xy;
    vec2 elementSize = a_Transform.zw;
    
    // Unpack shadow parameters to calculate quad padding
    float blur = isShape ? float(a_Payload0.w & 0x3Fu) : 0.0;
    vec2 shadowOffset = isShape ? vec2(
        float((a_Payload0.w >> 6u) & 0x3Fu) - 32.0,
        float((a_Payload0.w >> 12u) & 0x3Fu) - 32.0
    ) : vec2(0.0);
    v_ShadowColor = isShape ? unpackRGBA8(a_Payload1.x) : vec4(0.0);

    // Expand quad bounds based on blur radius and direction offset
    vec2 paddingMin = vec2(0.0);
    vec2 paddingMax = vec2(0.0);

    if (v_ShadowColor.a > 0.0 && (blur > 0.0 || length(shadowOffset) > 0.0)) {
        paddingMin = max(vec2(0.0), vec2(blur) - shadowOffset);
        paddingMax = max(vec2(0.0), vec2(blur) + shadowOffset);
    }

    vec2 expandedPos = elementPos - paddingMin;
    vec2 expandedSize = elementSize + paddingMin + paddingMax;

    vec2 actualPixelPos = expandedPos + localCoord * expandedSize;

    // Position relative to center of original unexpanded element box
    v_PosRelToCenter = (actualPixelPos - elementPos) - (elementSize * 0.5);

    // Convert pixel space to Top-Left NDC
    vec2 ndc = (actualPixelPos / u_resolution) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);

    v_Color = isShape ? unpackRGBA8(a_Payload0.x)
                      : (isText ? unpackRGBA8(a_Payload0.w) : vec4(0.0));
    v_BorderColor = isShape ? unpackRGBA8(a_Payload0.y) : vec4(0.0);
    v_BoxSize = elementSize;
 
    // Unpack border & corner info
    v_BorderWidths = isShape
        ? vec4(float(a_Payload0.z & 0xFu),
               float((a_Payload0.z >> 4u) & 0xFu),
               float((a_Payload0.z >> 8u) & 0xFu),
               float((a_Payload0.z >> 12u) & 0xFu))
        : vec4(0.0);
    v_CornerRadii = isShape
        ? vec4(float((a_Payload0.z >> 16u) & 0xFu),
               float((a_Payload0.z >> 20u) & 0xFu),
               float((a_Payload0.z >> 24u) & 0xFu),
               float((a_Payload0.z >> 28u) & 0xFu))
        : vec4(0.0);

    v_ShadowBlur = blur;
    v_ShadowOffset = shadowOffset;
}