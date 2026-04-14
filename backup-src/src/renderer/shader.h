#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>

namespace shader {
    unsigned int compileShader(unsigned int type, const char* src);
    unsigned int createShaderProgram(const char* vertSrc, const char* fragSrc);
}

#endif