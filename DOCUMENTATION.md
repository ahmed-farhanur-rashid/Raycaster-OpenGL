# Raycaster-OpenGL — Project Documentation

A complete guide to understanding and rebuilding this project from scratch, written at three levels of detail.

---

# Part 1: High-Level Overview

## What Is This?

A **Wolfenstein 3D-style raycasting engine** — a program that renders a 2D grid map as a pseudo-3D first-person view in real time. The player walks through a maze of textured walls with a sky above and a floor below.

## How Does It Work?

1. A **2D map** is stored as a grid of numbers (0 = empty, 1-9 = wall types)
2. For each vertical column of the screen, a **ray** is cast from the player's position into the map
3. When a ray hits a wall, the engine calculates the **distance** to that wall
4. The distance determines **how tall** the wall strip is drawn on screen (closer = taller)
5. **Textures** are sampled from PNG images and mapped onto walls, floor, and sky
6. A **minimap** overlay shows the top-down view with ray lines
7. The player can **move** (WASD) and **rotate** (arrow keys) with collision detection

## What Technologies Are Used?

| Technology | Role |
|------------|------|
| **C++** | Main language for all application code |
| **OpenGL 3.3** | GPU rendering API |
| **GLSL** | Shader language — all raycasting runs on the GPU |
| **GLFW** | Window creation, input handling, OpenGL context |
| **GLAD** | Loads OpenGL function pointers |
| **STB** | PNG image loading/writing |
| **MinGW64** | GCC compiler toolchain for Windows |

## What Are the Main Parts?

```
[main.cpp] ──→ creates window ──→ loads map ──→ enters game loop
                                                    │
                ┌───────────────────────────────────┘
                ▼
        ┌──────────────┐     ┌──────────────┐     ┌──────────────┐
        │  input.cpp   │     │  player.cpp  │     │ map_renderer │
        │  reads keys  │────→│  move/rotate │────→│  GPU shader  │
        └──────────────┘     └──────────────┘     │  does DDA    │
                                                  │  draws scene │
                                                  └──────────────┘
```

## Project Structure

```
src/                  ← All engine source code
  main.cpp            ← Entry point and game loop
  core/               ← Window init and keyboard input
  map/                ← Map data loading from text file
  player/             ← Player position, movement, collision
  renderer/           ← GPU raycasting shader + texture loading
resource/             ← Runtime assets (map file, PNG textures)
tools/                ← Standalone editor utilities
include/              ← Third-party headers (GLFW, GLAD, STB)
lib/                  ← Pre-built GLFW library
mingw64/              ← Bundled compiler toolchain
```

---

# Part 2: Medium-Level Explanation

## The Game Loop

The engine follows a standard real-time rendering loop:

```
while (window is open) {
    1. Calculate delta time (time since last frame)
    2. Read keyboard input → move/rotate player
    3. Send player state to GPU as shader uniforms
    4. GPU runs fragment shader on every pixel:
       - Sky pixels: sample panoramic sky texture
       - Wall pixels: DDA raycast → find wall → sample wall texture
       - Floor pixels: perspective floor mapping → sample floor texture
       - Minimap pixels: draw top-down grid overlay
    5. Swap buffers (display the frame)
}
```

## Module Breakdown

### `main.cpp` — Entry Point
- Creates an 800×600 GLFW window with OpenGL 3.3 core context
- Loads the map from `resource/maps/map.txt`
- Initializes the player at position (1.5, 1.5) facing east
- Runs the game loop with a 0.05s frame time cap

### `core/window.cpp` — Window Setup
- Configures GLFW for OpenGL 3.3 core profile
- Creates the window and sets the framebuffer resize callback
- Initializes GLAD to load OpenGL function pointers

### `core/input.cpp` — Keyboard Input
- WASD / arrow keys for movement and rotation
- M toggles the minimap overlay
- L toggles distance fog/lighting
- ESC closes the window
- Uses toggle debouncing (tracks previous key state)

### `map/map.cpp` — Map Loading
- Reads a plain text file where each character is a cell: `0` = empty, `1`-`9` = wall type
- Auto-sizes to fit content (max 256×256)
- Stores data in `worldMap[256][256]` global array

