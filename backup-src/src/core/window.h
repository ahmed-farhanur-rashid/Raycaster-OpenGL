#ifndef WINDOW_H
#define WINDOW_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace window {
    void initGLFW();
    GLFWwindow* createWindow(int width, int height, const char* title);
    void initGLAD();
    void framebufferSizeCallback(GLFWwindow* window, int width, int height);
}

#endif