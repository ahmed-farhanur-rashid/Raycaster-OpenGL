#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>

#include "core/window.h"
#include "core/input.h"
#include "core/game_state.h"
#include "map/map.h"
#include "player/player.h"
#include "entity/entity.h"
#include "renderer/map_renderer.h"
#include "renderer/minimap_renderer.h"
#include "renderer/hud_renderer.h"
#include "renderer/entity_renderer.h"
#include "renderer/menu_renderer.h"
#include "settings/settings.h"

/* ---- menu navigation state ---- */
static int  menuSelection   = 0;
static int  pauseSelection  = 0;
static bool escWasPressed   = false;
static bool upWasPressed    = false;
static bool downWasPressed  = false;
static bool enterWasPressed = false;

static void resetGame() {
    player::initPlayer();
    entity::initSpawner();
}

int main() {
    /* load settings before anything else (no GL needed) */
    settings::load("src/settings/config.json", "src/settings/keybind.json");

    const int SCREEN_W = settings::getInt("screen_width",  1280);
    const int SCREEN_H = settings::getInt("screen_height", 720);

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
    minimap::initMinimap(SCREEN_W, SCREEN_H);
    hud::initHUD(SCREEN_W, SCREEN_H);
    entity_renderer::initEntityRenderer(SCREEN_W, SCREEN_H);
    menu_renderer::initMenu(SCREEN_W, SCREEN_H);
    entity::initSpawner();

    /* start in main menu — show cursor */
    game::setState(GameState::MAIN_MENU);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float deltaTime = (float)(now - lastTime);
        lastTime = now;
        if (deltaTime > 0.05f) deltaTime = 0.05f;

        /* ---- read menu keys (edge-detected) ---- */
        bool escNow   = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        bool upNow    = glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS ||
                        glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
        bool downNow  = glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS ||
                        glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
        bool enterNow = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS;

        bool escEdge   = escNow   && !escWasPressed;
        bool upEdge    = upNow    && !upWasPressed;
        bool downEdge  = downNow  && !downWasPressed;
        bool enterEdge = enterNow && !enterWasPressed;

        escWasPressed   = escNow;
        upWasPressed    = upNow;
        downWasPressed  = downNow;
        enterWasPressed = enterNow;

        /* ================================================ */
        /*  MAIN MENU                                        */
        /* ================================================ */
        if (game::getState() == GameState::MAIN_MENU) {
            if (upEdge)   menuSelection = (menuSelection - 1 + 2) % 2;
            if (downEdge) menuSelection = (menuSelection + 1) % 2;

            if (enterEdge) {
                if (menuSelection == 0) {
                    /* START */
                    resetGame();
                    game::setState(GameState::PLAYING);
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    input::initMouse(window);
                } else {
                    /* QUIT */
                    glfwSetWindowShouldClose(window, true);
                }
            }
            if (escEdge) {
                glfwSetWindowShouldClose(window, true);
            }

            glClear(GL_COLOR_BUFFER_BIT);
            menu_renderer::renderMainMenuSelect(menuSelection);

            glfwSwapBuffers(window);
            glfwPollEvents();
            continue;
        }

        /* ================================================ */
        /*  PAUSED                                           */
        /* ================================================ */
        if (game::getState() == GameState::PAUSED) {
            if (upEdge)   pauseSelection = (pauseSelection - 1 + 3) % 3;
            if (downEdge) pauseSelection = (pauseSelection + 1) % 3;

            if (enterEdge) {
                if (pauseSelection == 0) {
                    /* CONTINUE */
                    game::setState(GameState::PLAYING);
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    input::initMouse(window);
                } else if (pauseSelection == 1) {
                    /* MAIN MENU */
                    game::setState(GameState::MAIN_MENU);
                    menuSelection = 0;
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                } else {
                    /* QUIT */
                    glfwSetWindowShouldClose(window, true);
                }
            }
            if (escEdge) {
                /* ESC in pause = continue */
                game::setState(GameState::PLAYING);
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                input::initMouse(window);
            }

            /* render the game frame behind the pause overlay */
            renderer::RenderState rs;
            rs.posX   = player::player.posX;
            rs.posY   = player::player.posY;
            rs.dirX   = player::player.dirX;
            rs.dirY   = player::player.dirY;
            rs.planeX = player::player.planeX;
            rs.planeY = player::player.planeY;
            rs.posZ   = player::player.posZ;
            rs.lightingEnabled = input::lightingEnabled;
            rs.minimapEnabled  = input::minimapEnabled;

            renderer::renderFrame(rs);
            entity_renderer::renderEntities(rs);
            minimap::renderMinimap(rs);
            hud::renderWeapon();
            menu_renderer::renderPauseMenu(pauseSelection);

            glfwSwapBuffers(window);
            glfwPollEvents();
            continue;
        }

        /* ================================================ */
        /*  PLAYING                                          */
        /* ================================================ */
        if (escEdge) {
            game::setState(GameState::PAUSED);
            pauseSelection = 0;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            glfwPollEvents();
            continue;
        }

        input::processInput(window, deltaTime);

        /* update entity spawning, AI, and combat */
        entity::updateSpawner(deltaTime,
                              player::player.posX, player::player.posY,
                              0, 1, 3, 2);  /* ghost sprite layer indices */
        for (auto& e : entity::entities) {
            if (e.type == EntityType::GHOST)
                entity::updateGhost(e, deltaTime,
                                    player::player.posX, player::player.posY);
        }
        entity::updateCombat(deltaTime, player::player.posX, player::player.posY);

        renderer::RenderState rs;
        rs.posX   = player::player.posX;
        rs.posY   = player::player.posY;
        rs.dirX   = player::player.dirX;
        rs.dirY   = player::player.dirY;

        rs.planeX = player::player.planeX;
        rs.planeY = player::player.planeY;

        rs.posZ   = player::player.posZ;
        rs.lightingEnabled = input::lightingEnabled;
        rs.minimapEnabled  = input::minimapEnabled;

        renderer::renderFrame(rs);
        entity_renderer::renderEntities(rs);
        minimap::renderMinimap(rs);
        hud::renderWeapon();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    menu_renderer::cleanupMenu();
    hud::cleanupHUD();
    entity_renderer::cleanupEntityRenderer();
    minimap::cleanupMinimap();
    renderer::cleanupRenderer();
    glfwTerminate();
    return 0;
}