#ifndef PROJECTILE_RENDERER_H
#define PROJECTILE_RENDERER_H

#include "map_renderer.h"

namespace projectile_renderer {
    void initProjectileRenderer(int screenW, int screenH);
    void renderProjectiles(const renderer::RenderState& state);
    void cleanupProjectileRenderer();
}

#endif
