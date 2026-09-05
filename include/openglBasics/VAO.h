#ifndef VAO_CLASS_H
#define VAO_CLASS_H

#include "openglBasics/VBO.h"

struct Layout {
    GLuint index;
    GLuint componentCount;
    GLenum type;
    GLboolean normalized = GL_FALSE;
    GLsizeiptr stride;
    uintptr_t offset;
    GLuint divisor = 0;
    GLboolean preserveInt = GL_FALSE;
};

class VAO {
  public:
    GLuint ID;
    VAO();

    // links a VBO to the VAO
    void LinkAttrib(VBO &VBO, Layout newLayout);
    void Bind();
    void Unbind();
    void Delete();
};

#endif