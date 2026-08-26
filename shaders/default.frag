#version 330 core//we want to use version 330 core
//since we are using opengl 3.3 core

out vec4 FragColor; 

in vec4 color; 
in vec2 uv;

void main()
{
   FragColor = vec4(color);
}