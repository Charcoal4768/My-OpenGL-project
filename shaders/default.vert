#version 330 core

// Instanced Attributes
layout (location = 0) in vec4 a_Transform;
layout (location = 1) in vec4 a_PackedColor;
layout (location = 2) in vec4 a_PackedBorderColor;
layout (location = 3) in uint a_BorderAndCornerInfo;
layout (location = 4) in uint a_ShadowInfo;

uniform vec2 u_resolution;

out vec4 v_Color;
out vec2 v_PosRelToCenter;
flat out vec4 v_BorderWidths;
flat out vec4 v_BorderColor;
flat out vec4 v_CornerRadii;
flat out vec2 v_BoxSize;
flat out float v_ShadowBlur;
flat out vec2  v_ShadowOffset;
flat out vec4  v_ShadowColor;

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

    vec2 elementPos = a_Transform.xy;
    vec2 elementSize = a_Transform.zw;
    
    // Unpack shadow parameters to calculate quad padding
    float blur = float(a_ShadowInfo & 0x3Fu);
    vec2 shadowOffset = vec2(
        float((a_ShadowInfo >> 6u) & 0x3Fu) - 32.0,
        float((a_ShadowInfo >> 12u) & 0x3Fu) - 32.0
    );
    float shadowAlpha = float((a_ShadowInfo >> 30u) & 0x3Fu) / 3.0;

    // Expand quad bounds based on blur radius and direction offset
    vec2 paddingMin = vec2(0.0);
    vec2 paddingMax = vec2(0.0);

    if (shadowAlpha > 0.0 && (blur > 0.0 || length(shadowOffset) > 0.0)) {
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

    v_Color = a_PackedColor;
    v_BorderColor = a_PackedBorderColor; 
    v_BoxSize = elementSize;
 
    // Unpack border & corner info
    v_BorderWidths.x = float(a_BorderAndCornerInfo & 0xFu);
    v_BorderWidths.y = float((a_BorderAndCornerInfo >> 4u) & 0xFu);
    v_BorderWidths.z = float((a_BorderAndCornerInfo >> 8u) & 0xFu);
    v_BorderWidths.w = float((a_BorderAndCornerInfo >> 12u) & 0xFu);

    v_CornerRadii.x = float((a_BorderAndCornerInfo >> 16u) & 0xFu);
    v_CornerRadii.y = float((a_BorderAndCornerInfo >> 20u) & 0xFu);
    v_CornerRadii.z = float((a_BorderAndCornerInfo >> 24u) & 0xFu);
    v_CornerRadii.w = float((a_BorderAndCornerInfo >> 28u) & 0xFu);

    v_ShadowBlur = blur;
    v_ShadowOffset = shadowOffset;

    v_ShadowColor.r = float((a_ShadowInfo >> 18u) & 0xFu) / 15.0; 
    v_ShadowColor.g = float((a_ShadowInfo >> 22u) & 0xFu) / 15.0; 
    v_ShadowColor.b = float((a_ShadowInfo >> 26u) & 0xFu) / 15.0; 
    v_ShadowColor.a = shadowAlpha;  
}