#include <glad/glad.h>
#include "hud_renderer.h"
#include "shader.h"
#include <stb/stb_image.h>
#include <cstdio>
#include <cmath>
#include <cstring>
#include "../player/player.h"

namespace hud {

static unsigned int prog, vao, vbo, weaponTex;
static int scrW, scrH;
static float bobTimer  = 0.0f;
static float bobOffset = 0.0f;  // NDC units
static float recoilOffset = 0.0f;  // Weapon recoil (positive = closer to camera)

// ---- HUD Shaders ----
static const char* hudVsrc = R"(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec4 aColor;
out vec4 vColor;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vColor = aColor;
}
)";

static const char* hudFsrc = R"(
#version 330 core
in  vec4 vColor;
out vec4 fragColor;
void main() {
    fragColor = vColor;
}
)";

static unsigned int hudProg, hudVao, hudVbo;

// Each vertex: x, y (NDC), r, g, b, a
static float hudVerts[512 * 6];  // buffer for many quads
static int   hudVertCount = 0;

// ---- Shaders ----
static const char* vsrc = R"(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
out vec2 uv;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    uv = aUV;
}
)";

static const char* fsrc = R"(
#version 330 core
in  vec2 uv;
out vec4 fragColor;
uniform sampler2D weaponTex;
void main() {
    vec4 c = texture(weaponTex, uv);
    if (c.a < 0.1) discard;   // alpha clip for transparency
    fragColor = c;
}
)";

void initHUD(int screenW, int screenH) {
    scrW = screenW;
    scrH = screenH;

    prog = shader::createShaderProgram(vsrc, fsrc);

    // Initial quad (will be updated each frame with bob)
    // Weapon positioning - modify these values to adjust:
    // x0/x1 control width (currently spans from -0.6 to 0.6 in NDC)
    // y0/y1 control height and position (y0=-1.0 is bottom, y1=-0.05 gives taller weapon)
    // To make it LARGER, increase the range:
    float x0 = -1.0f, x1 = 1.0f;   // Wider (1.6 units)
    float y0 = -0.75f, y1 = 0.75f;   // Taller (1.0 unit)
    float quad[] = {
        x0, y0,   0.0f, 1.0f,
        x1, y0,   1.0f, 1.0f,
        x0, y1,   0.0f, 0.0f,
        x1, y1,   1.0f, 0.0f,
    };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // Load weapon texture
    int w, h, ch;
    unsigned char* data = stbi_load("resource/textures/weapon_pistol.png", &w, &h, &ch, 4);
    glGenTextures(1, &weaponTex);
    glBindTexture(GL_TEXTURE_2D, weaponTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    if (data) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
    } else {
        fprintf(stderr, "WARNING: could not load weapon texture\n");
        unsigned char fallback[4] = {200, 200, 200, 255};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, fallback);
    }

    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "weaponTex"), 0);

    // ---- Initialize HUD VAO/VBO ----
    hudProg = shader::createShaderProgram(hudVsrc, hudFsrc);

    glGenVertexArrays(1, &hudVao);
    glGenBuffers(1, &hudVbo);
    glBindVertexArray(hudVao);
    glBindBuffer(GL_ARRAY_BUFFER, hudVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(hudVerts), nullptr, GL_DYNAMIC_DRAW);

    // Position (location 0): 2 floats
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Colour (location 1): 4 floats
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void updateBob(bool isMoving, float deltaTime) {
    if (isMoving) bobTimer += deltaTime * 8.0f;
    else bobTimer = 0.0f;
    bobOffset = isMoving ? sinf(bobTimer) * 0.04f : 0.0f;
}

void updateRecoil(bool isFiring, float deltaTime) {
    const float RECOIL_SPEED = 8.0f;   // How fast weapon kicks back
    const float RECOVERY_SPEED = 16.0f;  // How fast it returns
    const float MAX_RECOIL = 0.05f;     // Maximum recoil distance
    
    if (isFiring) {
        // Kick weapon towards camera
        recoilOffset += RECOIL_SPEED * deltaTime;
        if (recoilOffset > MAX_RECOIL) recoilOffset = MAX_RECOIL;
    } else {
        // Recover to normal position
        recoilOffset -= RECOVERY_SPEED * deltaTime;
        if (recoilOffset < 0.0f) recoilOffset = 0.0f;
    }
}

