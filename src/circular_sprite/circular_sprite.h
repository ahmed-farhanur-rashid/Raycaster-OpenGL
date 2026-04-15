#ifndef CIRCULAR_SPRITE_H
#define CIRCULAR_SPRITE_H

#include "../renderer/map_renderer.h"

namespace circular_sprite {
    void init(int screenW, int screenH);
    void update(float dt);
    void render(const renderer::RenderState& state);
    void cleanup();
}

#endif
