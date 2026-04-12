#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>

#include "core/window.h"
#include "core/input.h"
#include "map/map.h"
#include "player/player.h"
#include "renderer/map_renderer.h"
#include "renderer/hud_renderer.h"
#include "entities/projectile.h"
#include "entities/enemy.h"

const int SCREEN_W = 800;
const int SCREEN_H = 600;

int main() {
    window::initGLFW();
    GLFWwindow* window = window::createWindow(SCREEN_W, SCREEN_H, "Raycaster");
    window::initGLAD();
    glViewport(0, 0, SCREEN_W, SCREEN_H);

    player::initPlayer();
    renderer::initRenderer(SCREEN_W, SCREEN_H);
    hud::initHUD(SCREEN_W, SCREEN_H);

    // Load map AFTER sprite registry is initialized
    if (!map::loadMap("resource/maps/map.txt")) {
        fprintf(stderr, "Could not load map, exiting.\n");
        glfwTerminate();
        return -1;
    }
    
    // Upload map texture to GPU after loading
    renderer::uploadMapTexture();

    printf("=== Raycaster Controls ===\n");
    printf("W/S or Up/Down   - Move forward/backward\n");
    printf("A/D              - Strafe left/right\n");
    printf("Left/Right arrow - Rotate\n");
    printf("M                - Toggle minimap\n");
    printf("L                - Toggle lighting\n");
    printf("ESC              - Quit\n");
    printf("==========================\n");

    double lastTime = glfwGetTime();
    bool gameOver = false;

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float deltaTime = (float)(now - lastTime);
        lastTime = now;
        if (deltaTime > 0.05f) deltaTime = 0.05f;

        if (!gameOver) {
            input::processInput(window, deltaTime);
            projectile::updateProjectiles(deltaTime);
            enemy::updateEnemies(deltaTime);
            player::checkPickups(deltaTime);  // <-- ADD: check for pickups
            
            // Check if player died
            if (player::isDead()) {
                gameOver = true;
                printf("GAME OVER! Press ENTER to restart.\n");
            }
        } else {
            // Game over screen - wait for ENTER to restart
            if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) {
                // Reset player
                player::initPlayer();
                // Reload map to respawn enemies and pickups
                map::loadMap("resource/maps/map.txt");
                renderer::uploadMapTexture();
                gameOver = false;
                printf("Game restarted!\n");
                
                // Force a render frame to clear any OpenGL state issues
                glClear(GL_COLOR_BUFFER_BIT);
                glfwSwapBuffers(window);
            }
        }

        renderer::renderFrame();
        if (!gameOver) {
            hud::renderWeapon();
            hud::renderHUD();
        } else {
            hud::renderGameOver();
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    renderer::cleanupRenderer();
    hud::cleanupHUD();
    glfwTerminate();
    return 0;
}