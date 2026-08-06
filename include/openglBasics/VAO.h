#ifndef VAO_CLASS_H
#define VAO_CLASS_H

#include<glad/glad.h>
#include"openglBasics/VBO.h"

struct Layout{
    GLuint index;
    GLuint componentCount;
    GLenum type;
    GLboolean normalized;
    GLsizeiptr stride;
    GLsizei offset;
};

class VAO{
    public:
        GLuint ID;
        VAO() = default;

        //links a VBO to the VAO
        void LinkAttrib(VBO VBO, Layout newLayout);
        void Bind();
        void Unbind();
        void Delete();
};

#endif