### `player/player.cpp` — Player State
- Stores position `(posX, posY)`, direction vector `(dirX, dirY)`, and camera plane `(planeX, planeY)`
- `movePlayer()` applies translation with per-axis collision detection (checks map grid at new position ± margin)
- `rotatePlayer()` applies a 2D rotation matrix to both the direction and camera plane vectors

### `renderer/map_renderer.cpp` — The Renderer
This is the core of the engine. It:

1. **At init**: Compiles a GLSL shader program, uploads the map grid as an integer texture, loads wall/floor/sky PNG textures, creates a full-screen quad
2. **Per frame**: Sets uniforms (player pos, dir, plane, screen size, toggles), binds textures, draws the quad
3. **In the shader**: Every pixel runs the DDA raycast algorithm to determine what to draw

### `renderer/shader.cpp` — Shader Utilities
- `compileShader()` compiles vertex or fragment shader source, logs errors
- `createShaderProgram()` links a vertex + fragment shader into a program

## The DDA Raycasting Algorithm

For each screen column:

1. Compute the **ray direction** from the player's direction + camera plane offset
2. Initialize the DDA: calculate step direction and initial side distances for X and Y grid lines
3. **Step through the grid**: always advance to the nearest grid line (X or Y)
4. When a non-zero cell is hit, compute the **perpendicular distance** to avoid fisheye distortion
5. The wall strip height = `screenHeight / perpDistance`
6. Sample the wall texture at the correct U,V coordinate

## Texture Pipeline

All textures are loaded from `resource/textures/` as PNG files at startup:
- **4 wall textures** → stored in a `GL_TEXTURE_2D_ARRAY` (one layer per wall type)
- **1 floor texture** → `GL_TEXTURE_2D` with `GL_REPEAT` wrapping
- **1 sky texture** → `GL_TEXTURE_2D`, mapped as a panorama based on ray angle

Textures can be swapped by replacing the PNG files — no recompilation needed.

## Tools

### Map Editor (`tools/map_editor.cpp`)
- Standalone GLFW application for creating/editing maps
- Click to draw walls, right-click to erase, keys 1-9 for wall types
- Bresenham line algorithm for smooth brush strokes between mouse positions
- Undo/redo stack (256 steps), grid overlay, save to `resource/maps/map.txt`

### Texture Editor (`tools/texture_editor.cpp`)
- Pixel art editor for creating textures
- RGB color picker via keyboard + scroll wheel
- Undo/redo, fill, clear, save to PNG

---

# Part 3: Low-Level Implementation Guide

## How to Build This From Scratch

### Prerequisites

- A C++ compiler (GCC/MinGW recommended — bundled in this repo)
- Basic understanding of C++, OpenGL concepts, and linear algebra

### Step 1: Set Up the Development Environment

1. **Get MinGW64** — download or use the bundled `mingw64/` directory
2. **Get GLFW** — download the pre-compiled library and headers, or use the bundled `lib/` and `include/GLFW/`
3. **Get GLAD** — go to https://glad.dav1d.de, select OpenGL 3.3 Core, generate, and place `glad.h` in `include/glad/` and `glad.c` in `src/`
4. **Get STB** — download `stb_image.h` and `stb_image_write.h` into `include/stb/`
5. **Create the directory structure**:
   ```
   project/
     include/glad/ include/GLFW/ include/KHR/ include/stb/
     lib/
     src/ src/core/ src/map/ src/player/ src/renderer/
     resource/maps/ resource/textures/
     tools/
     build/
   ```

### Step 2: Create the Window (`src/core/window.cpp`)

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>

void initGLFW() {
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        exit(EXIT_FAILURE);
    }
    // Request OpenGL 3.3 core profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
}

GLFWwindow* createWindow(int w, int h, const char* title) {
    GLFWwindow* win = glfwCreateWindow(w, h, title, NULL, NULL);
    if (!win) {
        fprintf(stderr, "Failed to create window\n");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    glfwMakeContextCurrent(win);
    glfwSetFramebufferSizeCallback(win, framebufferSizeCallback);
    return win;
}

void initGLAD() {
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        fprintf(stderr, "Failed to initialize GLAD\n");
        exit(EXIT_FAILURE);
    }
}

