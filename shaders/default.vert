#version 330 core//we want to use version 330 core
//since we are using opengl 3.3 core

layout (location = 0) in vec3 aPos;

layout (location = 1) in vec4 aColor;

layout (location = 2) in vec2 aUV;

out vec4 color;
out vec2 uv;

uniform vec2 u_resolution;//variable that can be accessed by the CPU

void main()
{
   vec2 ndc = (aPos.xy / u_resolution) * 2.0 - 1.0;

   gl_Position = vec4(ndc.x, -ndc.y, aPos.z, 1.0);

   //GPU automatically interpolates these variables across fragments
   color = aColor;
   uv = aUV;
}