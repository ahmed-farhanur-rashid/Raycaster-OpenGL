# Raycaster

A Wolfenstein-style pseudo-3D raycasting engine built with C++ and OpenGL, using GLFW for windowing and GLAD for OpenGL loading. Self-contained — no external dependencies beyond what's included in this repository.

![Raycaster](https://img.shields.io/badge/OpenGL-3.3-blue) ![Language](https://img.shields.io/badge/C%2B%2B-11-orange) ![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)

---

## Features

- **DDA raycasting** — fast, accurate wall detection per screen column
- **3D projection** — perpendicular-distance wall strip rendering
- **Textured walls** — PNG-based wall textures (hot-swappable at `resource/textures/`)
- **Panoramic sky** — direction-mapped sky texture wrapping the horizon
- **Textured floor** — perspective-correct floor rendering with distance fog
- **Distance-based shading / fog** — toggleable fog that darkens geometry with distance
- **WASD movement** — smooth walking, strafing, and rotation with collision detection
- **Minimap overlay** — real-time top-down view with ray visualization
- **Self-contained** — created on a portable instance of OpenGL environment. Refer to this repo
- **Visual map editor** — standalone tool to create and edit maps with click-and-drag wall placement

---

## Controls

### Raycaster

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

### Map Editor

| Input | Action |
|-------|--------|
| Left-click / drag | Place wall |
| Right-click / drag | Erase wall |
| 1-9 | Select wall type |
| G | Toggle grid overlay |
| C | Clear interior (keep perimeter) |
| Ctrl+S | Save to `resource/maps/map.txt` |
| Ctrl+Z | Undo |
| Ctrl+Y | Redo |
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

To build the map editor:

```bat
mingw64\bin\mingw32-make -f Makefile editor
build\map_editor.exe 32
```

The argument is the map side length (map will be N×N). Default is 32, max 256.

---

## Project Structure

```
src/
  main.cpp              - Entry point, game loop
  glad.c                - OpenGL loader (auto-generated)
  stb_impl.c            - STB library implementations (image loading)
  core/
    window.cpp/h        - GLFW window + GLAD init
    input.cpp/h         - Keyboard input, toggle state
  map/
    map.cpp/h           - Map loading from ASCII grid files
  player/
    player.cpp/h        - Player state, movement, collision, rotation
  renderer/
    map_renderer.cpp/h  - Raycasting renderer (walls, sky, floor, minimap)
    shader.cpp/h        - GLSL shader compilation/linking
resource/
  maps/map.txt          - ASCII map grid (0 = empty, 1-9 = wall types)
  textures/             - PNG textures (walls, floor, sky) — swap freely
tools/
  map_editor.cpp        - Visual map editor (standalone GLFW app)
  gen_textures.c        - Procedural texture generator (standalone)
include/                - GLFW, GLAD, STB, KHR headers
lib/                    - Pre-built GLFW static library (64-bit)
mingw64/                - Bundled MinGW64 compiler toolchain
build/                  - Output directory
```

---

## Textures

Textures are loaded from `resource/textures/` at startup and can be swapped without recompiling. The included `tools/gen_textures.c` can generate procedural textures:

```bat
mingw64\bin\g++ -Iinclude tools/gen_textures.c -lm -o build\gen_textures.exe
build\gen_textures.exe
```

---

## Dependencies (all bundled)

| Library | Version | Purpose |
|---------|---------|---------|
| GLFW | 3.x | Window creation, input, OpenGL context |
| GLAD | — | OpenGL 3.3 core function loader |
| STB | — | PNG image loading (`stb_image`) and writing (`stb_image_write`) |
| MinGW64 | 15.2.0 | GCC-based compiler toolchain for Windows |
