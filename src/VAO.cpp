#include "openglBasics/VAO.h"

VAO::VAO() { glGenVertexArrays(1, &ID); }

void VAO::LinkAttrib(VBO &VBO, Layout newLayout) {
    VBO.Bind();
    glVertexAttribPointer(newLayout.index, newLayout.componentCount, newLayout.type,
                          GL_FALSE, newLayout.stride,
                          (void *)newLayout.offset); // The VAO
    glEnableVertexAttribArray(newLayout.index);
    glVertexAttribDivisor(newLayout.index, newLayout.divisor);
    VBO.Unbind();
}

void VAO::Bind() { glBindVertexArray(ID); }

void VAO::Unbind() { glBindVertexArray(0); }

void VAO::Delete() { glDeleteVertexArrays(1, &ID); }