void framebufferSizeCallback(GLFWwindow* w, int width, int height) {
    glViewport(0, 0, width, height);
}
```

**What this does**: Initializes GLFW, creates a window with an OpenGL 3.3 context, and loads all OpenGL function pointers through GLAD.

### Step 3: Build the Map Loader (`src/map/map.cpp`)

The map is a plain text file where each character represents a grid cell:

```
111111111111
100000000001
103000000201
100000000001
100000400001
111111111111
```

- `0` = empty space (player can walk here)
- `1`-`9` = wall of that type (determines which texture to use)

```cpp
#include <fstream>
#include <string>

#define MAX_MAP_DIM 256
int mapWidth = 0, mapHeight = 0;
int worldMap[MAX_MAP_DIM][MAX_MAP_DIM];

bool loadMap(const char* path) {
    std::ifstream f(path);
    if (!f) return false;
    int row = 0;
    std::string line;
    while (std::getline(f, line) && row < MAX_MAP_DIM) {
        int cols = (int)line.size();
        if (cols > mapWidth) mapWidth = cols;
        for (int x = 0; x < cols; x++)
            worldMap[row][x] = (line[x] >= '0' && line[x] <= '9') ? line[x] - '0' : 0;
        row++;
    }
    mapHeight = row;
    return true;
}
```

### Step 4: Implement the Player (`src/player/player.cpp`)

The player has:
- **Position**: `(posX, posY)` — floating point coordinates in map space
- **Direction**: `(dirX, dirY)` — unit vector pointing where the player faces
- **Camera plane**: `(planeX, planeY)` — perpendicular to direction, defines FOV

**Movement (Translation)**:
```cpp
void movePlayer(float forward, float strafe, float dt) {
    float speed = moveSpeed * dt;
    // Calculate right vector (perpendicular to direction)
    float rightX = -dirY, rightY = dirX;
    // Combined movement vector
    float moveX = dirX * forward + rightX * strafe;
    float moveY = dirY * forward + rightY * strafe;
    // Apply with per-axis collision detection
    float newX = posX + moveX * speed;
    float newY = posY + moveY * speed;
    float margin = 0.2f;
    // Check X axis independently
    int checkX = (int)(newX + (moveX > 0 ? margin : -margin));
    if (worldMap[(int)posY][checkX] == 0) posX = newX;
    // Check Y axis independently
    int checkY = (int)(newY + (moveY > 0 ? margin : -margin));
    if (worldMap[checkY][(int)posX] == 0) posY = newY;
}
```

**Rotation (2D rotation matrix)**:
```cpp
void rotatePlayer(float angle) {
    float c = cos(angle), s = sin(angle);
    // Rotate direction vector
    float oldDirX = dirX;
    dirX = dirX * c - dirY * s;
    dirY = oldDirX * s + dirY * c;
    // Rotate camera plane (must stay perpendicular to dir)
    float oldPlaneX = planeX;
    planeX = planeX * c - planeY * s;
    planeY = oldPlaneX * s + planeY * c;
}
```

This is the standard 2D rotation formula: $x' = x \cos\theta - y \sin\theta$, $y' = x \sin\theta + y \cos\theta$

### Step 5: Create the Shader Compiler (`src/renderer/shader.cpp`)

```cpp
unsigned int compileShader(unsigned int type, const char* src) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    // Check for errors
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, NULL, log);
        fprintf(stderr, "Shader error: %s\n", log);
        exit(EXIT_FAILURE);
    }
    return shader;
}

