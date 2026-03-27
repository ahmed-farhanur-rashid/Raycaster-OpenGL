#ifndef INPUT_H
#define INPUT_H

#include <GLFW/glfw3.h>

namespace input {
    void processInput(GLFWwindow* window, float dt);

    extern bool minimapEnabled;
    extern bool lightingEnabled;
}

#endif