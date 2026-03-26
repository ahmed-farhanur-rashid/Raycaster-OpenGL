# Raycaster

A Wolfenstein-style pseudo-3D raycasting engine built with C++ and OpenGL, using GLFW for windowing and GLAD for OpenGL loading. No external dependencies beyond what's included in this repository.

![Raycaster](https://img.shields.io/badge/OpenGL-3.3-blue) ![Language](https://img.shields.io/badge/C%2B%2B-11-orange) ![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)

---

## Features

- **DDA raycasting** — fast, accurate wall detection per screen column
- **3D projection** — perpendicular-distance wall strip rendering
- **4 procedural wall textures** — brick, stone, blue tile, wood planks
- **Distance-based shading / fog** — walls darken with distance
- **Billboard sprites** — barrel, lamp, and pillar objects with depth sorting and z-buffer occlusion
- **WASD movement** — smooth walking, strafing, and rotation with collision detection
- **Minimap overlay** — real-time top-down view with ray visualization
- **Lighting toggle** — turn distance fog on/off at runtime
- **Self-contained** — ships with its own MinGW64 toolchain; no system install required

---

## Controls

| Key | Action |
|-----|--------|
| W / Up | Move forward |
| S / Down | Move backward |
| A | Strafe left |
| D | Strafe right |
| Left arrow | Rotate left |
| Right arrow | Rotate right |
| M | Toggle minimap |
| L | Toggle lighting/fog |
| ESC | Quit |

---

## Building

Double-click `build.bat` from Explorer, or run it from a terminal:

```bat
build.bat
```

The script uses the bundled `mingw64/` toolchain — no system MinGW or Make installation needed. The compiled binary is output to `build/main.exe`.

To rebuild manually:

```bat
mingw64\bin\mingw32-make -f Makefile
```

---

## Project Structure

```
src/
  main.cpp              - Entry point, game loop
  glad.c                - OpenGL loader (generated)
  core/
    window.cpp/h        - GLFW window + GLAD init
    input.cpp/h         - Keyboard input, toggle state
  map/
    map.cpp/h           - Map data and sprite definitions
  player/
    player.cpp/h        - Player state, movement, collision, rotation
  renderer/
    map_renderer.cpp/h  - Full raycasting renderer (walls, sprites, minimap)
    shader.cpp/h        - GLSL shader compilation/linking
include/                - GLFW, GLAD, GLUT headers
lib/x64/                - Pre-built GLFW/FreeGLUT libraries (64-bit)
mingw64/                - Bundled MinGW64 compiler toolchain
build/                  - Output directory
```

---

## Implementation Stages

| Stage | Description |
|-------|-------------|
| 0 | OpenGL window, GLAD init, game loop with delta time |
| 1 | 16×16 hardcoded map grid with 4 wall types |
| 2 | DDA ray casting — per-column wall hit detection |
| 3 | 3D projection — strip heights from perpendicular distance |
| 4 | WASD + arrow key controls, wall collision detection |
| 5 | Procedural textures mapped onto walls, distance-based fog |
| 6 | Minimap overlay with player position and ray lines |
| 7 | Billboard sprites with depth sorting and z-buffer clipping |
| 8 | Lighting toggle (L), minimap toggle (M), clean architecture |

---

## Dependencies (all bundled)

| Library | Version | Purpose |
|---------|---------|---------|
| GLFW | 3.x | Window creation, input, OpenGL context |
| GLAD | — | OpenGL 3.3 core function loader |
| FreeGLUT | — | Fallback GL utility (linked but not used directly) |
| MinGW64 | 15.2.0 | GCC-based compiler toolchain for Windows |
