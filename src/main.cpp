#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>

#include "core/window.h"
#include "core/input.h"
#include "map/map.h"
#include "player/player.h"
#include "renderer/map_renderer.h"

const int SCREEN_W = 800;
const int SCREEN_H = 600;

int main() {
    initGLFW();
    GLFWwindow* window = createWindow(SCREEN_W, SCREEN_H, "Raycaster");
    initGLAD();
    glViewport(0, 0, SCREEN_W, SCREEN_H);

    if (!loadMap("resource/maps/map.txt")) {
        fprintf(stderr, "Could not load map, exiting.\n");
        glfwTerminate();
        return -1;
    }

    initPlayer();
    initRenderer(SCREEN_W, SCREEN_H);

    printf("=== Raycaster Controls ===\n");
    printf("W/S or Up/Down   - Move forward/backward\n");
    printf("A/D              - Strafe left/right\n");
    printf("Left/Right arrow - Rotate\n");
    printf("M                - Toggle minimap\n");
    printf("L                - Toggle lighting\n");
    printf("ESC              - Quit\n");
    printf("==========================\n");

    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;
        if (dt > 0.05f) dt = 0.05f;

        processInput(window, dt);

        renderFrame();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    cleanupRenderer();
    glfwTerminate();
    return 0;
}