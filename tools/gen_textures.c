/*  tools/gen_textures.c
 *  Standalone texture generator for the forest-themed raycaster.
 *  Compile:  g++ -Iinclude tools/gen_textures.c -lm -o build/gen_textures.exe
 *  Run from project root:  build/gen_textures.exe
 *  Outputs PNGs to resource/textures/
 */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../include/stb/stb_image_write.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define TEX_SZ 128
#define SKY_W  512
#define SKY_H  128

/* ---- helpers ---- */
static inline uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
}
static inline uint8_t clampf(float v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

/* ---- value noise ---- */
static float hashf(int x, int y) {
    int n = x + y * 57;
    n = (n << 13) ^ n;
    return (float)((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 2147483648.0f;
}
static float smoothNoise(float x, float y) {
    int ix = (int)floorf(x), iy = (int)floorf(y);
    float fx = x - ix, fy = y - iy;
    float sx = fx * fx * (3 - 2 * fx);
    float sy = fy * fy * (3 - 2 * fy);
    float i0 = hashf(ix, iy) + sx * (hashf(ix + 1, iy) - hashf(ix, iy));
    float i1 = hashf(ix, iy + 1) + sx * (hashf(ix + 1, iy + 1) - hashf(ix, iy + 1));
    return i0 + sy * (i1 - i0);
}
static float fbm(float x, float y, int oct) {
    float v = 0, amp = 0.5f, freq = 1;
    for (int i = 0; i < oct; i++) {
        v += amp * smoothNoise(x * freq, y * freq);
        amp *= 0.5f; freq *= 2;
    }
    return v;
}
/* turbulence (absolute-value noise for cracks/veins) */
static float turb(float x, float y, int oct) {
    float v = 0, amp = 0.5f, freq = 1;
    for (int i = 0; i < oct; i++) {
        v += amp * fabsf(2.0f * smoothNoise(x * freq, y * freq) - 1.0f);
        amp *= 0.5f; freq *= 2;
    }
    return v;
}

/* ================================================================
 *  WALL TEXTURES  (128 x 128)
 * ================================================================ */

/* Wall 0 – tree: distinct brown trunk in center, leaf canopy around it */
static void genWall0(uint32_t* buf) {
    int S = TEX_SZ;
    for (int y = 0; y < S; y++)
    for (int x = 0; x < S; x++) {
        float fx = (float)x / S, fy = (float)y / S;
        float cx = fx - 0.5f;
        /* trunk shape: wavy center column, wider at base, narrow at top */
        float trunkW = 0.15f + 0.06f * fy; /* wider at bottom (fy=1) */
        float wobble = fbm(fx * 2 + 55, fy * 6 + 66, 3) * 0.03f;
        int isTrunk = (fabsf(cx + wobble) < trunkW) && (fy > 0.18f);

        if (isTrunk) {
            /* bark with deep vertical fissures */
            float grain = fbm(fx * 3 + 10, fy * 1.5f + 5, 5);
            float fissure = turb(fx * 8 + 20, fy * 2 + 15, 4);
            float v = grain * 0.6f + (1 - fissure) * 0.4f;
            float bR = 45 + v * 65;
            float bG = 25 + v * 40;
            float bB = 10 + v * 20;
            /* deep fissure darkening */
            if (fissure > 0.55f) {
                float d = (fissure - 0.55f) * 3.0f;
                if (d > 1) d = 1;
                bR *= (1 - d * 0.7f); bG *= (1 - d * 0.7f); bB *= (1 - d * 0.7f);
            }
            /* cylindrical shading */
            float edgeDist = fabsf(cx + wobble) / trunkW;
            float shade = 1 - edgeDist * edgeDist * 0.6f;
            bR *= shade; bG *= shade; bB *= shade;
            /* scattered lichen patches */
            float lich = fbm(fx * 6 + 200, fy * 4 + 210, 3);
            if (lich > 0.62f) {
                float s = (lich - 0.62f) * 4.0f; if (s > 1) s = 1;
                bR = bR * (1 - s * 0.5f) + 40 * s;
                bG = bG * (1 - s * 0.3f) + 55 * s;
                bB = bB * (1 - s * 0.4f);
            }
            buf[y * S + x] = rgba(clampf(bR), clampf(bG), clampf(bB), 255);
        } else {
            /* leaf canopy with clustered shapes */
            float l1 = fbm(fx * 5 + 77, fy * 5 + 88, 5);
            float l2 = fbm(fx * 9 + 99, fy * 9 + 111, 4);
            float l3 = fbm(fx * 16 + 120, fy * 16 + 130, 3);
            float leaf = l1 * 0.4f + l2 * 0.35f + l3 * 0.25f;
            /* dark gaps between clusters ("sky" showing through) */
            float gap = fbm(fx * 4 + 155, fy * 4 + 166, 4);
            if (gap < 0.25f) {
                float d = (0.25f - gap) / 0.25f;
                uint8_t v = clampf(5 + (1 - d) * 12);
                buf[y * S + x] = rgba(v, clampf(v + 4), v, 255);
            } else {
                float lR = 15 + leaf * 50;
                float lG = 50 + leaf * 135;
                float lB = 8 + leaf * 25;
                /* highlight patches (top-lit leaves) */
                float hl = fbm(fx * 7 + 133, fy * 7 + 144, 3);
                if (hl > 0.55f) { float s = (hl - 0.55f) * 3.5f; lR += s * 40; lG += s * 50; lB += s * 12; }
                /* scattered yellow/autumn leaves */
                float yel = hashf(x * 3 + 200, y * 5 + 210);
                if (yel > 0.92f) { lR = 130 + l3 * 80; lG = 110 + l3 * 40; lB = 15; }
                /* darker undersides */
                float under = fbm(fx * 6 + 250, fy * 3 + 260, 2);
                if (under < 0.3f) { float d = (0.3f - under) / 0.3f; lR *= (1 - d * 0.4f); lG *= (1 - d * 0.3f); lB *= (1 - d * 0.4f); }
                buf[y * S + x] = rgba(clampf(lR), clampf(lG), clampf(lB), 255);
            }
        }
    }
}

/* Wall 1 – mossy stone with individual blocks and mortar lines */
static void genWall1(uint32_t* buf) {
    int S = TEX_SZ;
    for (int y = 0; y < S; y++)
    for (int x = 0; x < S; x++) {
        float fx = (float)x / S, fy = (float)y / S;
        /* stone block grid with offset rows */
        int row = y / (S / 4);
        int blockW = S / 3;
        int offX = (row % 2) * (blockW / 2);
        int bx = (x + offX) % blockW;
        int by = y % (S / 4);
        int mortarW = 2;
        int isMortar = (bx < mortarW) || (by < mortarW);

        if (isMortar) {
            float mn = fbm(fx * 12 + 100, fy * 12 + 110, 3);
            buf[y * S + x] = rgba(clampf(40 + mn * 25), clampf(38 + mn * 22), clampf(35 + mn * 20), 255);
        } else {
            int blockId = row * 5 + ((x + offX) / blockW);
            float blockHue = hashf(blockId * 17 + 300, blockId * 31 + 400);
            float n = fbm(fx * 6 + blockHue * 50, fy * 6 + 200, 5);
            float t = turb(fx * 10 + 150, fy * 10 + 160, 4);
            float bR = 65 + blockHue * 30 + n * 50;
            float bG = 65 + blockHue * 25 + n * 45;
            float bB = 70 + blockHue * 20 + n * 42;
            if (t > 0.5f) { float d = (t - 0.5f) * 2.0f; bR *= (1 - d * 0.3f); bG *= (1 - d * 0.3f); bB *= (1 - d * 0.3f); }
            float crack = turb(fx * 15 + 70, fy * 15 + 80, 3);
            if (crack > 0.75f) { float d = (crack - 0.75f) * 5.0f; if (d > 1) d = 1; bR *= (1 - d * 0.6f); bG *= (1 - d * 0.6f); bB *= (1 - d * 0.6f); }
            float mossZone = (float)by / (S / 4);
            float moss = fbm(fx * 4 + 50, fy * 3 + 30, 5);
            if (moss > 0.4f && mossZone < 0.6f) {
                float s = (moss - 0.4f) * 2.5f; if (s > 1) s = 1;
                s *= (1 - mossZone);
                bR = bR * (1 - s) + (25 + n * 35) * s;
                bG = bG * (1 - s) + (55 + n * 70) * s;
                bB = bB * (1 - s) + (15 + n * 15) * s;
            }
            float stain = fbm(fx * 2 + 500, fy * 8 + 510, 3);
            if (stain > 0.6f) { float d = (stain - 0.6f) * 2.0f; bR *= (1 - d * 0.15f); bG *= (1 - d * 0.1f); bB *= (1 - d * 0.05f); }
            buf[y * S + x] = rgba(clampf(bR), clampf(bG), clampf(bB), 255);
        }
    }
}

/* Wall 2 – dense foliage/hedge with leaf clusters, berries, depth */
static void genWall2(uint32_t* buf) {
    int S = TEX_SZ;
    for (int y = 0; y < S; y++)
    for (int x = 0; x < S; x++) {
        float fx = (float)x / S, fy = (float)y / S;
        float l1 = fbm(fx * 4 + 300, fy * 4 + 400, 5);
        float l2 = fbm(fx * 8 + 500, fy * 8 + 600, 4);
        float l3 = fbm(fx * 15 + 520, fy * 15 + 620, 3);
        float leaf = l1 * 0.35f + l2 * 0.35f + l3 * 0.3f;
        float branchV = turb(fx * 2 + 720, fy * 6 + 730, 3);
        int isBranch = (branchV > 0.6f) && (leaf < 0.4f);
        if (isBranch) {
            float bn = fbm(fx * 8 + 740, fy * 3 + 750, 3);
            buf[y * S + x] = rgba(clampf(50 + bn * 35), clampf(30 + bn * 25), clampf(12 + bn * 12), 255);
        } else {
            float gap = fbm(fx * 5 + 760, fy * 5 + 770, 4);
            if (gap < 0.2f) {
                float d = (0.2f - gap) / 0.2f;
                buf[y * S + x] = rgba(clampf(8 + (1 - d) * 18), clampf(12 + (1 - d) * 20), clampf(5 + (1 - d) * 10), 255);
            } else {
                float lR = 18 + leaf * 45;
                float lG = 45 + leaf * 120;
                float lB = 10 + leaf * 28;
                float sun = fbm(fx * 7 + 780, fy * 6 + 790, 2);
                if (sun > 0.6f) { float s = (sun - 0.6f) * 3.5f; lR += s * 35; lG += s * 45; lB += s * 10; }
                float age = fbm(fx * 6 + 800, fy * 6 + 810, 3);
                if (age < 0.3f) { float d = (0.3f - age) / 0.3f; lG *= (1 - d * 0.35f); lR += d * 15; }
                float berry = hashf(x * 7 + 820, y * 11 + 830);
                if (berry > 0.985f && leaf > 0.35f) { lR = 180; lG = 25; lB = 20; }
                buf[y * S + x] = rgba(clampf(lR), clampf(lG), clampf(lB), 255);
            }
        }
    }
}

/* Wall 3 – old wooden planks with knots, nails, weathering */
static void genWall3(uint32_t* buf) {
    int S = TEX_SZ;
    int plankW = S / 4;
    for (int y = 0; y < S; y++)
    for (int x = 0; x < S; x++) {
        float fx = (float)x / S, fy = (float)y / S;
        int plankIdx = x / plankW;
        int px = x % plankW;
        float plankHue = hashf(plankIdx * 37 + 1100, 42);

        if (px < 1 || px >= plankW - 1) {
            buf[y * S + x] = rgba(15, 10, 5, 255);
            continue;
        }

        float curvature = sinf((float)px / plankW * 3.14159f) * 0.03f;
        float grain = fbm(fx * 2 + plankIdx * 5, fy * 15 + curvature * fy * 8, 5);
        float fine = fbm(fx * 6 + 1200, fy * 30 + 1210, 3);
        float v = grain * 0.65f + fine * 0.35f;

        float bR = 85 + plankHue * 40 + v * 60;
        float bG = 55 + plankHue * 30 + v * 45;
        float bB = 25 + plankHue * 12 + v * 22;

        float edgeDist = fabsf((float)px - plankW / 2.0f) / (plankW / 2.0f);
        float edgeShade = 1 - edgeDist * edgeDist * 0.3f;
        bR *= edgeShade; bG *= edgeShade; bB *= edgeShade;

        for (int k = 0; k < 2; k++) {
            float kx = (plankIdx + 0.5f) * plankW + hashf(plankIdx + k * 10, 50) * plankW * 0.4f - plankW * 0.2f;
            float ky = (k + 0.3f) * S / 2.0f + hashf(plankIdx + k * 20, 60) * S * 0.15f;
            float dx = x - kx, dy = y - ky;
            float knotDist = sqrtf(dx * dx + dy * dy * 0.4f);
            float knotR = 4 + hashf(plankIdx + k, 70) * 5;
            if (knotDist < knotR * 2) {
                float kt = knotDist / (knotR * 2);
                float ring = sinf(knotDist * 1.2f) * 0.3f;
                float dark = (1 - kt) * 0.5f;
                bR = bR * (1 - dark) + (35 + ring * 20) * dark;
                bG = bG * (1 - dark) + (22 + ring * 15) * dark;
                bB = bB * (1 - dark) + (10 + ring * 8) * dark;
            }
        }

        if ((y % (S / 4) >= S / 8 - 1) && (y % (S / 4) <= S / 8 + 1) && (px >= plankW / 2 - 1) && (px <= plankW / 2 + 1)) {
            bR = 50; bG = 48; bB = 45;
        }

        float weather = fbm(fx * 3 + 1300, fy * 5 + 1400, 4);
        if (weather > 0.55f) { float w = (weather - 0.55f) * 3.0f; if (w > 1) w = 1; bR *= (1 - w * 0.35f); bG *= (1 - w * 0.3f); bB *= (1 - w * 0.25f); }

        buf[y * S + x] = rgba(clampf(bR), clampf(bG), clampf(bB), 255);
    }
}

/* ================================================================
 *  FLOOR TEXTURE  (128 x 128) – soil with scattered fallen leaves
 * ================================================================ */
static void genFloor(uint32_t* buf) {
    int S = TEX_SZ;
    for (int y = 0; y < S; y++)
    for (int x = 0; x < S; x++) {
        float fx = (float)x / S, fy = (float)y / S;
        /* multi-octave soil base */
        float n1 = fbm(fx * 6 + 1500, fy * 6 + 1600, 5);
        float n2 = fbm(fx * 12 + 1510, fy * 12 + 1610, 4);
        float n3 = fbm(fx * 24 + 1520, fy * 24 + 1620, 3);
        float soil = n1 * 0.45f + n2 * 0.35f + n3 * 0.2f;

        /* leaf coverage noise (multiple scales for varied shapes) */
        float lf1 = fbm(fx * 5 + 2300, fy * 5 + 2350, 4);
        float lf2 = fbm(fx * 10 + 2500, fy * 10 + 2550, 3);
        float lf3 = fbm(fx * 18 + 2600, fy * 18 + 2650, 3);
        float leafCover = lf1 * 0.4f + lf2 * 0.35f + lf3 * 0.25f;

        if (leafCover > 0.4f) {
            /* fallen leaf pixel */
            int leafId = (int)(fbm(fx * 3 + 3300, fy * 3 + 3400, 2) * 6);
            float detail = fbm(fx * 20 + 2800, fy * 20 + 2850, 3);
            float lR, lG, lB;
            switch (leafId) {
                case 0: /* warm orange */
                    lR = 170 + detail * 55; lG = 85 + detail * 35; lB = 12 + detail * 15; break;
                case 1: /* deep crimson red */
                    lR = 150 + detail * 45; lG = 30 + detail * 20; lB = 10 + detail * 12; break;
                case 2: /* golden yellow */
                    lR = 175 + detail * 50; lG = 145 + detail * 45; lB = 18 + detail * 15; break;
                case 3: /* faded olive green */
                    lR = 55 + detail * 35; lG = 75 + detail * 50; lB = 22 + detail * 15; break;
                case 4: /* russet brown */
                    lR = 100 + detail * 40; lG = 55 + detail * 25; lB = 15 + detail * 12; break;
                default: /* dark dried brown */
                    lR = 60 + detail * 30; lG = 38 + detail * 18; lB = 12 + detail * 10; break;
            }
            /* leaf vein structure */
            float vein = turb(fx * 30 + 2900, fy * 30 + 3000, 3);
            if (vein > 0.6f) {
                float v = (vein - 0.6f) * 2.5f;
                lR *= (1 - v * 0.25f); lG *= (1 - v * 0.2f); lB *= (1 - v * 0.25f);
            }
            /* leaf edge darkening where coverage thins */
            if (leafCover < 0.48f) {
                float e = (0.48f - leafCover) / 0.08f;
                lR *= (1 - e * 0.35f); lG *= (1 - e * 0.35f); lB *= (1 - e * 0.35f);
            }
            /* overlapping leaves create slight shadow */
            float overlap = fbm(fx * 8 + 3100, fy * 8 + 3200, 2);
            if (overlap > 0.65f) { lR *= 0.8f; lG *= 0.8f; lB *= 0.8f; }
            /* curled/dry edges */
            float curl = fbm(fx * 22 + 3300, fy * 22 + 3400, 2);
            if (curl > 0.72f && leafCover < 0.55f) { lR = lR * 0.85f + 20; lG *= 0.75f; lB *= 0.7f; }

            buf[y * S + x] = rgba(clampf(lR), clampf(lG), clampf(lB), 255);
        } else {
            /* exposed soil */
            float bR = 48 + soil * 42;
            float bG = 30 + soil * 28;
            float bB = 12 + soil * 16;
            /* pebbles / gravel */
            float peb = fbm(fx * 16 + 1700, fy * 16 + 1800, 3);
            if (peb > 0.68f) { float s = (peb - 0.68f) * 4.0f; bR += s * 30; bG += s * 25; bB += s * 22; }
            /* root / twig debris */
            float tw = turb(fx * 12 + 2100, fy * 4 + 2200, 3);
            if (tw > 0.65f) { float s = (tw - 0.65f) * 3.5f; if (s > 1) s = 1; bR = bR * (1 - s * 0.5f) + 35 * s; bG = bG * (1 - s * 0.5f) + 22 * s; bB = bB * (1 - s * 0.5f) + 8 * s; }
            /* damp darker patches */
            float damp = fbm(fx * 3 + 1900, fy * 3 + 2000, 4);
            if (damp < 0.3f) { float d = (0.3f - damp) * 2.5f; bR *= (1 - d * 0.3f); bG *= (1 - d * 0.25f); bB *= (1 - d * 0.2f); }

            buf[y * S + x] = rgba(clampf(bR), clampf(bG), clampf(bB), 255);
        }
    }
}

/* ================================================================
 *  SKY TEXTURE  (512 x 128)  –  dark cloudy night
 * ================================================================ */
static void genSky(uint32_t* buf) {
    /* Use cylindrical coordinates so the texture wraps seamlessly horizontally.
       Map x to a circle: cx = cos(2*PI*x/W), cy = sin(2*PI*x/W)
       Then sample noise using (cx,cy) so x=0 and x=W-1 produce the same value. */
    float PI2 = 6.2831853f;
    for (int y = 0; y < SKY_H; y++)
    for (int x = 0; x < SKY_W; x++) {
        float angle = PI2 * (float)x / (float)SKY_W;
        float cx = cosf(angle) * 3.0f;  /* scale controls cloud size */
        float cy = sinf(angle) * 3.0f;
        float fy = (float)y * 0.04f;

        float hg = (float)y / SKY_H;
        float bR = 8  + hg * 15;
        float bG = 10 + hg * 18;
        float bB = 20 + hg * 25;
        float c1 = fbm(cx + 2300, cy + fy + 2400, 5);
        float c2 = fbm(cx * 0.8f + 2500, cy * 0.8f + fy * 0.75f + 2600, 4);
        float cloud = c1 * 0.6f + c2 * 0.4f;
        if (cloud > 0.4f) {
            float s = (cloud - 0.4f) * 2.5f; if (s > 1) s = 1;
            bR += s * (25 + hg * 10);
            bG += s * (22 + hg * 8);
            bB += s * (30 + hg * 12);
        }
        float un = fbm(cx * 1.2f + 2700, cy * 1.2f + fy * 1.25f + 2800, 3);
        if (un < 0.3f && cloud > 0.4f) {
            float d = (0.3f - un) * 2.0f;
            bR *= (1 - d * 0.3f); bG *= (1 - d * 0.3f); bB *= (1 - d * 0.25f);
        }
        if (cloud < 0.35f && y < SKY_H * 0.6f) {
            float st = hashf(x * 3 + 5000, y * 7 + 6000);
            if (st > 0.997f) {
                float br = 40 + st * 60;
                bR += br * 0.8f; bG += br * 0.8f; bB += br;
            }
        }
        buf[y * SKY_W + x] = rgba(clampf(bR), clampf(bG), clampf(bB), 255);
    }
}

/* ================================================================
 *  SPRITE TEXTURES  (128 x 128)
 * ================================================================ */

/* Sprite 0 – bush */
static void genSprite0(uint32_t* buf) {
    int S = TEX_SZ;
    for (int y = 0; y < S; y++)
    for (int x = 0; x < S; x++) {
        float fx = (float)x / S, fy = (float)y / S;
        float cx = fx - 0.5f, cy = fy - 0.5f;
        float dist = sqrtf(cx * cx + cy * cy);
        float maxR = 0.3f + fy * 0.15f;
        if (fy < 0.25f) maxR = 0.08f + fy * 0.6f;
        if (dist < maxR && fy > 0.08f) {
            float n = fbm(fx * 8 + 3000, fy * 8 + 3100, 4);
            float edge = 1 - (dist / maxR);
            edge = edge * edge;
            buf[y * S + x] = rgba(clampf((25 + n * 40) * (0.5f + edge * 0.5f)),
                                   clampf((50 + n * 90) * (0.5f + edge * 0.5f)),
                                   clampf((15 + n * 25) * (0.5f + edge * 0.5f)), 255);
        } else
            buf[y * S + x] = rgba(0, 0, 0, 0);
    }
}

/* Sprite 1 – lantern */
static void genSprite1(uint32_t* buf) {
    int S = TEX_SZ;
    for (int y = 0; y < S; y++)
    for (int x = 0; x < S; x++) {
        float fx = (float)x / S, fy = (float)y / S;
        float cx = fx - 0.5f, cy = fy - 0.375f;
        float postW = 0.025f;
        if (fy > 0.5f && fabsf(cx) < postW)
            buf[y * S + x] = rgba(80, 50, 30, 255);
        else if (cx * cx + cy * cy < 0.015f && fy < 0.55f && fy > 0.12f) {
            float d = sqrtf(cx * cx + cy * cy) / 0.12f;
            buf[y * S + x] = rgba(clampf(255 * (1 - d * 0.3f)), clampf(180 * (1 - d * 0.5f)), clampf(50 * (1 - d * 0.8f)), 255);
        } else
            buf[y * S + x] = rgba(0, 0, 0, 0);
    }
}

/* Sprite 2 – tree stump */
static void genSprite2(uint32_t* buf) {
    int S = TEX_SZ;
    for (int y = 0; y < S; y++)
    for (int x = 0; x < S; x++) {
        float fx = (float)x / S, fy = (float)y / S;
        float cx = fx - 0.5f;
        float halfW = 0.19f + fbm(fx * 4 + 4000, fy * 2 + 4100, 2) * 0.04f;
        if (fabsf(cx) < halfW && fy > 0.33f) {
            float n = fbm(fx * 6 + 4200, fy * 2 + 4300, 4);
            uint8_t r = clampf(70 + n * 50), g = clampf(45 + n * 35), b = clampf(20 + n * 20);
            if (fy < 0.38f) {
                float ring = sinf(sqrtf(cx * cx + (fy - 0.33f) * (fy - 0.33f) * 16) * 25);
                r = clampf(90 + ring * 30); g = clampf(65 + ring * 20); b = clampf(35 + ring * 15);
            }
            buf[y * S + x] = rgba(r, g, b, 255);
        } else
            buf[y * S + x] = rgba(0, 0, 0, 0);
    }
}

/* ================================================================ */
int main(void) {
    static uint32_t buf[TEX_SZ * TEX_SZ];
    static uint32_t skyBuf[SKY_W * SKY_H];

    printf("Generating forest textures (%dx%d)...\n", TEX_SZ, TEX_SZ);

    genWall0(buf); stbi_write_png("resource/textures/wall_0.png", TEX_SZ, TEX_SZ, 4, buf, TEX_SZ * 4);
    printf("  wall_0.png  (tree with leaves)\n");
    genWall1(buf); stbi_write_png("resource/textures/wall_1.png", TEX_SZ, TEX_SZ, 4, buf, TEX_SZ * 4);
    printf("  wall_1.png  (mossy stone blocks)\n");
    genWall2(buf); stbi_write_png("resource/textures/wall_2.png", TEX_SZ, TEX_SZ, 4, buf, TEX_SZ * 4);
    printf("  wall_2.png  (dense foliage hedge)\n");
    genWall3(buf); stbi_write_png("resource/textures/wall_3.png", TEX_SZ, TEX_SZ, 4, buf, TEX_SZ * 4);
    printf("  wall_3.png  (old wooden planks)\n");

    genFloor(buf); stbi_write_png("resource/textures/floor_0.png", TEX_SZ, TEX_SZ, 4, buf, TEX_SZ * 4);
    printf("  floor_0.png (soil with fallen leaves)\n");

    genSky(skyBuf); stbi_write_png("resource/textures/sky_0.png", SKY_W, SKY_H, 4, skyBuf, SKY_W * 4);
    printf("  sky_0.png   (dark cloudy night, %dx%d)\n", SKY_W, SKY_H);

    genSprite0(buf); stbi_write_png("resource/textures/sprite_0.png", TEX_SZ, TEX_SZ, 4, buf, TEX_SZ * 4);
    printf("  sprite_0.png (bush)\n");
    genSprite1(buf); stbi_write_png("resource/textures/sprite_1.png", TEX_SZ, TEX_SZ, 4, buf, TEX_SZ * 4);
    printf("  sprite_1.png (lantern)\n");
    genSprite2(buf); stbi_write_png("resource/textures/sprite_2.png", TEX_SZ, TEX_SZ, 4, buf, TEX_SZ * 4);
    printf("  sprite_2.png (tree stump)\n");

    printf("\nDone! All textures saved to resource/textures/\n");
    printf("You can replace any PNG with your own artwork.\n");
    return 0;
}