// ---- HUD Quad Drawing Utilities ----
static float pxToNdcX(float px) { return  2.0f * px / scrW - 1.0f; }
static float pxToNdcY(float py) { return -2.0f * py / scrH + 1.0f; }

static void pushQuad(float x0, float y0, float x1, float y1,
                     float r, float g, float b, float a) {
    // Convert pixel coords to NDC
    float nx0 = pxToNdcX(x0), ny0 = pxToNdcY(y0);
    float nx1 = pxToNdcX(x1), ny1 = pxToNdcY(y1);

    float v[] = {
        nx0, ny0, r, g, b, a,
        nx1, ny0, r, g, b, a,
        nx0, ny1, r, g, b, a,
        nx1, ny0, r, g, b, a,
        nx1, ny1, r, g, b, a,
        nx0, ny1, r, g, b, a,
    };
    if (hudVertCount + 6 > 512) return;
    memcpy(&hudVerts[hudVertCount * 6], v, sizeof(v));
    hudVertCount += 6;
}

void renderWeapon() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Update quad with bob and recoil offsets
    // Recoil makes weapon bigger (closer to camera)
    const float MAX_BOB = 0.1f;
    float x0 = -1.0f, x1 = 1.0f;
    float y0 = -1.0f - MAX_BOB + bobOffset + recoilOffset;  // Add recoil
    float y1 = 0.5f - MAX_BOB + bobOffset + recoilOffset;   // Add recoil
    float verts[] = {
        x0, y0,   0.0f, 1.0f,
        x1, y0,   1.0f, 1.0f,
        x0, y1,   0.0f, 0.0f,
        x1, y1,   1.0f, 0.0f,
    };

    glUseProgram(prog);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, weaponTex);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
}

void renderHUD() {
    hudVertCount = 0;

    int W = scrW, H = scrH;

    // ---- Crosshair ----
    float cx = W * 0.5f, cy = H * 0.5f;
    float csz = 8.0f, ct = 1.5f;
    pushQuad(cx - csz, cy - ct, cx + csz, cy + ct, 1, 1, 1, 0.8f);  // horizontal
    pushQuad(cx - ct, cy - csz, cx + ct, cy + csz, 1, 1, 1, 0.8f);  // vertical

    // ---- Health Bar ----
    float barX = 20.0f, barY = (float)H - 30.0f;
    float barW = 200.0f, barH = 16.0f;
    float healthFrac = (float)player::player.health / (float)player::player.maxHealth;

    pushQuad(barX, barY, barX + barW, barY + barH, 0.1f, 0.1f, 0.1f, 0.7f); // background
    float hue = healthFrac;  // green when full, yellow/red when low
    pushQuad(barX + 1, barY + 1,
             barX + 1 + (barW - 2) * healthFrac, barY + barH - 1,
             1.0f - hue, hue, 0.0f, 1.0f);  // health fill

    // ---- Ammo Bar ----
    float abarX = (float)W - 220.0f, abarY = (float)H - 30.0f;
    float ammoFrac = (float)player::player.ammo / (float)player::player.maxAmmo;

    pushQuad(abarX, abarY, abarX + barW, abarY + barH, 0.1f, 0.1f, 0.1f, 0.7f);
    pushQuad(abarX + 1, abarY + 1,
             abarX + 1 + (barW - 2) * ammoFrac, abarY + barH - 1,
             0.9f, 0.8f, 0.2f, 1.0f);  // yellow ammo fill

    // ---- Border lines on bars ----
    pushQuad(barX,  barY,  barX + barW, barY + 1,      1,1,1,0.4f);  // top
    pushQuad(barX,  barY + barH - 1, barX + barW, barY + barH, 1,1,1,0.4f); // bottom

    // ---- Upload and draw ----
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(hudProg);
    glBindVertexArray(hudVao);
    glBindBuffer(GL_ARRAY_BUFFER, hudVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, hudVertCount * 6 * sizeof(float), hudVerts);
    glDrawArrays(GL_TRIANGLES, 0, hudVertCount);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
}

