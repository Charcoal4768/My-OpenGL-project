#version 330 core//we want to use version 330 core
//since we are using opengl 3.3 core

layout (location = 0) in vec4 a_Transform;

layout (location = 1) in vec4 a_PackedColor;

layout (location = 2) in uvec2 a_BorderInfo;

layout (location = 3) in uint a_CornerInfo;

uniform vec2 u_resolution;//variable that can be accessed by the CPU is called a uniform

out vec4 v_Color;
out vec2 v_LocalUV;
//flat just means dont interpolate
flat out vec4 v_BorderWidths;
flat out vec4 v_BorderColor;
flat out vec4 v_CornerRadii;
flat out vec2 v_BoxSize;

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
   v_LocalUV = localCoord;

   vec2 elementPos = a_Transform.xy;
   vec2 elementSize = a_Transform.zw;
   vec2 actualPixelPos = elementPos + (localCoord * elementSize);

   vec2 ndc = (actualPixelPos / u_resolution) * 2.0 - 1.0;
   gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);

   v_Color = a_PackedColor;
   v_BoxSize = elementSize;

   v_CornerRadii.x = float(a_CornerInfo & 0xFFu); //0xFF = 11111111 = 8 bits
   v_CornerRadii.y = float((a_CornerInfo >> 8u) & 0xFFu);
   v_CornerRadii.z = float((a_CornerInfo >> 16u) & 0xFFu);
   v_CornerRadii.w = float((a_CornerInfo >> 24u) & 0xFFu);

   uint widthBits = a_BorderInfo.x;
   v_BorderWidths.x = float(widthBits & 0xFFu);
   v_BorderWidths.y = float((widthBits >> 8u) & 0xFFu);
   v_BorderWidths.z = float((widthBits >> 16u) & 0xFFu);
   v_BorderWidths.w = float((widthBits >> 24u) & 0xFFu);

   uint colorBits = a_BorderInfo.y;
   v_BorderColor.x = float(colorBits & 0xFFu) / 255.0;
   v_BorderColor.y = float((colorBits >> 8u) & 0xFFu) / 255.0;
   v_BorderColor.z = float((colorBits >> 16u) & 0xFFu) / 255.0;
   v_BorderColor.w = float((colorBits >> 24u) & 0xFFu) / 255.0;
}