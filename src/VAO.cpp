#include "openglBasics/VAO.h"

VAO::VAO() { glGenVertexArrays(1, &ID); }

void VAO::LinkAttrib(VBO &VBO, Layout newLayout) {
    VBO.Bind();
    if (newLayout.preserveInt) {
        glVertexAttribIPointer(newLayout.index, newLayout.componentCount,
                               newLayout.type, newLayout.stride,
                               (void *)newLayout.offset);
    } else {
        glVertexAttribPointer(newLayout.index, newLayout.componentCount,
                              newLayout.type, newLayout.normalized, newLayout.stride,
                              (void *)newLayout.offset);
    }
    glEnableVertexAttribArray(newLayout.index);
    glVertexAttribDivisor(newLayout.index, newLayout.divisor);
    VBO.Unbind();
}

void VAO::Bind() { glBindVertexArray(ID); }

void VAO::Unbind() { glBindVertexArray(0); }

void VAO::Delete() { glDeleteVertexArrays(1, &ID); }