void renderGameOver() {
    // Reset vertex count
    hudVertCount = 0;
    
    int W = scrW, H = scrH;
    float centerX = W * 0.5f;
    float centerY = H * 0.4f;
    
    // Dark overlay - add as first quad
    pushQuad(0, 0, (float)W, (float)H, 0.0f, 0.0f, 0.0f, 0.7f);
    
    // "GAME OVER" text using simple blocks
    // G
    pushQuad(centerX - 120, centerY - 20, centerX - 100, centerY + 20, 1, 0, 0, 1);
    pushQuad(centerX - 120, centerY + 10, centerX - 100, centerY + 20, 1, 0, 0, 1);
    pushQuad(centerX - 120, centerY - 20, centerX - 110, centerY - 10, 1, 0, 0, 1);
    // A
    pushQuad(centerX - 90, centerY - 20, centerX - 70, centerY + 20, 1, 0, 0, 1);
    pushQuad(centerX - 90, centerY - 10, centerX - 70, centerY, 1, 0, 0, 1);
    // M
    pushQuad(centerX - 60, centerY - 20, centerX - 40, centerY + 20, 1, 0, 0, 1);
    pushQuad(centerX - 30, centerY - 20, centerX - 10, centerY + 20, 1, 0, 0, 1);
    // E
    pushQuad(centerX, centerY - 20, centerX + 20, centerY + 20, 1, 0, 0, 1);
    pushQuad(centerX, centerY, centerX + 15, centerY + 10, 1, 0, 0, 1);
    // Space
    // O
    pushQuad(centerX + 40, centerY - 20, centerX + 60, centerY + 20, 1, 0, 0, 1);
    pushQuad(centerX + 45, centerY - 15, centerX + 55, centerY + 15, 0, 0, 0, 1);
    // V
    pushQuad(centerX + 70, centerY - 20, centerX + 90, centerY + 20, 1, 0, 0, 1);
    // E
    pushQuad(centerX + 100, centerY - 20, centerX + 120, centerY + 20, 1, 0, 0, 1);
    pushQuad(centerX + 100, centerY, centerX + 115, centerY + 10, 1, 0, 0, 1);
    pushQuad(centerX + 100, centerY - 10, centerX + 115, centerY, 1, 0, 0, 1);
    pushQuad(centerX + 100, centerY - 20, centerX + 115, centerY - 10, 1, 0, 0, 1);
    pushQuad(centerX + 100, centerY + 10, centerX + 115, centerY + 20, 1, 0, 0, 1);
    pushQuad(centerX + 100, centerY, centerX + 115, centerY + 10, 1, 0, 0, 1);
    pushQuad(centerX + 100, centerY - 10, centerX + 115, centerY, 1, 0, 0, 1);
    pushQuad(centerX + 100, centerY - 20, centerX + 115, centerY - 10, 1, 0, 0, 1);
    pushQuad(centerX + 100, centerY + 10, centerX + 115, centerY + 20, 1, 0, 0, 1);
    
    // "Press ENTER to restart" text
    float textY = H * 0.6f;
    pushQuad(centerX - 100, textY - 10, centerX + 100, textY + 10, 1, 1, 1, 0.8f);
    
    // Upload and draw all quads at once
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glUseProgram(hudProg);
    glBindVertexArray(hudVao);
    glBindBuffer(GL_ARRAY_BUFFER, hudVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, hudVertCount * 6 * sizeof(float), hudVerts);
    glDrawArrays(GL_TRIANGLES, 0, hudVertCount);
    glBindVertexArray(0);
    
    glDisable(GL_BLEND);
}

void cleanupHUD() {
    glDeleteTextures(1, &weaponTex);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);

    // Cleanup HUD resources
    glDeleteVertexArrays(1, &hudVao);
    glDeleteBuffers(1, &hudVbo);
    glDeleteProgram(hudProg);
}

} // namespace hud
