#version 330 core//we want to use version 330 core
//since we are using opengl 3.3 core

layout (location = 0) in vec3 aPos; //tell our shader
//there is a vec3 called aPos on layout location 0

layout (location = 1) in vec4 aColor;
//tell our shader there is a vec4 in layout location 1
//called aColor (right next to aPos)

out vec4 color; //output the color from the
//vertex data as a vec3
//so our fragment shader
//can use it

uniform vec2 u_resolution;//variable that can be accessed by the CPU

void main()
{
   vec2 ndc = (aPos.xy / u_resolution) * 2.0 - 1.0;

   gl_Position = vec4(ndc.x, -ndc.y, aPos.z, 1.0);

   color = aColor;
}