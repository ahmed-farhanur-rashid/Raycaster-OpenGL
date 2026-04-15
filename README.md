# Raycaster

A Wolfenstein-style pseudo-3D raycasting engine built with C++ and OpenGL, using GLFW for windowing and GLAD for OpenGL loading. Self-contained — no external dependencies beyond what's included in this repository.

![Raycaster](https://img.shields.io/badge/OpenGL-3.3-blue) ![Language](https://img.shields.io/badge/C%2B%2B-11-orange) ![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)

---

## Features

- **GPU-accelerated DDA raycasting** — walls, floor, and sky rendered entirely in a GLSL fragment shader
- **Textured walls** — PNG-based wall textures via a 2D texture array (hot-swappable at `resource/textures/`)
- **Panoramic sky** — direction-mapped sky texture wrapping the horizon
- **Textured floor** — perspective-correct floor rendering with distance fog
- **Distance-based shading / fog** — toggleable fog that darkens geometry with distance
- **Mouse look** — horizontal mouse aiming with configurable sensitivity
- **WASD movement** — walking, strafing, sprinting, and collision detection
- **Jumping** — vertical movement with gravity physics
- **Weapon system** — 4 weapons (assault rifle, shotgun, energy weapon, handgun) with animated sprites, recoil, and ammo management
- **Projectile system** — billboard-rendered projectiles with per-column z-buffer depth testing
- **Target entity** — procedurally generated circle sprite (midpoint circle algorithm) that respawns when hit
- **HUD** — weapon sprite with bob animation, ammo/energy bars
- **Minimap overlay** — real-time top-down view with ray visualization
- **Main menu & pause menu** — full game state system with a built-in bitmap font
- **Data-driven configuration** — JSON files for all gameplay parameters and rebindable key bindings
- **Visual map editor** — standalone tool to create and edit maps with click-and-drag wall placement
- **Self-contained** — portable OpenGL environment with bundled toolchain

---

## Controls

### Raycaster

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
| 1 / 2 / 3 / 4 | Equip assault rifle / shotgun / energy weapon / handgun |
| Left / Right arrow | Rotate (keyboard) |
| M | Toggle minimap |
| L | Toggle lighting/fog |
| ESC | Pause menu |

### Menus

| Input | Action |
|-------|--------|
| W / Up | Navigate up |
| S / Down | Navigate down |
| Enter | Confirm selection |
| ESC | Back / quit |

### Map Editor

| Input | Action |
|-------|--------|
| Left-click / drag | Place wall |
| Right-click / drag | Erase wall |
| 1-9 | Select wall type |
| E | Toggle draw / erase mode |
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

## Configuration

Gameplay parameters and key bindings are loaded from JSON files — no recompilation needed.

- **`src/settings/config.json`** — screen size, movement speeds, weapon tuning, fog density, HUD layout, and more
- **`src/settings/keybind.json`** — rebindable controls using GLFW key names (`W`, `SPACE`, `LEFT_SHIFT`, etc.)

---

## Project Structure

```
src/
  main.cpp                      - Entry point, game loop, state dispatch
  glad.c                        - OpenGL loader (auto-generated)
  stb_impl.c                    - STB library implementations
  core/
    window.cpp/h                - GLFW window + GLAD init
    input.cpp/h                 - Keyboard/mouse input, weapon switching, projectile spawning
    game_state.cpp/h            - Game state machine (MAIN_MENU, PLAYING, PAUSED)
  map/
    map.cpp/h                   - Map loading from ASCII grid files
  player/
    player.cpp/h                - Player state, movement, collision, rotation, jumping
  renderer/
    map_renderer.cpp/h          - GPU texture setup, per-frame uniforms, full-screen quad
    world_shaders.cpp/h         - GLSL vertex + fragment shaders (DDA raycasting on GPU)
    minimap_renderer.cpp/h      - CPU-rasterized minimap overlay
    hud_renderer.cpp/h          - Weapon sprite display, bob animation, ammo bars
    projectile_renderer.cpp/h   - Billboard projectile rendering with z-buffer
    menu_renderer.cpp/h         - Main menu and pause menu with built-in bitmap font
    shader.cpp/h                - GLSL shader compilation/linking utility
  weapons/
    weapon.cpp/h                - Weapon struct, shared logic, factory declarations
    assault_rifle.cpp           - Auto-fire rifle with grenade alt-fire
    shotgun.cpp                 - Semi-auto with pump reload
    energy_weapon.cpp           - Hold-to-fire beam with charge/drain/overheat
    handgun.cpp                 - Unlimited semi-auto sidearm
  projectile/
    projectile.cpp/h            - Projectile spawning, movement, wall collision
  circular_sprite/
    circular_sprite.cpp/h       - Target entity (midpoint circle algorithm billboard)
  settings/
    settings.cpp/h              - JSON config + keybind loader
    config.json                 - Gameplay parameters
    keybind.json                - Key bindings
resource/
  maps/map.txt                  - ASCII map grid (0 = empty, 1-9 = wall types)
  textures/                     - PNG textures (walls, floor, sky, weapons, entities)
  audio/                        - Reserved for future audio assets
tools/
  map_editor.cpp                - Visual map editor (standalone GLFW app)
include/                        - GLFW, GLAD, STB, KHR headers
lib/                            - Pre-built GLFW static library (64-bit)
mingw64/                        - Bundled MinGW64 compiler toolchain
build/                          - Output directory
```

---

## Dependencies (all bundled)

| Library | Version | Purpose |
|---------|---------|---------|
| GLFW | 3.x | Window creation, input, OpenGL context |
| GLAD | — | OpenGL 3.3 core function loader |
| STB | — | PNG image loading (`stb_image`) and writing (`stb_image_write`) |
| MinGW64 | 15.2.0 | GCC-based compiler toolchain for Windows |
