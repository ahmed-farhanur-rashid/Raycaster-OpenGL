#include "input.h"
#include "../player/player.h"
#include "../renderer/hud_renderer.h"
#include "../entity/entity.h"
#include "../settings/settings.h"
#include <cmath>

namespace input {

bool minimapEnabled = true;
bool lightingEnabled = true;

static bool lKeyWasPressed = false;
static bool mKeyWasPressed = false;
static bool rKeyWasPressed = false;
static bool key1WasPressed = false;
static bool key2WasPressed = false;
static bool key3WasPressed = false;
static bool key4WasPressed = false;
static bool rmbWasPressed  = false;
static double lastMouseX = 0.0;
static bool   mousePrimed = false;          // skip the first frame's garbage delta

void initMouse(GLFWwindow* window) {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void processInput(GLFWwindow* window, float deltaTime) {
    // ESC is handled by main.cpp game state logic now

    // run physics every frame
    player::updatePhysics(deltaTime);

    // jump
    if (glfwGetKey(window, settings::getKey("jump", GLFW_KEY_SPACE)) == GLFW_PRESS)
        player::jump();

    // horizontal mouse look
    {
        const float MOUSE_SENSITIVITY = settings::getFloat("mouse_sensitivity", 0.002f);

        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        if (!mousePrimed) {
            lastMouseX  = mouseX;
            mousePrimed = true;
        } else {
            double deltaX = mouseX - lastMouseX;
            lastMouseX    = mouseX;

            if (deltaX != 0.0)
                player::rotatePlayer((float)(deltaX * MOUSE_SENSITIVITY));
        }
    }

    float forward = 0.0f;
    float strafe = 0.0f;

    if (glfwGetKey(window, settings::getKey("move_forward", GLFW_KEY_W)) == GLFW_PRESS)
        forward += 1.0f;
    if (glfwGetKey(window, settings::getKey("move_backward", GLFW_KEY_S)) == GLFW_PRESS)
        forward -= 1.0f;
    if (glfwGetKey(window, settings::getKey("strafe_right", GLFW_KEY_D)) == GLFW_PRESS)
        strafe += 1.0f;
    if (glfwGetKey(window, settings::getKey("strafe_left", GLFW_KEY_A)) == GLFW_PRESS)
        strafe -= 1.0f;

    bool sprinting = glfwGetKey(window, settings::getKey("sprint", GLFW_KEY_LEFT_SHIFT)) == GLFW_PRESS;

    if (forward != 0.0f || strafe != 0.0f)
        player::movePlayer(forward, strafe, deltaTime, sprinting);

    if (glfwGetKey(window, settings::getKey("rotate_left", GLFW_KEY_LEFT)) == GLFW_PRESS)
        player::rotatePlayer(-player::player.rotSpeed * deltaTime);
    if (glfwGetKey(window, settings::getKey("rotate_right", GLFW_KEY_RIGHT)) == GLFW_PRESS)
        player::rotatePlayer(player::player.rotSpeed * deltaTime);

    bool mNow = glfwGetKey(window, settings::getKey("toggle_minimap", GLFW_KEY_M)) == GLFW_PRESS;
    if (mNow && !mKeyWasPressed) minimapEnabled = !minimapEnabled;
    mKeyWasPressed = mNow;

    bool lNow = glfwGetKey(window, settings::getKey("toggle_lighting", GLFW_KEY_L)) == GLFW_PRESS;
    if (lNow && !lKeyWasPressed) lightingEnabled = !lightingEnabled;
    lKeyWasPressed = lNow;

    /* weapon controls */
    bool key1Now = glfwGetKey(window, settings::getKey("equip_assault_rifle", GLFW_KEY_1)) == GLFW_PRESS;
    if (key1Now && !key1WasPressed) hud::equipAssaultRifle();
    key1WasPressed = key1Now;

    bool key2Now = glfwGetKey(window, settings::getKey("equip_shotgun", GLFW_KEY_2)) == GLFW_PRESS;
    if (key2Now && !key2WasPressed) hud::equipShotgun();
    key2WasPressed = key2Now;

    bool key3Now = glfwGetKey(window, settings::getKey("equip_energy_weapon", GLFW_KEY_3)) == GLFW_PRESS;
    if (key3Now && !key3WasPressed) hud::equipEnergyWeapon();
    key3WasPressed = key3Now;

    bool key4Now = glfwGetKey(window, settings::getKey("equip_handgun", GLFW_KEY_4)) == GLFW_PRESS;
    if (key4Now && !key4WasPressed) hud::equipHandgun();
    key4WasPressed = key4Now;

    bool lmbNow = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool rmbNow = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    bool rNow = glfwGetKey(window, settings::getKey("reload", GLFW_KEY_R)) == GLFW_PRESS;
    if (rNow && !rKeyWasPressed) hud::reload();
    rKeyWasPressed = rNow;

    /* update HUD bob + animation (clears firedThisFrame/firedAltThisFrame) */
    bool isMoving = (forward != 0.0f || strafe != 0.0f);
    hud::updateHUD(isMoving, deltaTime, lmbNow, rmbNow);

    /* fire grenade AFTER updateHUD so firedAltThisFrame survives the clear */
    if (hud::currentWeapon() != WeaponType::ENERGY_WEAPON) {
        if (rmbNow && !rmbWasPressed) hud::fireGrenade();
    }
    rmbWasPressed = rmbNow;

    /* detect weapon fire → spawn projectile
       Uses firedThisFrame() which is set by the weapon's auto-fire logic,
       so this works for single-shot AND auto-fire weapons.               */
    if (hud::firedThisFrame()) {
        WeaponType wt = hud::currentWeapon();
        float px = player::player.posX;
        float py = player::player.posY;
        float pz = player::player.posZ;
        float dx = player::player.dirX;
        float dy = player::player.dirY;
        /* perpendicular (right) vector for slight muzzle offset */
        float rx = -dy, ry = dx;
        float oFwd  = settings::getFloat("proj_offset_forward", 0.6f);
        float oSide = settings::getFloat("proj_offset_side",    0.15f);

        switch (wt) {
        case WeaponType::ASSAULT_RIFLE: {
            float range = settings::getFloat("ar_range",  20.0f);
            entity::spawnProjectile(px + dx * oFwd + rx * oSide,
                                    py + dy * oFwd + ry * oSide,
                                    dx, dy, 30.0f, ProjOwner::PLAYER, 0,
                                    range, 0.06f, pz);
            break;
        }
        case WeaponType::SHOTGUN: {
            float range = settings::getFloat("sg_range",  8.0f);
            entity::spawnProjectile(px + dx * oFwd + rx * oSide,
                                    py + dy * oFwd + ry * oSide,
                                    dx, dy, 18.0f, ProjOwner::PLAYER, 2,
                                    range, 0.18f, pz);
            break;
        }
        case WeaponType::ENERGY_WEAPON: {
            float range = settings::getFloat("en_range",  25.0f);
            entity::spawnProjectile(px + dx * oFwd + rx * oSide,
                                    py + dy * oFwd + ry * oSide,
                                    dx, dy, 20.0f, ProjOwner::PLAYER, 1,
                                    range, 0.10f, pz);
            break;
        }
        case WeaponType::HANDGUN: {
            float range = settings::getFloat("hg_range",  18.0f);
            entity::spawnProjectile(px + dx * oFwd + rx * oSide,
                                    py + dy * oFwd + ry * oSide,
                                    dx, dy, 28.0f, ProjOwner::PLAYER, 0,
                                    range, 0.07f, pz);
            break;
        }
        default: break;
        }
    }

    /* alt-fire (grenade / energy RMB) → spawn projectile */
    if (hud::firedAltThisFrame()) {
        WeaponType wt = hud::currentWeapon();
        float px = player::player.posX;
        float py = player::player.posY;
        float pz = player::player.posZ;
        float dx = player::player.dirX;
        float dy = player::player.dirY;
        float rx = -dy, ry = dx;
        float oFwd  = settings::getFloat("proj_offset_forward", 0.6f);
        float oSide = settings::getFloat("proj_offset_side",    0.15f);

        switch (wt) {
        case WeaponType::ASSAULT_RIFLE: {
            float range = settings::getFloat("ar_grenade_range",  6.0f);
            entity::spawnProjectile(px + dx * oFwd + rx * oSide,
                                    py + dy * oFwd + ry * oSide,
                                    dx, dy, 12.0f, ProjOwner::PLAYER, 2,
                                    range, 0.22f, pz);
            break;
        }
        case WeaponType::ENERGY_WEAPON: {
            float range = settings::getFloat("en_range",  25.0f);
            entity::spawnProjectile(px + dx * oFwd + rx * oSide,
                                    py + dy * oFwd + ry * oSide,
                                    dx, dy, 20.0f, ProjOwner::PLAYER, 1,
                                    range, 0.10f, pz);
            break;
        }
        default: break;
        }
    }
}

} // namespace input