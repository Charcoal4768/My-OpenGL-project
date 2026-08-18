#ifndef VBO_CLASS_H
#define VBO_CLASS_H

#include <glad/glad.h>

class VBO{
    public:
        GLuint ID;
        VBO();

        void Bind();
        void Unbind();
        void Data(GLsizeiptr size, const void *vertices);
        void Delete();
};

#endif