unsigned int createShaderProgram(const char* vertSrc, const char* fragSrc) {
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertSrc);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}
```

### Step 6: Build the Renderer (`src/renderer/map_renderer.cpp`)

This is the largest and most important file. The approach:

#### 6a. The Rendering Strategy

Instead of raycasting on the CPU and uploading a pixel buffer, we render a **full-screen quad** and run the raycasting algorithm **in the fragment shader**. Every pixel runs the DDA algorithm independently on the GPU.

#### 6b. Vertex Shader

Trivial — just passes through a full-screen quad with UV coordinates:

```glsl
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
out vec2 uv;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    uv = aUV;
}
```

#### 6c. Fragment Shader — The DDA Algorithm

The core raycasting logic runs per-pixel in GLSL:

```glsl
// For each pixel, determine its screen column
float camX = 2.0 * pixelX / screenWidth - 1.0;  // -1 (left) to +1 (right)
vec2 rayDir = playerDir + playerPlane * camX;     // ray direction for this column
```

**DDA initialization:**
```glsl
int mapX = int(playerPos.x), mapY = int(playerPos.y);
// Delta distances: how far the ray must travel to cross one grid line
float ddX = abs(1.0 / rayDir.x);  // distance per X step
float ddY = abs(1.0 / rayDir.y);  // distance per Y step
// Initial side distances: distance from player to first X/Y grid line
// Depends on which direction the ray is going
```

**DDA loop:**
```glsl
for (int i = 0; i < MAX_STEPS; i++) {
    // Always step to the NEAREST grid line
    if (sideDistX < sideDistY) {
        sideDistX += ddX;   // advance to next vertical grid line
        mapX += stepX;       // move one cell in X
        side = 0;            // hit a vertical wall (NS face)
    } else {
        sideDistY += ddY;   // advance to next horizontal grid line
        mapY += stepY;       // move one cell in Y
        side = 1;            // hit a horizontal wall (EW face)
    }
    // Check if this cell is a wall
    if (getMap(mapX, mapY) > 0) {
        // Calculate perpendicular distance (avoids fisheye)
        // ... hit!
        break;
    }
}
```

**Perpendicular distance** (critical for correct projection):
```glsl
if (side == 0)
    perpDist = (mapX - playerPos.x + (1 - stepX) * 0.5) / rayDir.x;
else
    perpDist = (mapY - playerPos.y + (1 - stepY) * 0.5) / rayDir.y;
```

**Wall height and texture mapping:**
```glsl
int lineHeight = int(screenHeight / perpDist);
int drawStart = -lineHeight / 2 + screenHeight / 2;
int drawEnd = lineHeight / 2 + screenHeight / 2;

// Texture U coordinate: where on the wall face the ray hit
float wallX = (side == 0)
    ? playerPos.y + perpDist * rayDir.y
    : playerPos.x + perpDist * rayDir.x;
wallX = fract(wallX);  // 0.0 to 1.0

// Texture V coordinate: vertical position within the wall strip
float texV = (pixelY - drawStart) / lineHeight;  // 0.0 to 1.0
```

#### 6d. Floor Rendering

For pixels below the wall strip:
```glsl
int p = pixelY - screenHeight / 2;  // distance from center
float rowDistance = (0.5 * screenHeight) / p;  // perspective distance
vec2 floorPos = playerPos + rowDistance * rayDir;  // world position
color = texture(floorTex, fract(floorPos));  // sample with wrapping
```

#### 6e. Sky Rendering

For pixels above the wall strip:
```glsl
float angle = atan(rayDir.y, rayDir.x);     // horizontal angle
float u = (angle + PI) / (2.0 * PI);        // 0..1 wrapping panorama
float v = pixelY / (screenHeight / 2.0);    // 0 at top, 1 at horizon
color = texture(skyTex, vec2(u, v));
```

#### 6f. Distance Fog

Applied to walls and floor when lighting is enabled:
```glsl
float shade = 1.0 / (1.0 + distance * distance * 0.04);
color *= shade;
```

#### 6g. Minimap

A 160×160 pixel overlay at the top-left. For each pixel in the minimap region:
- Map it to world coordinates
- If the world cell is a wall → gray
- If close to a sample ray line → green
- If close to the player position → red dot
- If along the player direction → yellow line

#### 6h. Texture Upload

At initialization, textures are loaded from PNG and uploaded to OpenGL:

- **Map grid** → `GL_TEXTURE_2D` with `GL_R8UI` format (integer texture, one byte per cell)
- **Wall textures** → `GL_TEXTURE_2D_ARRAY` with 4 layers (one per wall type)
- **Floor texture** → `GL_TEXTURE_2D` with `GL_REPEAT` wrapping
- **Sky texture** → `GL_TEXTURE_2D` with horizontal repeat, vertical clamp

Each texture is bound to a fixed texture unit (0-3) and accessed in the shader via `uniform sampler2D`/`usampler2D`/`sampler2DArray`.

#### 6i. Per-Frame Rendering

Every frame:
1. Set uniforms: `playerPos`, `playerDir`, `playerPlane`, `screenSize`, `mapSize`, `lightingEnabled`, `minimapEnabled`
2. Bind all textures to their units
3. Draw the full-screen quad (4 vertices, triangle strip)
4. The GPU runs the fragment shader on every pixel in parallel

### Step 7: Wire Everything Together (`src/main.cpp`)

```cpp
int main() {
    initGLFW();
    GLFWwindow* window = createWindow(800, 600, "Raycaster");
    initGLAD();
    glViewport(0, 0, 800, 600);

    loadMap("resource/maps/map.txt");
    initPlayer();
    initRenderer(800, 600);

    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;
        if (dt > 0.05f) dt = 0.05f;  // cap delta time

        processInput(window, dt);
        renderFrame();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    cleanupRenderer();
    glfwTerminate();
    return 0;
}
```

### Step 8: Create the Build System

**Makefile:**
```makefile
CC       = g++
INCLUDES = -Iinclude
LIBS     = -Llib -lglfw3 -lopengl32 -lgdi32 -lwinmm -luser32 \
           -static-libgcc -static-libstdc++
