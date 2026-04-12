#include "sprite_registry.h"
#include <glad/glad.h>
#include <stb/stb_image.h>
#include <cstring>
#include <cstdio>

namespace sprite {

static std::map<std::string, std::string> spritePaths;
static std::map<std::string, int> spriteIndices;
static unsigned int spriteTexArray = 0;
static const int TEX_SZ = 64;

void initSpriteRegistry() {
    spritePaths.clear();
    spriteIndices.clear();
    
    // Map sprite names to file paths (without extension)
    spritePaths["barrel"] = "resource/textures/sprite_barrel";
    spritePaths["lamp"] = "resource/textures/sprite_lamp";
    spritePaths["column"] = "resource/textures/sprite_column";
    spritePaths["torch"] = "resource/textures/sprite_torch";
    spritePaths["enemy"] = "resource/textures/sprite_enemy";
    spritePaths["enemy_wounded"] = "resource/textures/sprite_enemy_wounded";
    spritePaths["enemy_dead"] = "resource/textures/sprite_enemy_dead";
    spritePaths["enemy_agro"] = "resource/textures/sprite_enemy_agro";
    spritePaths["health"] = "resource/textures/sprite_health";
    spritePaths["ammo"] = "resource/textures/sprite_ammo";
    spritePaths["key"] = "resource/textures/sprite_key";
    spritePaths["bullet"] = "resource/textures/sprite_bullet";
}

int getSpriteIndex(const std::string& name) {
    auto it = spriteIndices.find(name);
    if (it != spriteIndices.end()) {
        return it->second;
    }
    fprintf(stderr, "WARNING: Unknown sprite '%s', using fallback index 0\n", name.c_str());
    return 0;
}

unsigned int loadSpriteTextures() {
    glGenTextures(1, &spriteTexArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, spriteTexArray);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8,
                 TEX_SZ, TEX_SZ, (int)spritePaths.size(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Load each sprite by name using path
    int index = 0;
    for (auto& pair : spritePaths) {
        const std::string& name = pair.first;
        const std::string& basePath = pair.second;
        
        // Try PNG first, then other formats
        std::string path = basePath + ".png";
        
        int sw, sh, sc;
        unsigned char* raw = stbi_load(path.c_str(), &sw, &sh, &sc, 4);
        if (raw) {
            unsigned char* buf = new unsigned char[TEX_SZ * TEX_SZ * 4];
            for (int y = 0; y < TEX_SZ; y++)
                for (int x = 0; x < TEX_SZ; x++) {
                    int sx = x * sw / TEX_SZ, sy = y * sh / TEX_SZ;
                    memcpy(&buf[(y*TEX_SZ+x)*4], &raw[(sy*sw+sx)*4], 4);
                }
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, index,
                            TEX_SZ, TEX_SZ, 1, GL_RGBA, GL_UNSIGNED_BYTE, buf);
            stbi_image_free(raw);
            delete[] buf;
            spriteIndices[name] = index;  // Store the assigned index
            printf("Loaded sprite: %s -> %s.png (index %d)\n", name.c_str(), basePath.c_str(), index);
            index++;
        } else {
            fprintf(stderr, "WARNING: could not load sprite %s\n", path.c_str());
            // Upload a fallback colored square
            unsigned char* buf = new unsigned char[TEX_SZ * TEX_SZ * 4];
            memset(buf, (index + 1) * 30, TEX_SZ * TEX_SZ * 4);
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, index,
                            TEX_SZ, TEX_SZ, 1, GL_RGBA, GL_UNSIGNED_BYTE, buf);
            delete[] buf;
            spriteIndices[name] = index;
            index++;
        }
    }

    return spriteTexArray;
}

void cleanupSprites() {
    if (spriteTexArray) {
        glDeleteTextures(1, &spriteTexArray);
        spriteTexArray = 0;
    }
    spritePaths.clear();
    spriteIndices.clear();
}

} // namespace sprite
