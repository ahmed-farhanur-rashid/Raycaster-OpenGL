# Project Todo

> See `GUIDELINES_STATUS.md` for full CG Lab guidelines coverage analysis.

---

## Guideline Requirements — Must Do

- [ ] **Midpoint Circle algorithm** — Implement midpoint circle drawing somewhere visible (e.g. minimap player dot, a HUD element, or a demo overlay). Guidelines require at least 2 of: DDA, Bresenham, Midpoint Circle. Currently only DDA and Bresenham are done.
- [ ] **Demonstrate more 2D transformations** — Add visible scaling, reflection, or shear. Could be: scalable minimap (user-adjustable size), reflected floor, or sheared UI elements.
- [ ] **Project Report (PDF)** — Write and submit: cover page, abstract (150-200 words), introduction, scene description, algorithms used, transformations and animations, implementation details with code snippets, screenshots, EP1/EP3/EP5 mapping, challenges faced.

## High Priority — Engine Features

- [ ] **Multiple wall textures** — Map values 1-9 select `texIdx`, but all 4 slots load the same PNG. Load distinct textures per slot.
- [ ] **Mouse look** — Add `glfwSetCursorPosCallback` for smooth rotation instead of arrow keys.
- [ ] **Resizable window** — Update screenSize uniform dynamically in framebuffer callback instead of fixed 800×600.

## Medium Priority — Polish

- [ ] **Ceiling texture** — Replace panoramic sky on indoor sections with a textured ceiling (mirror floor raycasting logic).
- [ ] **Sprite system** — Billboard sprites loaded from map file (e.g. `S` characters in map.txt).
- [ ] **Audio** — Ambient sound and footstep effects via miniaudio or OpenAL.
- [ ] **Animated textures** — Cycle between texture frames for water, torches, etc.

## Stretch Goals

- [ ] **Multi-floor / height mapping** — Variable wall heights or multiple floor levels.

## Optional

- [ ] **Presentation slides** — Visual summary for project defense/viva.

---

## Completed

- [x] **GPU-side raycasting** — DDA loop, wall/floor/sky rendering, fog, and minimap all run in a single fragment shader
- [x] **DDA algorithm** — Full implementation in GLSL castRay()
- [x] **Bresenham line algorithm** — Used in map editor for brush stroke interpolation
- [x] **2D Translation** — Player movement with collision detection
- [x] **2D Rotation** — Player direction via rotation matrix
- [x] **Textured walls, floor, sky** — PNG-based texture pipeline
- [x] **Distance fog / lighting toggle** — L key toggles
- [x] **Minimap overlay** — Real-time with ray visualization
- [x] **Map editor tool** — Visual editor with undo/redo, 9 wall types, grid, Bresenham brush
- [x] **Texture editor tool** — Pixel art editor for custom textures
- [x] **Self-contained build** — Bundled MinGW64 + batch script, no external deps
