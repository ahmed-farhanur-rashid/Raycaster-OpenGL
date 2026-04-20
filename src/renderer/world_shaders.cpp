/*
 * world_shaders.cpp — GLSL shaders for the GPU raycasting renderer
 *
 * Vertex shader: passes through a full-screen quad.
 * Fragment shader: performs DDA raycasting for walls, floor, and sky.
 */

#include "world_shaders.h"

namespace world_shaders {

const char* vertexSource = R"(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
out vec2 uv;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    uv = aUV;
}
)";

const char* fragmentSource = R"(
#version 330 core
in  vec2 uv;
out vec4 fragColor;

uniform vec2  playerPos;
uniform vec2  playerDir;
uniform vec2  playerPlane;
uniform float playerHeight;   // Player's Z position (jumping)
uniform vec2  screenSize;
uniform ivec2 mapSize;
uniform bool  lightingEnabled;

uniform usampler2D    mapTex;
uniform sampler2DArray wallTex;
uniform sampler2D     floorTex;
uniform sampler2D     skyTex;
uniform float         fogDensity;
uniform float         wallSideDarken;

#define PI        3.14159265359
#define MAX_STEPS 256

/* ---- helpers ---- */
int getMap(int x, int y) {
    if (x < 0 || x >= mapSize.x || y < 0 || y >= mapSize.y) return 1;
    return int(texelFetch(mapTex, ivec2(x, y), 0).r);
}

/* DDA ray cast.  Returns wall type (>0 on hit, 0 on miss). */
int castRay(vec2 rd, out int side, out float perpDist) {
    int mapX = int(playerPos.x), mapY = int(playerPos.y);

    float ddX = (rd.x == 0.0) ? 1e30 : abs(1.0 / rd.x);
    float ddY = (rd.y == 0.0) ? 1e30 : abs(1.0 / rd.y);
    float sdX, sdY;
    int   stepX, stepY;

    if (rd.x < 0.0) { stepX = -1; sdX = (playerPos.x - float(mapX)) * ddX; }
    else             { stepX =  1; sdX = (float(mapX) + 1.0 - playerPos.x) * ddX; }
    if (rd.y < 0.0) { stepY = -1; sdY = (playerPos.y - float(mapY)) * ddY; }
    else             { stepY =  1; sdY = (float(mapY) + 1.0 - playerPos.y) * ddY; }

    side = 0;
    for (int i = 0; i < MAX_STEPS; i++) {
        if (sdX < sdY) { sdX += ddX; mapX += stepX; side = 0; }
        else            { sdY += ddY; mapY += stepY; side = 1; }
        int cell = getMap(mapX, mapY);
        if (cell > 0) {
            if (side == 0) perpDist = (float(mapX) - playerPos.x + float(1 - stepX) * 0.5) / rd.x;
            else           perpDist = (float(mapY) - playerPos.y + float(1 - stepY) * 0.5) / rd.y;
            if (perpDist < 0.001) perpDist = 0.001;
            return cell;
        }
    }
    perpDist = 1e30;
    return 0;
}

/* ---- rendering helpers ---- */
vec3 renderSky(vec2 rd, float py) {
    int halfH = int(screenSize.y) / 2;
    float angle = atan(rd.y, rd.x);             // world-space heading, -PI..PI
    float u = angle / (2.0 * PI) + 0.5;         // wrap to 0..1
    float v = clamp(py / float(halfH), 0.0, 1.0);
    return texture(skyTex, vec2(u, v)).rgb;
}

vec3 renderFloor(vec2 rd, float py) {
    int halfH = int(screenSize.y) / 2;
    int p = int(py) - halfH;
    if (p < 1) p = 1;
    float rowDist = (float(halfH) * (1.0 + playerHeight)) / float(p);
    vec2  f = playerPos + rowDist * rd;
    vec3 color = texture(floorTex, fract(f)).rgb;

    if (lightingEnabled) {
        float shade = 1.0 / (1.0 + rowDist * rowDist * fogDensity);
        color *= shade;
    }
    return color;
}

vec3 renderWall(vec2 rd, int side, float perpDist, int wallType, float py, float vertOffset, int lineH) {
    int halfH = int(screenSize.y) / 2;
    int horizon = halfH;
    int drawStart = -lineH / 2 + horizon + int(vertOffset);
    int iy = int(py);

    float wallX = (side == 0)
                ? playerPos.y + perpDist * rd.y
                : playerPos.x + perpDist * rd.x;
    wallX = fract(wallX);

    float texU = wallX;
    if (side == 0 && rd.x > 0.0) texU = 1.0 - texU;
    if (side == 1 && rd.y < 0.0) texU = 1.0 - texU;

    float texV = clamp(float(iy - drawStart) / float(max(lineH, 1)), 0.0, 1.0);

    int texIdx = wallType - 1;
    if (texIdx < 0 || texIdx >= 9) texIdx = 0;

    vec3 color = texture(wallTex, vec3(texU, texV, float(texIdx))).rgb;

    /* EW-side darkening */
    if (side == 1) color *= wallSideDarken;

    /* distance fog */
    if (lightingEnabled) {
        float shade = 1.0 / (1.0 + perpDist * perpDist * fogDensity);
        color *= shade;
    }
    return color;
}

/* ---- main ---- */
void main() {
    /* pixel coords: (0,0) = top-left */
    float px = uv.x * screenSize.x;
    float py = uv.y * screenSize.y;
    int   iy = int(py);
    int   halfH   = int(screenSize.y) / 2;
    int   horizon = halfH;

    float camX = 2.0 * px / screenSize.x - 1.0;
    vec2  rd   = playerDir + playerPlane * camX;

    int   side;
    float perpDist;
    int   wallType = castRay(rd, side, perpDist);
    bool  hit      = wallType > 0;

    int lineH = (perpDist < 1e20) ? int(screenSize.y / perpDist) : 0;

    float vertOffset = (playerHeight / perpDist) * float(halfH);
    int drawStart = -lineH / 2 + horizon + int(vertOffset);
    int drawEnd   =  lineH / 2 + horizon + int(vertOffset);

    vec3 color;
    if (iy < drawStart || !hit) {
        color = renderSky(rd, py);
    } else if (iy > drawEnd) {
        color = renderFloor(rd, py);
    } else {
        color = renderWall(rd, side, perpDist, wallType, py, vertOffset, lineH);
    }

    fragColor = vec4(color, 1.0);
}
)";

} // namespace world_shaders
