#ifndef SPRITE_REGISTRY_H
#define SPRITE_REGISTRY_H

#include <string>
#include <map>

namespace sprite {
    // Initialize the sprite registry with file paths
    void initSpriteRegistry();
    
    // Get sprite texture index by name
    int getSpriteIndex(const std::string& name);
    
    // Load all sprites into OpenGL texture array
    unsigned int loadSpriteTextures();
    
    // Cleanup
    void cleanupSprites();
}

#endif
