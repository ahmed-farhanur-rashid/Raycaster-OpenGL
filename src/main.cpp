#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>

#include "core/window.h"
#include "core/input.h"
#include "map/map.h"
#include "player/player.h"
#include "renderer/map_renderer.h"
#include "renderer/hud_renderer.h"
#include "settings/settings.h"

int main() {
    /* load settings before anything else (no GL needed) */
    settings::load("resource/config.json", "resource/keybind.json");

    const int SCREEN_W = settings::getInt("screen_width",  800);
    const int SCREEN_H = settings::getInt("screen_height", 600);

    window::initGLFW();
    glfwSwapInterval(1); // enable vsync
    GLFWwindow* window = window::createWindow(SCREEN_W, SCREEN_H, "Raycaster");
    window::initGLAD();
    glViewport(0, 0, SCREEN_W, SCREEN_H);

    if (!map::loadMap("resource/maps/map.txt")) {
        fprintf(stderr, "Could not load map, exiting.\n");
        glfwTerminate();
        return -1;
    }

    player::initPlayer();
    renderer::initRenderer(SCREEN_W, SCREEN_H);
    hud::initHUD(SCREEN_W, SCREEN_H);
    input::initMouse(window);

    printf("=== Raycaster Controls ===\n");
    printf("W/S or Up/Down   - Move forward/backward\n");
    printf("A/D              - Strafe left/right\n");
    printf("Shift            - Sprint\n");
    printf("Left/Right arrow - Rotate\n");
    printf("Mouse            - Look left/right\n");
    printf("Space            - Jump\n");
    printf("M                - Toggle minimap\n");
    printf("L                - Toggle lighting\n");
    printf("1                - Equip Assault Rifle\n");
    printf("2                - Equip Shotgun\n");
    printf("3                - Equip Energy Weapon\n");
    printf("4                - Equip Handgun\n");
    printf("LMB              - Fire / Normal fire\n");
    printf("RMB              - Grenade / Overcharge\n");
    printf("R                - Reload\n");
    printf("ESC              - Quit\n");
    printf("==========================\n");

    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float deltaTime = (float)(now - lastTime);
        lastTime = now;
        if (deltaTime > 0.05f) deltaTime = 0.05f;

        input::processInput(window, deltaTime);

        renderer::renderFrame();
        hud::renderWeapon();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    hud::cleanupHUD();
    renderer::cleanupRenderer();
    glfwTerminate();
    return 0;
}