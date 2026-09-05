#version 330 core//we want to use version 330 core
//since we are using opengl 3.3 core

out vec4 FragColor; 

in vec4 v_Color; 
in vec2 v_UV;

flat in vec4 v_BorderWidths;
flat in vec4 v_BorderColor;
flat in vec4 v_CornerRadii;


void main()
{
   FragColor = vec4(v_Color);
}