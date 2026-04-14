# Visual Fidelity Improvement Guide

Planned rendering upgrades for the GPU raycaster, organized into progressive tiers.
Each tier builds on the previous one. **Do not implement yet** — this is a planning document.

---

## Tier 1 — Foundational Lighting (Shader-Only)

These changes live entirely in the raycasting fragment shader and require no new assets.

### 1.1 Directional Sun Light
- Add a `vec3 sunDir` uniform (e.g. normalized `(0.8, -1.0, 0.6)`).
- Compute a wall-face normal from the DDA side result (N/S/E/W).
- Apply `max(dot(normal, -sunDir), 0.0)` as a diffuse factor on the wall colour.
- Multiply by a `vec3 sunColor` uniform to tint warm/cool.

### 1.2 Distance Fog (Exponential)
- After computing `perpWallDist`, blend toward a `vec3 fogColor` uniform:
  ```glsl
  float fogFactor = 1.0 - exp(-fogDensity * perpWallDist);
  color = mix(color, fogColor, fogFactor);
  ```
- Apply the same fog to floor/ceiling using their computed distances.
- `fogDensity` (~0.06–0.12) and `fogColor` go in `config.json`.

### 1.3 Gamma Correction
- Convert final colour from linear to sRGB in the last line of the fragment shader:
  ```glsl
  fragColor = vec4(pow(color.rgb, vec3(1.0 / 2.2)), 1.0);
  ```
- Ensure textures are sampled in linear space (`GL_SRGB8_ALPHA8` internal format).

### 1.4 Ambient Occlusion Approximation
- Darken wall pixels near the top/bottom edges using the texture V coordinate:
  ```glsl
  float ao = smoothstep(0.0, 0.08, uv.y) * smoothstep(0.0, 0.08, 1.0 - uv.y);
  color *= mix(0.7, 1.0, ao);
  ```

---

## Tier 2 — Enhanced Materials

### 2.1 Point Lights
- Upload an array of light positions + colours via UBO or SSBO.
- For each fragment, accumulate `attenuation * max(dot(N, L), 0.0) * lightColor`.
- Limit to ~8 lights for performance; use a radius cut-off.

### 2.2 Normal Mapping
- Ship a normal-map texture per wall tile.
- Sample the normal map in the fragment shader, transform from tangent space to world
  space using the wall face direction, and use the result for all lighting calculations.

### 2.3 Specular Highlights
- Compute a half-vector `H = normalize(L + V)` per light.
- Add `pow(max(dot(N, H), 0.0), shininess) * specStrength` to the colour.
- `shininess` and `specStrength` can vary per wall texture (store in an atlas).

### 2.4 Emissive Surfaces
- Add an emissive channel to certain wall textures (e.g. lava, screens).
- Sample emissive texture and add directly to colour (not affected by lighting).

---

## Tier 3 — Post-Processing & Advanced

### 3.1 Shadow Mapping (Directional)
- Render a depth map from the sun's point of view (orthographic).
- In the main shader, project fragments into light space and compare with the shadow map.
- Apply a bias to avoid shadow acne; use PCF for soft edges.
- Requires a second render pass and an FBO.

### 3.2 Bloom
- Render bright pixels (above a threshold) into a separate FBO attachment.
- Blur that attachment with a two-pass Gaussian kernel (ping-pong FBOs).
- Additive-blend the bloom texture onto the final image.

### 3.3 Screen-Space Ambient Occlusion (SSAO)
- Generate a depth/normal G-buffer (or reuse existing data).
- Sample hemisphere kernel around each fragment; accumulate occlusion.
- Blur the result and multiply into the final colour.

### 3.4 Volumetric Light Shafts (God Rays)
- Radial blur from the sun's screen-space position on the bright-pass buffer.
- Additive blend onto the final image.
- Very cheap for indoor scenes with limited visibility.

---

## Implementation Notes

- **Tier 1** is purely additive — existing shaders gain parameters, nothing breaks.
- **Tier 2** adds per-texture data; the wall texture atlas needs normal-map companions.
- **Tier 3** introduces FBOs and multi-pass rendering; refactor `map_renderer` to
  support render-to-texture before starting.
- All new uniforms and tunable values should go in `config.json`.
- Profile each change with a timer (`glQueryCounter`) before and after to ensure
  the frame budget stays within 16 ms at 800×600.
