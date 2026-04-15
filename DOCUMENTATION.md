# Raycaster-OpenGL: Comprehensive Technical Documentation

**From Zero Knowledge to Full Mastery**

---

## Table of Contents

1. [Foundations](#1-foundations)
   - 1.1 [What is Raycasting?](#11-what-is-raycasting)
   - 1.2 [The DDA Algorithm](#12-the-dda-algorithm)
   - 1.3 [The Camera Model](#13-the-camera-model)
   - 1.4 [OpenGL and the GPU Pipeline](#14-opengl-and-the-gpu-pipeline)
   - 1.5 [Key Terminology](#15-key-terminology)
2. [System Overview](#2-system-overview)
   - 2.1 [High-Level Architecture](#21-high-level-architecture)
   - 2.2 [Component Breakdown](#22-component-breakdown)
   - 2.3 [Data Flow](#23-data-flow)
   - 2.4 [Dependency Graph](#24-dependency-graph)
3. [Design Decisions](#3-design-decisions)
   - 3.1 [GPU vs CPU Raycasting](#31-gpu-vs-cpu-raycasting)
   - 3.2 [Library Choices](#32-library-choices)
   - 3.3 [Data-Driven Weapon System](#33-data-driven-weapon-system)
   - 3.4 [Namespace-Based Modules](#34-namespace-based-modules)
   - 3.5 [JSON Configuration](#35-json-configuration)
   - 3.6 [Embedded Shaders and Font](#36-embedded-shaders-and-font)
4. [Implementation Walkthrough](#4-implementation-walkthrough)
   - 4.1 [Startup Sequence](#41-startup-sequence)
   - 4.2 [The Game Loop](#42-the-game-loop)
   - 4.3 [GPU Raycasting Shader](#43-gpu-raycasting-shader)
   - 4.4 [Player Movement and Collision](#44-player-movement-and-collision)
   - 4.5 [Weapon System](#45-weapon-system)
   - 4.6 [Projectile System](#46-projectile-system)
   - 4.7 [Billboard Sprite Rendering](#47-billboard-sprite-rendering)
   - 4.8 [Minimap Renderer](#48-minimap-renderer)
   - 4.9 [Menu System and Game States](#49-menu-system-and-game-states)
   - 4.10 [Settings and Keybinding](#410-settings-and-keybinding)
   - 4.11 [Map Format and Loading](#411-map-format-and-loading)
   - 4.12 [The Map Editor Tool](#412-the-map-editor-tool)
   - 4.13 [Midpoint Circle Sprite](#413-midpoint-circle-sprite)
5. [Practical Usage](#5-practical-usage)
   - 5.1 [Building the Project](#51-building-the-project)
   - 5.2 [Running the Game](#52-running-the-game)
   - 5.3 [Using the Map Editor](#53-using-the-map-editor)
   - 5.4 [Customizing via Configuration](#54-customizing-via-configuration)
   - 5.5 [Rebinding Controls](#55-rebinding-controls)
6. [Debugging & Pitfalls](#6-debugging--pitfalls)
   - 6.1 [Common Build Errors](#61-common-build-errors)
   - 6.2 [Runtime Issues](#62-runtime-issues)
   - 6.3 [Shader Bugs](#63-shader-bugs)
   - 6.4 [Edge Cases and Limitations](#64-edge-cases-and-limitations)
7. [Extension & Scaling](#7-extension--scaling)
   - 7.1 [Adding New Wall Textures](#71-adding-new-wall-textures)
   - 7.2 [Adding New Weapons](#72-adding-new-weapons)
   - 7.3 [Adding Enemies](#73-adding-enemies)
   - 7.4 [Multi-Floor Support](#74-multi-floor-support)
   - 7.5 [Audio Integration](#75-audio-integration)
   - 7.6 [Performance Optimization](#76-performance-optimization)
   - 7.7 [Networking / Multiplayer](#77-networking--multiplayer)
8. [Learning Path](#8-learning-path)

---

## 1. Foundations

### 1.1 What is Raycasting?

Raycasting is a rendering technique that simulates a 3D perspective from a 2D map. It was popularized by Wolfenstein 3D (1992) and uses the principle that **for every vertical column of pixels on screen, you cast a single ray from the player's position into the world, find what it hits, and draw a vertical strip of wall at the appropriate height**.

The key insight: the farther away a wall is, the shorter the strip appears on screen. This creates the illusion of depth without actually constructing a 3D scene.

**Why not "real" 3D?** Raycasting is far simpler than full 3D rendering. The world is a 2D grid; walls have uniform height; there are no slopes or overlapping floors. These constraints make the math tractable and the code short, while producing a convincing first-person view.

**Mental model:** Imagine standing in a room and looking through a window made of vertical slats. Through each slat, you see one narrow column of the world. Each slat independently shows how far away the wall behind it is, and scales accordingly. That is exactly what raycasting does, one slat per screen column.

### 1.2 The DDA Algorithm

DDA (Digital Differential Analyzer) is the algorithm that traces each ray through the grid. It is the mathematical core of the engine.

**Problem:** Given a ray starting at position $(posX, posY)$ with direction $(dirX, dirY)$, find the first grid cell that contains a wall.

**Approach:** The ray crosses grid lines at regular intervals. Horizontal grid lines are spaced 1 unit apart; vertical grid lines are also spaced 1 unit apart. The idea is to "step" the ray forward, alternating between the next horizontal and vertical grid line crossing—whichever is closer.

**Key variables:**

| Variable | Meaning |
|----------|---------|
| `mapX`, `mapY` | Current grid cell the ray is in |
| `stepX`, `stepY` | Direction of grid traversal (+1 or -1) |
| `sideDistX`, `sideDistY` | Distance from ray origin to the next X or Y grid line |
| `deltaDistX`, `deltaDistY` | Distance between consecutive X or Y grid line crossings |
| `side` | 0 if the ray hit a vertical (N/S) grid line, 1 if horizontal (E/W) |

The stepping logic (from the fragment shader):

```glsl
for (int i = 0; i < MAX_STEPS; i++) {
    if (sdX < sdY) { sdX += ddX; mapX += stepX; side = 0; }
    else            { sdY += ddY; mapY += stepY; side = 1; }
    if (getMap(mapX, mapY) > 0) { /* hit a wall */ break; }
}
```

At each step, the algorithm asks: "Is the next vertical grid line closer, or the next horizontal grid line?" It advances to whichever is nearer, then checks if it entered a wall cell. This is $O(n)$ in the number of cells traversed, which is very efficient.

**Perpendicular distance:** Once a wall is hit, we compute the *perpendicular* distance from the player to the wall (not the Euclidean distance). This is critical because Euclidean distance causes a "fisheye" distortion—walls curve outward at the edges of the screen. The formula:

$$
\text{perpDist} = \frac{mapX - posX + \frac{1 - stepX}{2}}{dirX} \quad \text{(for side = 0)}
$$

$$
\text{perpDist} = \frac{mapY - posY + \frac{1 - stepY}{2}}{dirY} \quad \text{(for side = 1)}
$$

### 1.3 The Camera Model

The player's view is defined by three vectors:

| Vector | Symbol | Purpose |
|--------|--------|---------|
| Position | $(posX, posY)$ | Where the player stands in the map |
| Direction | $(dirX, dirY)$ | Where the player is looking (unit-ish vector) |
| Camera plane | $(planeX, planeY)$ | Perpendicular to direction; defines the field of view |

For every column $x$ on screen (ranging from 0 to `screenWidth - 1`):

$$
cameraX = \frac{2x}{\text{screenWidth}} - 1 \quad \in [-1, 1]
$$

$$
rayDir = (dirX + planeX \cdot cameraX, \; dirY + planeY \cdot cameraX)
$$

The camera plane length controls the field of view (FOV). The default value of 0.666 with a unit direction vector gives approximately a 66° FOV—a comfortable value for a first-person perspective.

**Rotation** is a 2D rotation matrix applied to both the direction and camera plane simultaneously:

$$
\begin{pmatrix} x' \\ y' \end{pmatrix} = \begin{pmatrix} \cos\theta & -\sin\theta \\ \sin\theta & \cos\theta \end{pmatrix} \begin{pmatrix} x \\ y \end{pmatrix}
$$

This is implemented in `player::rotatePlayer()`.

### 1.4 OpenGL and the GPU Pipeline

This project uses **OpenGL 3.3 Core Profile** via GLFW (windowing) and GLAD (function loading).

The rendering architecture is a **full-screen quad with a fragment shader**. This means:

1. A quad covering the entire screen is uploaded to the GPU (4 vertices).
2. The vertex shader passes these through unchanged.
3. The fragment shader runs **once per pixel** and performs DDA raycasting, texture sampling, floor rendering, and sky rendering entirely on the GPU.

**Why this matters:** Each pixel is computed independently and in parallel on GPU cores. A modern GPU can run thousands of fragments simultaneously, making this approach extremely fast compared to CPU raycasting.

**Key OpenGL concepts used:**

| Concept | Usage |
|---------|-------|
| VAO/VBO | Vertex data for screen quads |
| Shader programs | GLSL vertex + fragment shaders |
| `GL_TEXTURE_2D` | Floor, sky, minimap, weapon sprites |
| `GL_TEXTURE_2D_ARRAY` | Wall textures (multiple layers in one handle) |
| `R8UI` (unsigned integer texture) | Map grid data uploaded to GPU |
| Uniform variables | Per-frame player state sent to the shader |
| Alpha blending | Transparent sprites, menu overlays |

### 1.5 Key Terminology

| Term | Definition |
|------|------------|
| **NDC** | Normalized Device Coordinates. OpenGL's coordinate system: $(-1,-1)$ is bottom-left, $(1,1)$ is top-right |
| **Fragment** | One pixel being processed by the fragment shader |
| **Billboard** | A sprite that always faces the camera (used for projectiles) |
| **Z-buffer** | Per-column depth array that prevents sprites behind walls from being drawn |
| **FOV** | Field of View — how wide the player can see |
| **Delta time** | Time elapsed since last frame; used to make movement frame-rate independent |
| **Edge detection** | Checking for key press transitions (pressed this frame but not last frame) to handle toggle keys |
| **DDA** | Digital Differential Analyzer — the ray-stepping algorithm |

---

## 2. System Overview

### 2.1 High-Level Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│                          main.cpp                                │
│  ┌─────────┐  ┌──────────┐  ┌───────────┐  ┌──────────────────┐ │
│  │ Settings │  │  Window   │  │ Game State│  │   Game Loop      │ │
│  │ (JSON)   │→ │ (GLFW)    │→ │ (FSM)     │→ │ Input→Update→    │ │
│  │          │  │ (GLAD)    │  │           │  │ Render→Swap      │ │
│  └─────────┘  └──────────┘  └───────────┘  └──────────────────┘ │
└──────────────────────────────────────────────────────────────────┘
                              │
        ┌─────────────────────┼──────────────────────┐
        ▼                     ▼                      ▼
 ┌──────────────┐  ┌──────────────────┐   ┌──────────────────┐
 │   PLAYING     │  │   MAIN_MENU      │   │   PAUSED         │
 │               │  │                  │   │                  │
 │ input::       │  │ menu_renderer::  │   │ Render scene +   │
 │ processInput  │  │ renderMainMenu   │   │ menu_renderer::  │
 │               │  │                  │   │ renderPauseMenu  │
 │ player::      │  └──────────────────┘   └──────────────────┘
 │ movePlayer    │
 │ rotatePlayer  │
 │               │
 │ projectile::  │
 │ update        │
 │               │
 │ renderer::    │
 │ renderFrame   │ ←── GPU DDA raycasting shader
 │               │
 │ circular_     │
 │ sprite::      │
 │ render        │
 │               │
 │ projectile_   │
 │ renderer::    │
 │ render        │
 │               │
 │ minimap::     │
 │ renderMinimap │ ←── CPU DDA (separate pass)
 │               │
 │ hud::         │
 │ renderWeapon  │
 └───────────────┘
```

### 2.2 Component Breakdown

| Module | Files | Responsibility |
|--------|-------|----------------|
| **Window** | `src/core/window.{cpp,h}` | GLFW initialization, OpenGL context creation, GLAD loading |
| **Input** | `src/core/input.{cpp,h}` | Keyboard/mouse processing, movement, weapon switching, toggle states |
| **Game State** | `src/core/game_state.{cpp,h}` | Finite state machine (MAIN_MENU → PLAYING ↔ PAUSED), menu navigation |
| **Settings** | `src/settings/settings.{cpp,h}` | JSON config/keybind loading, runtime parameter queries |
| **Map** | `src/map/map.{cpp,h}` | ASCII grid file parsing into 2D integer array |
| **Player** | `src/player/player.{cpp,h}` | Position/direction state, movement with collision, rotation, jumping/gravity |
| **World Renderer** | `src/renderer/map_renderer.{cpp,h}` | GPU shader setup, texture upload, per-frame uniform updates |
| **World Shaders** | `src/renderer/world_shaders.{cpp,h}` | GLSL source code for full-screen DDA raycasting |
| **Shader Utility** | `src/renderer/shader.{cpp,h}` | Compile/link GLSL shader programs with error handling |
| **Minimap** | `src/renderer/minimap_renderer.{cpp,h}` | CPU-rasterized top-down map overlay |
| **HUD** | `src/renderer/hud_renderer.{cpp,h}` | Weapon sprite display, bob animation, ammo bars |
| **Projectile Renderer** | `src/renderer/projectile_renderer.{cpp,h}` | Billboard rendering of projectiles with z-buffer |
| **Menu Renderer** | `src/renderer/menu_renderer.{cpp,h}` | Main menu and pause menu with built-in bitmap font |
| **Weapons** | `src/weapons/weapon.{cpp,h}` + per-weapon files | Weapon struct, state machine, factory functions |
| **Projectile** | `src/projectile/projectile.{cpp,h}` | Projectile spawning, movement, wall collision, lifetime |
| **Circular Sprite** | `src/circular_sprite/circular_sprite.{cpp,h}` | Procedural target entity using midpoint circle algorithm |
| **Map Editor** | `tools/map_editor.cpp` | Standalone visual map creation tool |

### 2.3 Data Flow

**Per-frame data flow during PLAYING state:**

```
1. GLFW polls input events
       │
2. game::updateMenuInput()  ─── checks ESC for pause transition
       │
3. input::processInput()
       │
       ├─── player::updatePhysics(dt)  ─── gravity, vertical position
       ├─── player::jump()             ─── on SPACE press
       ├─── Mouse delta → player::rotatePlayer(angle)
       ├─── WASD → player::movePlayer(fwd, strafe, dt, sprint)
       ├─── Toggle keys → minimapEnabled, lightingEnabled
       ├─── Number keys → hud::equipWeapon()
       ├─── hud::updateHUD() → weapon state machine tick
       └─── hud::firedThisFrame() → projectile::spawnProjectile()
       │
4. projectile::updateProjectiles(dt)  ─── movement, wall collision, cleanup
       │
5. circular_sprite::update(dt)  ─── projectile hit detection
       │
6. renderer::buildRenderState()  ─── snapshot player state
       │
7. renderer::renderFrame(rs)     ─── GPU raycasting (walls, floor, sky)
       │
8. circular_sprite::render(rs)   ─── billboard target entity
       │
9. projectile_renderer::renderProjectiles(rs) ─── CPU z-buffer + billboard quads
       │
10. minimap::renderMinimap(rs)   ─── CPU DDA + pixel buffer
       │
11. hud::renderWeapon()          ─── weapon sprite + ammo bars
       │
12. glfwSwapBuffers()            ─── display frame
```

### 2.4 Dependency Graph

```
settings ←─────────────────────────────┐
    ↑                                  │
    │   map ←───────── map_renderer    │
    │    ↑                  ↑          │
    │    │                  │          │
    │  player ←──── input ──┤          │
    │    ↑           ↑      │          │
    │    │           │      │          │
    │  game_state ───┘      │          │
    │                       │          │
    │  weapon ←── hud_renderer         │
    │    ↑           ↑                 │
    │    │           │                 │
    │  projectile ←──┘                 │
    │    ↑                             │
    │    │                             │
    │  projectile_renderer             │
    │    ↑                             │
    │    │                             │
    │  circular_sprite ────────────────┘
    │
  shader (utility, used by all renderers)
```

---

## 3. Design Decisions

### 3.1 GPU vs CPU Raycasting

**Decision:** The main world rendering (walls, floor, sky) runs entirely in a GLSL fragment shader on the GPU. Overlay systems (minimap, projectiles) use CPU-side raycasting.

**Why GPU for the world?**
- Each screen pixel is independent — perfect for GPU parallelism.
- A 960×720 frame has 691,200 pixels. The GPU processes these in parallel across hundreds of cores; a CPU would iterate sequentially.
- The scene uniforms (player position, direction) are tiny and change once per frame.
- Map data and textures are uploaded once at init, then reside in GPU memory.

**Why CPU for overlays?**
- The minimap is a 160×160 pixel buffer — too small to justify a separate GPU pass.
- Projectile rendering requires sorting sprites back-to-front and per-column z-buffer checks that are more naturally expressed in sequential CPU code.
- These are lightweight operations that don't bottleneck the frame.

**Trade-offs considered:**
- *Full CPU raycasting* (the original approach in the repo memory) would be simpler to debug but slower at high resolutions.
- *Compute shaders* could handle the z-buffer on GPU, but add complexity for marginal gain.

### 3.2 Library Choices

| Library | Purpose | Why Chosen |
|---------|---------|------------|
| **GLFW** | Windowing, input, OpenGL context | Lightweight, cross-platform, no unnecessary complexity. Unlike SDL, it focuses solely on windowing. |
| **GLAD** | OpenGL function loading | Generated loader specific to OpenGL 3.3 core. Ensures only the needed functions are loaded. Alternative: GLEW (heavier, loads everything). |
| **STB Image** | PNG texture loading | Single-header library. No build system integration needed — just `#include` and define implementation in one `.c` file. Alternative: libpng (much heavier dependency). |
| **MinGW-w64** | Compiler toolchain | Bundled with the repo for zero-setup builds on Windows. No Visual Studio required. |

**Why not SDL?** SDL includes audio, threading, and other subsystems that aren't needed. GLFW is purpose-built for OpenGL windowing. However, SDL's audio module would be useful if sound is added (see [Section 7.5](#75-audio-integration)).

**Why no external math library (GLM)?** The math in this project is minimal — 2D rotations, dot products, and basic trigonometry. GLM would be overkill.

### 3.3 Data-Driven Weapon System

Weapons use a **factory pattern with function pointers** rather than traditional C++ inheritance:

```cpp
struct Weapon {
    void (*handleFire)(Weapon& w, float dt, bool lmb, bool rmb);
    void (*advanceAnim)(Weapon& w, float dt);
    // ... data fields ...
};

Weapon createAssaultRifle();  // factory function
```

**Why function pointers instead of virtual methods?**
- No vtable overhead (though negligible here).
- All weapon data lives in a flat struct — trivially copyable, cache-friendly.
- Adding a new weapon means writing one new `.cpp` file with a factory function. No class hierarchy to navigate.
- The shared logic (`weaponFirePrimary`, `updateWeapon`) is in `weapon.cpp`, while per-weapon behavior is in callbacks.

**Why data-driven (config.json)?**
- Every timing value (fire duration, reload speed, recoil), projectile parameter, and ammo count is loaded from `config.json`.
- This allows gameplay tuning without recompilation. Change `"assault_rifle_auto_fire_interval": 0.05` to `0.1` and the fire rate halves instantly.

### 3.4 Namespace-Based Modules

Each module uses a C++ namespace (`player::`, `renderer::`, `minimap::`, etc.) rather than classes with static methods.

**Why?**
- Simpler syntax: `player::movePlayer()` vs `Player::getInstance().movePlayer()`.
- No singleton boilerplate. Module state is file-static (`static` variables in `.cpp`), which is effectively the same as a singleton but without the pattern overhead.
- Clear module boundaries without inheritance hierarchies.

**Trade-off:** File-static state makes unit testing harder (you can't create independent instances). For a project of this size, that trade-off is acceptable.

### 3.5 JSON Configuration

**Decision:** Use a custom minimal flat-JSON parser rather than a library like nlohmann/json.

**Why?**
- The config format is flat key-value pairs — no nested objects or arrays.
- The custom parser is ~80 lines of code vs importing a 25,000-line header.
- It supports `//` comments (not standard JSON, but useful for configuration).
- Zero dependencies.

**Limitation:** Only flat `{ "key": value }` structures are supported. Nested objects would require a real JSON library.

### 3.6 Embedded Shaders and Font

GLSL shaders are stored as C++ raw string literals (`R"(...)"`) in `.cpp` files rather than loaded from external files:

```cpp
const char* fragmentSource = R"(
    #version 330 core
    // ... shader code ...
)";
```

The menu system uses a hardcoded 5×7 bitmap font (ASCII 32–95) compiled into the binary.

**Why?**
- Single-binary deployment. No need to ship shader files or font files alongside the executable.
- No file-loading failures or path issues.
- The shaders are intimately coupled to the C++ code (they share uniform names), so separating them would add complexity without benefit.

**Trade-off:** Shader changes require recompilation. For rapid shader iteration, loading from files would be better, but this project's shaders are stable.

---

## 4. Implementation Walkthrough

### 4.1 Startup Sequence

When `main()` runs, initialization proceeds in strict order:

```
settings::load("config.json", "keybind.json")
    ↓  Parse flat JSON into key→value maps
    ↓  Convert keybind strings ("W", "LEFT_SHIFT") to GLFW key codes
    ↓
window::initGLFW()
    ↓  Initialize GLFW library
    ↓
glfwSwapInterval(1)
    ↓  Enable VSync (1 buffer swap per monitor refresh)
    ↓
window::createWindow(960, 720, "Raycaster")
    ↓  Request OpenGL 3.3 core profile
    ↓  Create window, set framebuffer callback
    ↓
window::initGLAD()
    ↓  Load all OpenGL function pointers
    ↓
map::loadMap("resource/maps/map.txt")
    ↓  Parse ASCII grid into worldMap[256][256]
    ↓
player::initPlayer()
    ↓  Set position (1.5, 1.5), direction (1, 0), plane (0, 0.666)
    ↓
renderer::initRenderer(960, 720)
    ↓  Compile world shader program
    ↓  Upload map grid as R8UI GPU texture
    ↓  Upload wall textures as 2D texture array
    ↓  Upload floor and sky textures
    ↓  Create full-screen quad VAO/VBO
    ↓
minimap::initMinimap(960, 720)
    ↓  Create minimap pixel buffer and GPU texture
    ↓  Compile minimap shader
    ↓  Create top-left corner quad
    ↓
hud::initHUD(960, 720)
    ↓  Compile HUD shader
    ↓  Preload ALL weapon textures (idle, fire, reload frames)
    ↓
projectile_renderer::initProjectileRenderer(960, 720)
    ↓  Compile projectile shader
    ↓  Generate procedural projectile textures (bullet, beam, shotgun)
    ↓
menu_renderer::initMenu(960, 720)
    ↓  Compile menu shader
    ↓  Build 5×7 bitmap font atlas texture
    ↓
projectile::initProjectiles()
    ↓  Clear projectile vector
    ↓
circular_sprite::init(960, 720)
    ↓  Generate circle texture via midpoint algorithm
    ↓  Spawn target at random empty tile
    ↓
game::setState(MAIN_MENU)
    ↓  Show cursor, enter menu
    ↓
Enter game loop
```

**Why this order matters:**
- Settings must load first because every other module queries it during init.
- GLFW must init before creating a window.
- GLAD must load after the OpenGL context exists (after `createWindow`).
- Map must load before renderers, because `renderer::initRenderer` uploads the map grid to GPU memory.
- Player must init before renderers that read `player::player` state.

### 4.2 The Game Loop

The game loop in `main.cpp` is a classic **single-threaded variable-timestep loop**:

```cpp
while (!glfwWindowShouldClose(window)) {
    double now = glfwGetTime();
    float deltaTime = (float)(now - lastTime);
    lastTime = now;
    if (deltaTime > maxDt) deltaTime = maxDt;  // cap to prevent spiral of death

    game::updateMenuInput(window);   // state transitions (ESC, menu navigation)

    switch (game::getState()) {
    case MAIN_MENU:  /* render menu */           break;
    case PAUSED:     /* render scene + overlay */ break;
    case PLAYING:    /* full update + render */   break;
    }

    glfwSwapBuffers(window);  // display the frame
    glfwPollEvents();         // process OS events
}
```

**Delta time capping:** If a frame takes longer than `max_delta_time` (default 0.05s = 50ms), the delta is clamped. This prevents the player from teleporting through walls after a long stall (e.g., window resize, system hiccup).

**VSync:** `glfwSwapInterval(1)` synchronizes buffer swaps with the monitor refresh rate, typically 60 Hz. This means the loop runs at ~60 FPS without burning CPU in a busy loop.

**State machine:** The `GameState` enum (MAIN_MENU, PLAYING, PAUSED) controls what is processed and rendered each frame:

| State | Input | Update | Render |
|-------|-------|--------|--------|
| MAIN_MENU | Menu navigation only | None | Menu overlay |
| PLAYING | Full movement + combat | Physics, projectiles, sprites | World + sprites + minimap + HUD |
| PAUSED | Menu navigation only | None | Frozen world + pause overlay |

### 4.3 GPU Raycasting Shader

The fragment shader in `world_shaders.cpp` is the visual core of the engine. For each pixel on screen, it determines whether that pixel shows sky, floor, or wall.

**Step 1: Compute the ray direction**

```glsl
float camX = 2.0 * px / screenSize.x - 1.0;  // -1 (left) to +1 (right)
vec2 rd = playerDir + playerPlane * camX;       // ray direction for this column
```

**Step 2: Cast the ray (DDA)**

The `castRay()` function performs the DDA algorithm described in [Section 1.2](#12-the-dda-algorithm). It returns the wall type (1–9), which side was hit (0 = N/S face, 1 = E/W face), and the perpendicular distance.

**Step 3: Determine what to draw**

```glsl
int lineH = int(screenSize.y / perpDist);       // wall strip height
int drawStart = -lineH/2 + horizon + vertOffset; // top of wall strip
int drawEnd   =  lineH/2 + horizon + vertOffset; // bottom of wall strip

if (iy < drawStart)   color = renderSky(rd, py);
else if (iy > drawEnd) color = renderFloor(rd, py);
else                   color = renderWall(rd, ...);
```

**Sky rendering:** Uses `atan(rd.y, rd.x)` to compute the heading angle, maps it to a horizontal texture coordinate. The vertical coordinate is based on distance from the horizon line. This creates a panoramic sky that wraps around the full 360°.

**Floor rendering:** Uses perspective-correct texture mapping:

$$
rowDist = \frac{halfH \cdot (1 + playerHeight)}{pixelY - halfH}
$$

$$
floorPos = playerPos + rowDist \cdot rayDir
$$

The fractional part of `floorPos` gives the floor texture coordinates. Optional distance fog darkens far-away floor tiles:

$$
shade = \frac{1}{1 + rowDist^2 \cdot fogDensity}
$$

**Wall rendering:** Computes texture coordinates from the wall hit point. `wallX = fract(hitPosition)` gives the horizontal texture coordinate. The vertical coordinate is the pixel's position within the wall strip. A second side-darkening factor (`wallSideDarken = 0.5`) makes E/W-facing walls darker, creating a subtle 3D effect.

**Jumping:** The `playerHeight` uniform shifts the horizon and wall positions vertically, creating the illusion of the camera moving up/down:

```glsl
float vertOffset = (playerHeight / perpDist) * float(halfH);
```

This cleverly simulates vertical movement without actually adding a Z dimension to the raycaster.

### 4.4 Player Movement and Collision

Player movement in `player.cpp` implements four operations: forward/backward movement, strafing, rotation, and jumping.

**Movement with collision detection:**

```cpp
// Compute movement vector from direction + strafe
float rightX = -player.dirY;  // perpendicular to direction
float rightY =  player.dirX;
float moveX = player.dirX * forward + rightX * strafe;
float moveY = player.dirY * forward + rightY * strafe;

float newX = player.posX + moveX * speed;
float newY = player.posY + moveY * speed;
```

Collision uses **axis-separated checking** with a margin:

```cpp
float margin = 0.2f;
int checkX = (int)(newX + (moveX > 0 ? margin : -margin));
if (worldMap[(int)player.posY][checkX] == 0)
    player.posX = newX;   // X movement is safe

int checkY = (int)(newY + (moveY > 0 ? margin : -margin));
if (worldMap[checkY][(int)player.posX] == 0)
    player.posY = newY;   // Y movement is safe
```

**Why axis-separated?** By checking X and Y independently, the player can "slide" along walls. If they walk diagonally into a wall, the X component might be blocked while Y is allowed (or vice versa). This feels natural — you slide along the wall instead of stopping dead.

**The 0.2 margin** prevents the player from getting arbitrarily close to walls, which would cause visual artifacts (clipping into the wall texture).

**Jumping and gravity:**

```cpp
void updatePhysics(float dt) {
    player.velZ -= GRAVITY * dt;       // gravity pulls down
    player.posZ += player.velZ * dt;   // integrate vertical position
    if (player.posZ <= 0.0f) {         // hit the ground
        player.posZ = 0.0f;
        player.velZ = 0.0f;
    }
}

void jump() {
    if (player.posZ == 0.0f)           // only jump when grounded
        player.velZ = JUMP_SPEED;
}
```

This is basic Euler integration of the equation $v = v_0 - g \cdot t$. The `posZ` value is passed to the shader as `playerHeight` to offset the rendered view.

### 4.5 Weapon System

The weapon system is a **state machine** with four states:

```
         ┌──── fire ────┐
         ▼              │
IDLE ──────→ FIRE_BULLET ──→ IDLE
  │                           ▲
  ├──alt fire→ FIRE_GRENADE ──┘
  │                           ▲
  └──reload ─→ RELOAD ───────┘
```

**Each weapon is a `Weapon` struct** containing:
- Type, state, animation timer
- Texture handles (idle, fire, alt-fire, reload animation frames)
- Timing values (fire duration, reload frame duration)
- Ammo counts and limits
- Function pointers for per-weapon logic (`handleFire`, `advanceAnim`)
- Ammo bar display configuration

**Per-frame update cycle:**

```
1. Clear firedThisFrame / firedAltThisFrame flags
2. Call weapon.handleFire(dt, lmb, rmb)
   ├── Check if button was just pressed (edge detection)
   ├── For auto-fire weapons (AR, energy): accumulate autoFireTimer
   └── Call weaponFirePrimary() or weaponFireSecondary() when appropriate
3. Store button states for next-frame edge detection
4. Call weapon.advanceAnim(dt)
   ├── Advance animTimer
   ├── Transition states when animation completes
   └── For reload: advance through animation frames
```

**The four weapons:**

| Weapon | Fire Mode | Alt-Fire | Ammo | Special |
|--------|-----------|----------|------|---------|
| Assault Rifle | Auto-fire (hold LMB) | Grenade launcher (RMB) | 30 bullets / 5 grenades | Reload animation (11 frames) |
| Shotgun | Semi-auto (click LMB) | None | Unlimited | Auto-pump after each shot (5-frame reload) |
| Energy Weapon | Hold-to-fire beam (LMB) | Overcharge beam (RMB) | Energy bars (drain/recharge) | Overheats when depleted |
| Handgun | Semi-auto (click LMB) | None | Unlimited | Minimal recoil |

**Energy weapon detail:** Instead of integer ammo, the energy weapon uses `charge1` and `charge2` floats (0.0 to 1.0). Firing drains the charge; releasing recharges it slowly. When charge hits 0, the weapon is "depleted" and enters a mandatory overheat reload animation. This is the most complex weapon behavior.

### 4.6 Projectile System

Projectiles are stored in a `std::vector<Projectile>` and updated each frame.

**Spawn:**

```cpp
Projectile p{};
p.x = x;  p.y = y;             // position
p.dx = dirX/len;  p.dy = dirY/len;  // normalized direction
p.speed = speed;
p.lifetime = 5.0f;             // seconds until auto-despawn
p.range = range;               // max travel distance
p.traveled = 0.0f;
p.visualScale = visualScale;   // billboard size
p.spawnZ = spawnZ;             // player height when fired
```

**Update (each frame):**

```cpp
float step = p.speed * dt;
p.x += p.dx * step;       // move forward
p.y += p.dy * step;
p.traveled += step;
p.lifetime -= dt;

// Kill if expired, out of range, or hit a wall
if (p.lifetime <= 0 || p.traveled >= p.range) { p.alive = false; }
if (worldMap[(int)p.y][(int)p.x] != 0) { p.alive = false; }
```

Dead projectiles are cleaned up using the erase-remove idiom:

```cpp
projectiles.erase(
    std::remove_if(projectiles.begin(), projectiles.end(),
                   [](const Projectile& p) { return !p.alive; }),
    projectiles.end());
```

### 4.7 Billboard Sprite Rendering

Projectiles and the circular target are rendered as **billboards** — flat quads that face the camera. The rendering pipeline for projectiles:

**Step 1: Build a CPU-side z-buffer**

For each screen column, a DDA ray is cast to find the wall distance. This creates a per-column depth array.

**Step 2: Transform sprite to screen space**

```cpp
// Relative position to player
float sx = spriteX - posX;
float sy = spriteY - posY;

// Camera-space transform (inverse of camera matrix)
float invDet = 1.0 / (planeX * dirY - dirX * planeY);
float transformX = invDet * (dirY * sx - dirX * sy);
float transformY = invDet * (-planeY * sx + planeX * sy);  // depth

// Screen-space position
float screenX = (screenWidth / 2) * (1 + transformX / transformY);
float spriteSize = abs(screenHeight / transformY) * scale;
```

**Step 3: Depth test against z-buffer**

For each column the sprite covers, check if the sprite's depth (`transformY`) is less than the wall depth in that column. The sprite is only drawn if at least one column is visible.

**Step 4: Render as a textured quad**

The sprite is drawn as a screen-space quad using alpha blending so the circular texture shows transparency.

**Back-to-front sorting:** Multiple sprites are sorted by distance (farthest first) so that closer sprites overdraw farther ones correctly.

### 4.8 Minimap Renderer

The minimap is rendered entirely on the CPU into a 160×160 RGBA pixel buffer, then uploaded to a GPU texture and drawn as a quad in the top-left corner.

**Layers drawn (in order):**

1. **Background:** Wall cells are light grey (199, 199, 199); empty cells are black.
2. **Ray lines (green):** 11 rays are cast (spread across the FOV) and their paths are drawn as green (0, 178, 0) pixels, showing the player's view cone.
3. **Player dot (red):** A filled circle at the player's position, radius proportional to the map-to-pixel scale.
4. **Direction line (yellow):** A 3-cell-long line in the player's facing direction.

**Why CPU-side?** The minimap is a tiny overlay (160×160 = 25,600 pixels). Setting up a separate GPU pass with framebuffers would be more complex than just filling a pixel buffer. The CPU can handle this trivially.

### 4.9 Menu System and Game States

The game has three states managed by a finite state machine in `game_state.cpp`:

```
                START
                  │
                  ▼
           ┌─────────────┐
           │  MAIN_MENU   │ ← Enter to select
           │              │ ← ESC to quit
           └──────┬───────┘
                  │ "START" selected
                  ▼
           ┌──────────────┐
           │   PLAYING     │ ←── ESC ──── ┐
           │               │              │
           └───────────────┘              │
                                          ▼
                                   ┌──────────────┐
                                   │   PAUSED      │
                                   │  - Continue   │
                                   │  - Main Menu  │
                                   │  - Quit       │
                                   └──────────────┘
```

**Menu navigation** uses edge detection on arrow keys and Enter. The `updateMenuInput()` function runs every frame, regardless of game state. This ensures state transitions (ESC to pause) are always responsive.

**Cursor management:** In PLAYING state, the cursor is captured (`GLFW_CURSOR_DISABLED`) for mouse look. In menus, it's normal (`GLFW_CURSOR_NORMAL`).

**Menu rendering** uses solid-colour rectangles and a built-in 5×7 bitmap font. The font is stored as 7 bytes per character, where each byte's lower 5 bits represent one row of pixels. These are unpacked into a 16×6 character atlas texture (80×42 pixels) at startup.

**Text rendering:** Each character is drawn as a textured quad sampled from the font atlas:

```
Character 'A' (65 - 32 = 33) → atlas position: column 1, row 2
UV coordinates: u0 = 1*5/80, u1 = 2*5/80, v0 = 2*7/42, v1 = 3*7/42
```

### 4.10 Settings and Keybinding

The settings system in `settings.cpp` provides a **two-tier configuration**:

1. **`config.json`** — numeric game parameters (float and int values)
2. **`keybind.json`** — action-to-key mappings using GLFW key name strings

**JSON parsing:** The custom parser handles:
- `"key": number` → stored as a float
- `"key": "string"` → stored as a string
- `// comments` → ignored (non-standard JSON, but practical)
- Flat structure only (no nested objects/arrays)

**Keybind resolution:** String key names are converted to GLFW key codes at load time via a lookup table:

```
"W"           → GLFW_KEY_W  (87)
"LEFT_SHIFT"  → GLFW_KEY_LEFT_SHIFT  (340)
"SPACE"       → GLFW_KEY_SPACE  (32)
"1"           → GLFW_KEY_1  (49)
```

**Usage pattern:** Modules query settings with a default fallback:

```cpp
float speed = settings::getFloat("move_speed", 3.0f);
int key = settings::getKey("jump", GLFW_KEY_SPACE);
```

If the JSON file is missing or a key doesn't exist, the default is used silently. This makes the system robust — the game works even without config files.

### 4.11 Map Format and Loading

Maps are plain ASCII text files. Each character represents one grid cell:

```
1111111111111111
1000000000000001
1001111001111001
1001000001000101
1001000000000001
1001111001111001
1000000000000001
1111111111111111
```

| Character | Meaning |
|-----------|---------|
| `0` | Empty space (walkable) |
| `1`–`9` | Wall (different types, mapped to textures) |
| Any other | Treated as empty |

**Loading (`map::loadMap`):**
1. Open the file, read line by line.
2. For each character, convert `'0'`–`'9'` to integer 0–9.
3. Track maximum width across all lines → `mapWidth`.
4. Count lines → `mapHeight`.
5. Maximum supported size: 256×256.

**The map grid** is stored as `int worldMap[256][256]` — a fixed-size array. This means the map is always allocated at its maximum size regardless of actual map dimensions.

### 4.12 The Map Editor Tool

`tools/map_editor.cpp` is a standalone application (separate from the game) that provides a visual map editor.

**Features:**
- **Click and drag** to place or erase walls
- **Brush types 1–9** selected with number keys
- **Draw/Erase mode toggle** (E key)
- **Grid overlay** toggle (G key)
- **Clear interior** (C key) — clears everything except the perimeter
- **Undo/Redo** (Ctrl+Z / Ctrl+Y) — up to 256 snapshots
- **Save** (Ctrl+S) → writes to `resource/maps/map.txt`
- **Bresenham line drawing** — dragging interpolates between cells for smooth lines

**Undo system:** Each edit pushes a full 256×256 grid snapshot to an undo stack. This is ~262KB per snapshot, with 256 slots = ~67MB max. Simple and reliable, though memory-intensive.

**The perimeter is always walls** — the editor enforces this to prevent the player from walking off the map edge.

**Building and running:**

```bat
mingw64\bin\mingw32-make -f Makefile editor
build\map_editor.exe 32
```

### 4.13 Midpoint Circle Sprite

`circular_sprite.cpp` implements a target entity using the **midpoint circle drawing algorithm** — a classic computer graphics algorithm for rasterizing circles.

**The algorithm:**

```cpp
int x = 0, y = radius, p = 1 - radius;
while (x <= y) {
    // Draw 4 horizontal spans (exploiting 8-fold symmetry)
    hline(cx-x, cx+x, cy+y);
    hline(cx-x, cx+x, cy-y);
    hline(cx-y, cx+y, cy+x);
    hline(cx-y, cx+y, cy-x);
    x++;
    if (p < 0) p += 2*x + 1;
    else { y--; p += 2*(x-y) + 1; }
}
```

This exploits the circle's 8-fold symmetry: computing one octant gives all 8. The "midpoint" decision variable `p` determines whether the next pixel should step diagonally or horizontally, using only integer arithmetic.

**Two passes:**
1. **Fill pass:** Draw horizontal spans between symmetric points → filled disc.
2. **Outline pass:** Plot individual pixels at the circumference (multiple widths for a thick outline).

The generated texture is uploaded to the GPU and rendered as a billboard sprite, just like projectiles. When hit by a projectile (distance < `hitRadius`), the sprite "respawns" at a random empty tile.

---

## 5. Practical Usage

### 5.1 Building the Project

**Prerequisites:** None. The repository bundles everything:
- MinGW-w64 compiler toolchain (`mingw64/`)
- GLFW static library (`lib/`)
- GLAD, STB headers (`include/`)

**Build command (simplest):**

Double-click `build.bat` in File Explorer, or from a terminal:

```bat
cd "path\to\Raycaster-OpenGL"
build.bat
```

The script:
1. Changes to the project directory (safe for double-click from Explorer).
2. Adds `mingw64\bin` to PATH.
3. Runs `mingw32-make -f Makefile`.
4. On success, automatically launches `build\main.exe`.

**Manual build:**

```bat
set PATH=mingw64\bin;%PATH%
mingw32-make -f Makefile
```

**Build outputs:**

| Target | Command | Output |
|--------|---------|--------|
| Game | `make` (default) | `build/main.exe` |
| Map Editor | `make editor` | `build/map_editor.exe` |
| Clean | `make clean` | Removes all executables |

### 5.2 Running the Game

```bat
build\main.exe
```

**First launch:** The game starts on the **Main Menu** screen. Press Enter on "START" to begin playing.

**Gameplay controls:**

| Input | Action |
|-------|--------|
| W / S | Move forward / backward |
| A / D | Strafe left / right |
| Mouse | Look left / right |
| Left Shift | Sprint |
| Space | Jump |
| Left Click | Fire weapon |
| Right Click | Alt-fire (grenade / overcharge beam) |
| R | Reload |
| 1 / 2 / 3 / 4 | Switch weapon (AR / Shotgun / Energy / Handgun) |
| M | Toggle minimap |
| L | Toggle fog/lighting |
| ESC | Pause menu |

**Game flow:**
1. Main Menu → "START" → Playing
2. Explore the map, switch weapons, shoot at the red target circle
3. ESC → Pause → Continue / Main Menu / Quit

### 5.3 Using the Map Editor

```bat
build\map_editor.exe 64
```

The argument is the map side length (N×N). Omit it to be prompted interactively.

**Workflow:**
1. The editor opens an 800×800 window showing the grid.
2. If `resource/maps/map.txt` exists, it's loaded into the editor.
3. Select a wall type with 1–9.
4. Left-click and drag to paint walls.
5. Right-click to erase.
6. Press G to toggle the grid overlay.
7. Save with Ctrl+S.
8. The saved map is immediately playable by the game.

### 5.4 Customizing via Configuration

Edit `src/settings/config.json` to change gameplay parameters:

**Display:**
```json
"screen_width": 1280,
"screen_height": 960
```

**Player tuning:**
```json
"move_speed": 4.0,        // faster walking
"sprint_speed": 6.0,      // faster sprinting
"player_fov": 0.8,        // wider field of view
"mouse_sensitivity": 0.003 // more responsive mouse
```

**Combat tuning:**
```json
"assault_rifle_auto_fire_interval": 0.03,  // faster fire rate
"shotgun_recoil": 0.12,                    // more kick
"energy_drain_normal": 10.0                // longer beam duration
```

**Visual tuning:**
```json
"fog_density": 0.08,           // thicker fog
"wall_side_darkening": 0.3     // darker E/W walls
```

Changes take effect on next launch — no recompilation needed.

### 5.5 Rebinding Controls

Edit `src/settings/keybind.json`:

```json
{
    "move_forward": "W",
    "move_backward": "S",
    "strafe_left": "A",
    "strafe_right": "D",
    "sprint": "LEFT_SHIFT",
    "jump": "SPACE",
    "toggle_minimap": "TAB",
    "equip_assault_rifle": "1",
    "reload": "R"
}
```

**Supported key names:**
- Single letters: `A`–`Z`
- Single digits: `0`–`9`
- Special: `SPACE`, `ESCAPE`, `ENTER`, `TAB`, `BACKSPACE`
- Modifiers: `LEFT_SHIFT`, `RIGHT_SHIFT`, `LEFT_CONTROL`, `RIGHT_CONTROL`, `LEFT_ALT`, `RIGHT_ALT`
- Arrows: `LEFT`, `RIGHT`, `UP`, `DOWN`
- Function keys: `F1`–`F12`

---

## 6. Debugging & Pitfalls

### 6.1 Common Build Errors

**"g++ is not recognized"**
- Cause: MinGW not in PATH.
- Fix: Run `build.bat` (it adds `mingw64\bin` to PATH). Or manually: `set PATH=mingw64\bin;%PATH%`.

**"cannot find -lglfw3"**
- Cause: Library not in the expected path.
- Fix: Verify `lib/libglfw3.a` exists. The Makefile expects it at `-Llib`.

**"undefined reference to `glfwInit`"**
- Cause: Link order issue or missing `-lglfw3`.
- Fix: Ensure the Makefile `LIBS` line includes `-lglfw3` before `-lopengl32`.

**"stb_image.h: no such file"**
- Cause: Include path not set.
- Fix: The `-Iinclude` flag in the Makefile should resolve this. Verify the file exists at `include/stb/stb_image.h`.

**Multiple definition errors**
- Cause: STB implementation macros defined in multiple translation units.
- Fix: `STB_IMAGE_IMPLEMENTATION` must only appear in `src/stb_impl.c`, never in headers.

### 6.2 Runtime Issues

**Black screen on startup**
- Cause: Texture files not found (wrong working directory).
- Fix: Run from the project root. `build.bat` handles this with `cd /d "%~dp0"`. Check console for `WARNING: could not load ...` messages.

**"Could not load map, exiting."**
- Cause: `resource/maps/map.txt` is missing.
- Fix: Create a map with the map editor, or ensure the file exists.

**Very low FPS**
- Cause: Running on integrated graphics (the GPU is running the fragment shader for every pixel).
- Fix: Ensure your system uses the discrete GPU. On laptops, you may need to set "High Performance" in GPU settings for the executable.

**Player won't move**
- Cause: Player spawns inside a wall (`player_spawn_x/y` in config.json points to a wall cell).
- Fix: Set spawn to (1.5, 1.5) or any other empty cell (the 0.5 offset centers the player in the cell).

**Mouse look is jerky on first frame**
- This is handled by the `mousePrimed` flag in `input.cpp`. The first frame's cursor position is stored without computing a delta, preventing a wild initial rotation.

### 6.3 Shader Bugs

**"Shader compilation failed"**
- The error log will be printed to stderr. Common causes:
  - Syntax errors in GLSL (if you modified the shaders).
  - Using GLSL features above version 330 (the project targets GLSL 330).
- The program exits immediately on shader failure — this is intentional, as there's no fallback.

**Fisheye distortion**
- If you modify the distance calculation, ensure you're using **perpendicular distance**, not Euclidean distance. The formula in `castRay()` is:
  ```glsl
  perpDist = (mapX - playerPos.x + (1 - stepX) * 0.5) / rd.x;
  ```
  If you accidentally use `sqrt(dx*dx + dy*dy)`, you'll get fisheye.

**Floor texture swimming**
- Check that you're using `fract()` on the floor position to get texture coordinates. Without `fract()`, the texture would race across the floor.

### 6.4 Edge Cases and Limitations

| Limitation | Explanation |
|------------|-------------|
| **Max map size: 256×256** | Fixed-size `worldMap[256][256]` array. Larger maps require changing `MAX_MAP_DIMENSION` and recompiling. |
| **No vertical look (pitch)** | The camera has no pitch — you can't look up or down. This is a fundamental raycasting limitation. |
| **No height variation** | All walls are the same height. No stairs, ramps, or multi-story buildings. |
| **Single wall texture per type** | All wall types (1–9) currently use the same texture (uploaded to all 4 array layers). |
| **No doors or moving walls** | The map is static. Doors would require special map cell types and animation logic. |
| **Projectiles don't damage anything** | Projectiles collide with walls but have no damage system. The circular sprite simply respawns when hit. |
| **No audio** | The `resource/audio/` directory exists but no audio system is implemented. |
| **Uppercase-only menu text** | The bitmap font only covers ASCII 32–95 (space through underscore). Lowercase letters (97–122) are not in the font atlas. |
| **Fixed window size at startup** | Resolution is read from config at startup. The framebuffer callback handles window resizing, but the internal render resolution stays the same. |

---

## 7. Extension & Scaling

### 7.1 Adding New Wall Textures

Currently the engine supports 4 wall texture layers (`N_WALL_TEX = 4`), all loaded from the same PNG file. To use different textures per wall type:

1. **Prepare texture files** — e.g., `resource/textures/wall/wall_1.png` through `wall_4.png`.

2. **Modify `map_renderer.cpp`** — In `initRenderer()`, replace the single-texture loop with individual loads:

```cpp
const char* wallPaths[N_WALL_TEX] = {
    "resource/textures/wall/wall_1.png",
    "resource/textures/wall/wall_2.png",
    "resource/textures/wall/wall_3.png",
    "resource/textures/wall/wall_4.png",
};
for (int i = 0; i < N_WALL_TEX; i++) {
    int tw, th, tc;
    unsigned char* raw = stbi_load(wallPaths[i], &tw, &th, &tc, 4);
    // resample and upload to layer i
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i,
                    TEX_SZ, TEX_SZ, 1, GL_RGBA, GL_UNSIGNED_BYTE, buf.data());
}
```

3. **Increase `N_WALL_TEX`** if you need more than 4. Update the shader's `texIdx` clamping:
```glsl
int texIdx = wallType - 1;
if (texIdx < 0 || texIdx >= N_WALL_TEX) texIdx = 0;
```

### 7.2 Adding New Weapons

1. **Create a new file**, e.g., `src/weapons/sniper_rifle.cpp`.

2. **Add a factory function:**
```cpp
Weapon createSniperRifle() {
    Weapon w;
    w.type = WeaponType::SNIPER_RIFLE;  // add to enum
    w.fireDur = 0.5f;
    w.unlimited = false;
    w.maxAmmo1 = 5;
    w.ammo1 = 5;
    // ... set all fields ...
    w.handleFire = srHandleFire;
    w.advanceAnim = srAdvanceAnim;
    w.texIdle = loadWeaponTexture("resource/textures/weapons/sniper/idle.png");
    w.texFire = loadWeaponTexture("resource/textures/weapons/sniper/fire.png");
    return w;
}
```

3. **Add `SNIPER_RIFLE` to the `WeaponType` enum** in `weapon.h`.

4. **Register it in `hud_renderer.cpp`:**
   - Increase the `weapons[]` array size.
   - Add `weapons[4] = createSniperRifle();` in `initHUD()`.
   - Add a `equipSniperRifle()` function.

5. **Add the key binding** in `input.cpp` for the number key `5`.

6. **Add projectile parameters** in the switch statement in `input.cpp`.

### 7.3 Adding Enemies

Enemies would require several new systems:

1. **Enemy data structure:** Position, health, AI state, sprite index.
2. **AI logic:** Pathfinding (A* on the grid), state machine (idle → chase → attack).
3. **Billboard rendering:** Extend `projectile_renderer` or create a new `enemy_renderer` using the same sprite/z-buffer approach.
4. **Damage system:** Projectile-enemy collision detection (distance check, similar to `circular_sprite::update`).
5. **Map integration:** Spawn points defined in the map format (new character codes, e.g., `A`–`Z` for enemies).

### 7.4 Multi-Floor Support

True multi-floor raycasting requires significant changes:

- **Approach 1 (stacked maps):** Multiple 2D layers with floor/ceiling heights per cell. Each ray checks multiple layers.
- **Approach 2 (Portal rendering):** When a ray crosses a floor transition, it "portals" to the corresponding position on the other floor.
- **Approach 3 (Switch to full 3D):** Use OpenGL's depth buffer and actual 3D geometry. At this point, you're no longer raycasting.

For most Wolfenstein-style games, Approach 1 is sufficient.

### 7.5 Audio Integration

The `resource/audio/` directory has `music/` and `sfx/` subdirectories, suggesting audio was planned.

**Recommended approach:** Add SDL2_mixer or miniaudio (single-header library):

1. Add `#include "miniaudio.h"` with a one-file implementation.
2. Create `src/audio/audio.{cpp,h}` with `init()`, `playSound()`, `playMusic()`.
3. Load WAV/OGG files from `resource/audio/sfx/` and `resource/audio/music/`.
4. Trigger effects: weapon fire, footsteps, menu selection.

### 7.6 Performance Optimization

**Current bottlenecks:**
- The fragment shader runs DDA for every pixel, including floor pixels (expensive `fract()` and distance computation).
- The CPU-side z-buffer in `projectile_renderer` re-casts all rays every frame when projectiles exist.

**Optimization opportunities:**
- **Reduce floor ray count:** The floor rendering could use a lookup table instead of per-pixel DDA.
- **Cache the z-buffer:** If only the main renderer needs it, compute it once and share it between `projectile_renderer` and `circular_sprite`.
- **LOD for minimap rays:** Reduce the minimap ray count for large maps.
- **Frustum culling for sprites:** Skip transform calculations for sprites clearly outside the FOV.
- **Resolution scaling:** Render at half resolution and upscale — the retro aesthetic supports it.

### 7.7 Networking / Multiplayer

For a basic multiplayer extension:

1. **Client-server model:** One player hosts; others connect over TCP/UDP.
2. **Shared state:** Player positions, directions, and projectile locations are synchronized.
3. **Prediction:** Use client-side prediction with server reconciliation for responsive movement.
4. **Libraries:** ENet (reliable UDP), or raw Winsock for Windows-only.

This is a substantial undertaking, but the modular architecture supports it — the `player::` namespace can be extended to an array of players, and the renderer already takes `RenderState` as a parameter.

---

## 8. Learning Path

### Stage 1: Understand the Foundations (1–2 weeks)

**Goal:** Grasp raycasting theory and 2D math.

1. Read Section 1 of this document completely.
2. Study the DDA algorithm in `world_shaders.cpp` — trace through `castRay()` by hand with sample values.
3. Open the game, enable the minimap (M key), and watch the green rays extend from the player dot. Understand what each ray represents.
4. Modify `config.json`: change `player_fov` from 0.666 to 0.4 (narrow) and 1.0 (wide). Observe how the view changes.
5. Modify `fog_density` and `wall_side_darkening`. Understand their visual effect.

**Key reading:** Lode's Raycasting Tutorial (the classic reference for Wolfenstein-style raycasting).

### Stage 2: Understand the Code Structure (1 week)

**Goal:** Navigate the codebase confidently.

1. Read `main.cpp` end to end (< 100 lines). Understand the initialization order and game loop.
2. Trace a single frame of the PLAYING state. Follow each function call.
3. Read `player.cpp` completely (< 90 lines). Understand axis-separated collision.
4. Read `map.cpp` completely (< 40 lines). Open `resource/maps/map.txt` and correlate the numbers to in-game rooms.
5. Read `settings.cpp`. Change a setting, rebuild, verify it works.

### Stage 3: Understand the Rendering Pipeline (1–2 weeks)

**Goal:** Understand how pixels get from math to screen.

1. Study `world_shaders.cpp` — the GLSL fragment shader. This is the most complex single file.
2. Understand the `castRay()` → `renderWall()` / `renderFloor()` / `renderSky()` decision tree.
3. Read `map_renderer.cpp` to understand how textures and uniforms are set up on the CPU side.
4. Study `minimap_renderer.cpp` — a simpler, CPU-side implementation of similar concepts.
5. Modify the sky texture path to a different image. See the effect.

### Stage 4: Understand Game Systems (1 week)

**Goal:** Understand weapons, projectiles, and HUD.

1. Read `weapon.h` — the struct definition and all fields.
2. Read `assault_rifle.cpp` — the most feature-complete weapon.
3. Read `energy_weapon.cpp` — the most complex weapon (charge/drain mechanics).
4. Read `projectile.cpp` — spawning and movement.
5. Read `projectile_renderer.cpp` — billboard rendering and z-buffer depth testing.
6. Read `hud_renderer.cpp` — weapon sprite positioning, bob animation, ammo bars.

### Stage 5: Make Modifications (ongoing)

**Goal:** Prove mastery through implementation.

1. **Easy:** Add a new wall texture (see [Section 7.1](#71-adding-new-wall-textures)).
2. **Easy:** Change the circular sprite's color via config.json.
3. **Medium:** Create a new map with the map editor and playtest it.
4. **Medium:** Add a new weapon type (see [Section 7.2](#72-adding-new-weapons)).
5. **Medium:** Modify the shader to add a crosshair at screen center.
6. **Hard:** Add a health system — the circular sprite shoots back.
7. **Hard:** Implement doors (animated map cells).
8. **Expert:** Add audio (see [Section 7.5](#75-audio-integration)).
9. **Expert:** Add a second floor to a room (see [Section 7.4](#74-multi-floor-support)).

### Stage 6: Master the Architecture

**Goal:** Be able to rewrite any module from scratch.

1. Implement a raycaster from scratch (separate project) using only the math from Section 1.
2. Replace the GPU raycaster with a CPU one (the old architecture) and benchmark the difference.
3. Write a procedural texture generator and integrate it.
4. Profile the game with a GPU debugger (RenderDoc) and identify optimization opportunities.
5. Extend the map format to support entity placement, then build an enemy system.

---

## Appendices

### A. File Manifest

| File | Lines (approx.) | Purpose |
|------|-----------------|---------|
| `src/main.cpp` | 100 | Entry point, game loop |
| `src/glad.c` | 2000 | Auto-generated OpenGL loader |
| `src/stb_impl.c` | 10 | STB library implementation defines |
| `src/core/window.cpp` | 40 | GLFW/GLAD initialization |
| `src/core/input.cpp` | 220 | Input processing + projectile spawning |
| `src/core/game_state.cpp` | 100 | Game state machine |
| `src/map/map.cpp` | 40 | Map file loading |
| `src/player/player.cpp` | 90 | Movement, rotation, physics |
| `src/renderer/map_renderer.cpp` | 230 | GPU texture setup and frame rendering |
| `src/renderer/world_shaders.cpp` | 170 | GLSL raycasting shader source |
| `src/renderer/minimap_renderer.cpp` | 200 | CPU minimap overlay |
| `src/renderer/hud_renderer.cpp` | 280 | Weapon HUD and ammo bars |
| `src/renderer/projectile_renderer.cpp` | 300 | Billboard projectile rendering |
| `src/renderer/menu_renderer.cpp` | 350 | Menu screens with bitmap font |
| `src/renderer/shader.cpp` | 45 | Shader compilation utility |
| `src/weapons/weapon.cpp` | 100 | Shared weapon logic |
| `src/weapons/assault_rifle.cpp` | 100 | AR factory + behavior |
| `src/weapons/shotgun.cpp` | 70 | Shotgun factory + behavior |
| `src/weapons/energy_weapon.cpp` | 130 | Energy weapon factory + behavior |
| `src/weapons/handgun.cpp` | 40 | Handgun factory + behavior |
| `src/projectile/projectile.cpp` | 60 | Projectile system |
| `src/circular_sprite/circular_sprite.cpp` | 270 | Target entity with midpoint circle |
| `src/settings/settings.cpp` | 180 | JSON config loading |
| `tools/map_editor.cpp` | 450 | Visual map editor |

### B. Build Flags

```makefile
CC       = g++
INCLUDES = -Iinclude
LIBS     = -Llib -lglfw3 -lopengl32 -lgdi32 -lwinmm -luser32
           -static-libgcc -static-libstdc++
```

| Flag | Purpose |
|------|---------|
| `-Iinclude` | Search `include/` for headers (GLFW, GLAD, STB, KHR) |
| `-Llib` | Search `lib/` for static libraries |
| `-lglfw3` | Link GLFW3 static library |
| `-lopengl32` | Link Windows OpenGL implementation |
| `-lgdi32` | Windows GDI (required by GLFW) |
| `-lwinmm` | Windows multimedia (timer functions) |
| `-luser32` | Windows user interface (window management) |
| `-static-libgcc` | Statically link GCC runtime (no DLL dependency) |
| `-static-libstdc++` | Statically link C++ runtime (no DLL dependency) |

### C. Uniform Reference

Uniforms set per frame in the world shader:

| Uniform | Type | Value |
|---------|------|-------|
| `playerPos` | `vec2` | Player (x, y) position in map space |
| `playerDir` | `vec2` | Player direction vector |
| `playerPlane` | `vec2` | Camera plane vector (perpendicular to dir) |
| `playerHeight` | `float` | Player vertical position (jumping) |
| `screenSize` | `vec2` | Screen resolution in pixels |
| `mapSize` | `ivec2` | Map dimensions (width, height) |
| `lightingEnabled` | `bool` | Whether fog/distance shading is active |

Uniforms set once at init:

| Uniform | Type | Value |
|---------|------|-------|
| `mapTex` | `usampler2D` | Texture unit 0 — map grid (R8UI) |
| `wallTex` | `sampler2DArray` | Texture unit 1 — wall texture array |
| `floorTex` | `sampler2D` | Texture unit 2 — floor texture |
| `skyTex` | `sampler2D` | Texture unit 3 — sky panorama |
| `fogDensity` | `float` | Fog falloff rate (default 0.04) |
| `wallSideDarken` | `float` | E/W wall darkening factor (default 0.5) |

---

*This documentation covers the Raycaster-OpenGL project as of its current state. For the latest changes, consult the source files directly.*
