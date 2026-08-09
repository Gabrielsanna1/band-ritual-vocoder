# Band Ritual GUI — Blender project + State Aligner, session 2026-04-20/21

## Blender project
- File: `/Users/ricosan/Desktop/band_ritual_v1.blend`
- Camera: `BR_KnobRenderCam` (ortho scale 0.0467) centred on Shape_17 — use this cam + hide all other meshes + hide lights + `film_transparent=True` to render the tiny `_circle_ind` pointer asset.
- Dedicated cam: `GigaKnobRenderCam` at (-0.319, 0.013, 0.3), ortho_scale 0.12 → frames the big MIX knob (`GigaKnob_Body/Bezel/DetailRing/Well`).
- Knob materials:
  - **Small knobs** (Shape_17..30_circle*): `BR_KnobTop` (image texture `knob_body_cream_texture.png` on cream body) + `BR_KnobSkirt` (brushed metal, anisotropic 0.7, bump, tangent).
  - **MIX knob** (GigaKnob_*): all 4 parts share `BR_KnobTop_Textured` — Principled BSDF with the cream texture plugged into BOTH Base Color AND Emission (strength 0.85) + BOX projection, so lights can't wash it grey. Last render (512,512) sampled (236, 215, 189) = proper cream.
- `Shape_*_circle_ind` objects have a small bevel on top-end edges (offset 0.18 mm / 3 seg, plus a 0.35 mm / 4 seg pass on Shape_17). Material `BR_KnobIndicator` exists but is NO LONGER USED by the state-aligner — the indicator is drawn procedurally in JS (see below).

## State aligner (webapp)
- Repo path: `/Users/ricosan/eqenrico-hub.github.io/agents/png-state-aligner/`
- Live: https://eqenrico-hub.github.io/agents/png-state-aligner/
- Key sample_data files: `png_a/b/c.png`, `knob_render.png`, `gigaknob_render.png` (MIX knob body), `slider_handle_native.png`, `decal_45.png` (COMPLEMENT glow), `decal_46.png` (ChatGPT decal), `clone_overlay*.png`, `default_manifest.json`.
- MIX knob region name: `knob_mix` at x=73 y=187 w=200 h=200 with `knob_image_file: gigaknob_render.png`. Rotates in interactive mode exactly like the small knobs (vertical drag, 200 image px = full 0..1 range, angle = −135°+270°·value).
- Indicator = procedural canvas path (rounded-top rectangle via `arcTo` on both top corners, radius `indW*0.18`, clamped ≤ indW/2 & indH/2). Colour `rgb(217, 79, 108)` flat. Pass 1 = SE world-space drop shadow (blur + 0.42 black) so the indicator reads as raised.
- Knob body shadow: `ctx.shadowColor rgba(0,0,0,0.50)`, offsetX `rw * 0.075`, offsetY `rh * 0.180`, blur `rw * 0.120`. Applied to body only; indicator draws outside the shadow block.
- decal_45 parent-linked to `region_42` (COMPLEMENT button rect 1267,586,146,34); appears only when region_42 is pressed.
- decal_46 = always-visible ChatGPT image at (1,674,337,225).

## Critical patterns (don't rebuild, don't fight)
1. **Downscale kills Blender bevels.** Any rounding smaller than ~5% of the source image is invisible after 1024→72 downscale. Do rounding in JS as a fraction of the final rect size, not in Blender geometry.
2. **Integer-pixel snap for rotation.** `Math.round(rx + rw/2)` + integer w/h before `ctx.rotate` — otherwise fractional state.scale positions make each knob sample differently and AA produces different thicknesses.
3. **Cache-bust sample_data PNGs.** GitHub Pages + Safari aggressively cache PNGs. For every runtime fetch append `?v=' + Date.now()`. (Hotfix history: the indicator PNG and the knob PNG both needed this.)
4. **Decal parent_name must resolve in the second import pass.** Decals in the manifest store `parent_name`; the importer copies it to `_pendingParentName` then the second pass rewrites it to `parentId`. Without this decals never show when their parent button is pressed.
5. **Drop-drag image naming.** On drop, new decal name = max existing `decal_N` + 1 (never `regions.length + 1` — collides and corrupts `crop_file` on export).
6. **rect_4 handle lives as `handle_file: slider_handle_native.png`** — always preserve this override on every bundle merge.

## Resume next session
- User trigger: *"resume png-state-aligner"* or *"resume Band Ritual hybrid GUI"*.
- Open: https://eqenrico-hub.github.io/agents/png-state-aligner/?v=<N+1>&r=<timestamp>
- Blender: open `band_ritual_v1.blend`, active cams are set up and ready.
- Likely next: filmstrip export (128 frames min→max) for VST3 JUCE `FilmstripLookAndFeel` (small knobs at 70/100/180 px and MIX at whatever final size ships).
