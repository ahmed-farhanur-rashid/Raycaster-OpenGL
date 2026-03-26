#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>

unsigned int compileShader(unsigned int type, const char* src);
unsigned int createShaderProgram(const char* vertSrc, const char* fragSrc);

#endif