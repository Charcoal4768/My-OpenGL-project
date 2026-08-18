#ifndef EBO_CLASS_H
#define EBO_CLASS_H

#include <glad/glad.h>

class EBO {
  public:
    GLuint ID;
    EBO();

    void Bind();
    void Unbind();
    void Data(GLsizeiptr size, const void *indices);
    void Delete();
};

#endif