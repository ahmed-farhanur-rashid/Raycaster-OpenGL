#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>

#include "core/window.h"
#include "core/input.h"
#include "core/game_state.h"
#include "map/map.h"
#include "player/player.h"
#include "projectile/projectile.h"
#include "renderer/map_renderer.h"
#include "renderer/minimap_renderer.h"
#include "renderer/hud_renderer.h"
#include "renderer/projectile_renderer.h"
#include "renderer/menu_renderer.h"
#include "settings/settings.h"

int main() {
    settings::load("src/settings/config.json", "src/settings/keybind.json");

    const int SCREEN_W = settings::getInt("screen_width",  960);
    const int SCREEN_H = settings::getInt("screen_height", 720);

    window::initGLFW();
    glfwSwapInterval(1);
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
    minimap::initMinimap(SCREEN_W, SCREEN_H);
    hud::initHUD(SCREEN_W, SCREEN_H);
    projectile_renderer::initProjectileRenderer(SCREEN_W, SCREEN_H);
    menu_renderer::initMenu(SCREEN_W, SCREEN_H);
    projectile::initProjectiles();

    game::setState(GameState::MAIN_MENU);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float deltaTime = (float)(now - lastTime);
        lastTime = now;
        float maxDt = settings::getFloat("max_delta_time", 0.05f);
        if (deltaTime > maxDt) deltaTime = maxDt;

        game::updateMenuInput(window);

        switch (game::getState()) {

        case GameState::MAIN_MENU:
            glClear(GL_COLOR_BUFFER_BIT);
            menu_renderer::renderMainMenuSelect(game::menuSelection());
            break;

        case GameState::PAUSED: {
            renderer::RenderState rs = renderer::buildRenderState();
            renderer::renderFrame(rs);
            projectile_renderer::renderProjectiles(rs);
            minimap::renderMinimap(rs);
            hud::renderWeapon();
            menu_renderer::renderPauseMenu(game::pauseSelection());
            break;
        }

        case GameState::PLAYING: {
            input::processInput(window, deltaTime);
            projectile::updateProjectiles(deltaTime);

            renderer::RenderState rs = renderer::buildRenderState();
            renderer::renderFrame(rs);
            projectile_renderer::renderProjectiles(rs);
            minimap::renderMinimap(rs);
            hud::renderWeapon();
            break;
        }
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    menu_renderer::cleanupMenu();
    hud::cleanupHUD();
    projectile_renderer::cleanupProjectileRenderer();
    minimap::cleanupMinimap();
    renderer::cleanupRenderer();
    glfwTerminate();
    return 0;
}