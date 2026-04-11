#include <glad/glad.h>
#include "hud_renderer.h"
#include "shader.h"
#include <stb/stb_image.h>
#include <cstdio>
#include <cmath>

namespace hud {

static unsigned int prog, vao, vbo, weaponTex;
static int scrW, scrH;
static float bobTimer  = 0.0f;
static float bobOffset = 0.0f;  // NDC units

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
}

void updateBob(bool isMoving, float deltaTime) {
    if (isMoving) bobTimer += deltaTime * 8.0f;
    else bobTimer = 0.0f;
    bobOffset = isMoving ? sinf(bobTimer) * 0.04f : 0.0f;
}

void renderWeapon() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Update quad with bob offset
    // Base position is shifted down by max bob amount so weapon bobs UP from resting position
    const float MAX_BOB = 0.04f;
    float x0 = -1.0f, x1 = 1.0f;
    float y0 = -1.0f - MAX_BOB + bobOffset;  // Rest at -1.04, bob up to -1.0
    float y1 = 0.5f - MAX_BOB + bobOffset; // Rest at -0.09, bob up to -0.05
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

void cleanupHUD() {
    glDeleteTextures(1, &weaponTex);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
}

} // namespace hud
