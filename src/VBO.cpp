#include"openglBasics/VBO.h"

VBO::VBO(){
    glGenBuffers(1, &ID);
    glBindBuffer(GL_ARRAY_BUFFER, ID);
}

void VBO::Bind(){
    glBindBuffer(GL_ARRAY_BUFFER, ID);
}

void VBO::Data(GLsizeiptr size, const GLfloat *vertices){
    if (vertices == nullptr) return;
    if (size <= 0) return;
    Bind();
    glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_DYNAMIC_DRAW);
}

void VBO::Unbind(){
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void VBO::Delete(){
    glDeleteBuffers(1, &ID);
}