SOURCES  = src/*.cpp src/*.c src/core/*.cpp src/renderer/*.cpp \
           src/map/*.cpp src/player/*.cpp

all: build/main.exe

build/main.exe: $(SOURCES)
	$(CC) $(INCLUDES) $(SOURCES) $(LIBS) -o build/main.exe
```

**build.bat** (Windows convenience script):
```bat
@echo off
cd /d "%~dp0"
set PATH=%cd%\mingw64\bin;%PATH%
mingw32-make -f Makefile
if not errorlevel 1 build\main.exe
```

### Step 9: Add Textures

Place PNG files in `resource/textures/`:
- Wall texture: referenced in `map_renderer.cpp` loader
- Floor texture: tiling ground texture
- Sky texture: wide panoramic sky image

The renderer loads specific filenames at startup. Modify the paths in `initRenderer()` to match your files.

### Step 10: Create a Map

Edit `resource/maps/map.txt` — or use the map editor tool:

```
1111111111
1000000001
1030000201
1000000001
1000004001
1111111111
```

Surround the map with walls (the perimeter should be non-zero) to prevent the player from walking off the edge.

---

## Key Algorithms Reference

### DDA (Digital Differential Analyzer)

Used for raycasting. Steps through a 2D grid along a ray direction, always choosing the nearest grid line crossing. Time complexity: O(n) where n is the number of grid cells the ray crosses.

### Bresenham's Line Algorithm

Used in the map editor for drawing continuous brush strokes between discrete mouse positions. Determines which grid cells a line between two points passes through using only integer arithmetic.

### 2D Rotation Matrix

Used for player rotation:

$$\begin{bmatrix} x' \\ y' \end{bmatrix} = \begin{bmatrix} \cos\theta & -\sin\theta \\ \sin\theta & \cos\theta \end{bmatrix} \begin{bmatrix} x \\ y \end{bmatrix}$$

Applied to both the direction vector and the camera plane vector to maintain their perpendicular relationship.

### Perspective Projection

Wall height on screen is inversely proportional to perpendicular distance:

$$\text{lineHeight} = \frac{\text{screenHeight}}{\text{perpDistance}}$$

**Perpendicular distance** (not Euclidean distance) is used to avoid the fisheye effect that would otherwise distort walls at the screen edges.

### Floor Casting

For a pixel at vertical position $p$ below the horizon:

$$\text{rowDistance} = \frac{0.5 \times \text{screenHeight}}{p}$$

This gives the world-space distance for that floor row, which is used to compute the texture coordinate.
