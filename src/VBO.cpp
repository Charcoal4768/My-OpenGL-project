#include "openglBasics/VBO.h"

VBO::VBO() {
    glGenBuffers(1, &ID);
    glBindBuffer(GL_ARRAY_BUFFER, ID);
}

void VBO::Bind() { glBindBuffer(GL_ARRAY_BUFFER, ID); }

void VBO::Data(GLsizeiptr size, const void *vertices) {
    if (!vertices || size <= 0)
        return;
    Bind();

    if (size > currentCapacity) {
        currentCapacity = size;
        glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_DYNAMIC_DRAW);
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, size, vertices);
    }
}

void VBO::Unbind() { glBindBuffer(GL_ARRAY_BUFFER, 0); }

void VBO::Delete() { glDeleteBuffers(1, &ID); }