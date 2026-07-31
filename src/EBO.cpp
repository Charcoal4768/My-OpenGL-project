#include"openglBasics/EBO.h"

EBO::EBO(){
    glGenBuffers(1, &ID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ID);
}

void EBO::Bind(){
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ID);
}

void EBO::Data(GLsizeiptr size, const GLuint *indices){
    if (indices == nullptr) return;
    if (size <= 0) return;
    Bind();
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, indices, GL_DYNAMIC_DRAW);
}

void EBO::Unbind(){
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void EBO::Delete(){
    glDeleteBuffers(1, &ID);
}