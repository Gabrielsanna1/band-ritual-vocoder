# Band Ritual MIX Knob — Blender Session 2026-04-26 / 27

End-of-session context for the MKV2_* MIX knob build in `band_ritual_v1.blend`.

## Final approved values (user-locked)

- **Indicator angle @ 0%**: `243.02°`
- **Indicator angle @ 100%**: `-35.77°`
- **Tip radius %** (radial offset of indicator pill): `83.81%`
- LED order: **LED_1_B first to light**, LED_5_R last (was reversed earlier — fixed)
- LED_W (white): always on

These live on `mkv2_indicator` PropertyGroup with update callbacks → drives N-panel sliders.

## Files

- **Main scene**: `/Users/ricosan/Desktop/band_ritual_v1.blend`
- **Vintage locked snapshot**: `/Users/ricosan/Desktop/band_ritual_v1_VINTAGE_LOCKED.blend`
- **Pre-LED-inset rollback point**: `/Users/ricosan/Desktop/band_ritual_v1_PRE_LED_INSET_2026_04_27.blend`
- **Filmstrip output**: `/tmp/MKV2_knob_filmstrip_128.png` (256×32768, 128 frames, full 270° sweep)
- **GIF preview**: `/tmp/MKV2_knob_animation.gif`
- **Static endpoints**: `/tmp/MKV2_knob_at_0.png`, `/tmp/MKV2_knob_at_100.png`
- **PBR textures**:
  - `/tmp/knob_textures/wp02_color.png` + `wp02_normal.png` + `wp02_rough.png` — White Plaster 02 (PolyHaven, dome top)
  - `/tmp/knob_textures/metal038/Metal038_1K-PNG_*.png` — Color/Roughness/NormalGL/Metalness (column + bezel)
- **Locked material**: `BR_PaintedVintage_LOCKED_2026_04_26` (fake user, vintage GUI look)

## MKV2 N-panel (in Blender, press N)

`MKV2_PT_indicator` panel:
- Tip Position slider
- Angle @ 0% / Angle @ 100% sliders (with auto-snap test_value preview: edits to angle@100 set test=1.0; edits to angle@0 set test=0.0)
- Test Value preview slider
- LED Ramp Width / LED Max Strength / LED_W Strength sliders
- 6 LED color swatches with hex paste icons + per-LED eyedroppers
- "🎯 Click 5 points on screen" button → `MKV2_OT_5color_picker` modal operator

Tint Amount slider on dome material is a PropertyGroup with update callback (driver was unreliable — replaced).

## 5-color picker tool — BROKEN, use online picker instead

`/tmp/mkv2_color_picker_tool.py` opens fullscreen tkinter window but displays empty/black even after granting Screen Recording permission. Likely cause: `screencapture -x` returns black frame because permission cache hasn't refreshed for `/usr/bin/python3`, or PIL display sizing wrong on Retina.

**Workaround user chose**: pick colors from any online color picker / browser DevTools and paste hex codes directly into the LED color swatch fields in the N-panel (each has a hex paste icon). No need to fix the tkinter tool.

## Pending work

1. **LED inset**: LEDs should cast light on `MKV2_OuterBezel` AND appear recessed into the metal (not just overlay objects). Checkpoint already saved at `band_ritual_v1_PRE_LED_INSET_2026_04_27.blend`.
2. **Re-render filmstrip** with final LED colors once user picks them.
3. **Webapp integration**: replace `/Users/ricosan/eqenrico-hub.github.io/agents/png-state-aligner/sample_data/gigaknob_render.png` with new filmstrip (or add filmstrip alongside).

## Critical lessons from this session

- **macOS osascript color picker freezes Blender** when called via `subprocess.run` — must use `Popen` + modal operator polling at 0.3s timer.
- **Drivers don't refresh live in shader nodes** for slider control — use PropertyGroup with update callback that directly sets node values + tags for redraw.
- **DiscTop dome distortion** with planar UV → fixed with **BOX (triplanar) projection** on Image Texture.
- **Two materials sharing same color** → flat papier-mâché look. Solved with darker bezel variant + AO + rim light.
- **Mesh-level imperfections** (user explicitly asked NOT for texture fakery): use Displace modifier with STUCCI noise.
- **Fillet between disc and knob**: bevel weight markers on bottom-outer edge of disc + top-inner edge of knob, then 1.4mm bevel via WEIGHT limit.
- **Indicator parenting pivot bug**: `ind.location.y` was `0.0322` instead of `0.0173` → fix via parenting + `matrix_parent_inverse = Identity`.
- **DSP lesson confirmed (CLAUDE.md)**: cuts work but boosts don't = you're trying to detect frequency from time-domain samples. Apply EQ before mixing instead.

## How to resume

User trigger: *"resume MIX knob LED inset"* or *"resume Band Ritual MIX knob"*.
1. Open `/Users/ricosan/Desktop/band_ritual_v1.blend` (or rollback to `_PRE_LED_INSET_2026_04_27.blend` if needed).
2. Press N in 3D viewport → MKV2 panel.
3. For LED color update: paste hex codes into the 6 swatch fields manually.
4. For filmstrip re-export: use the cam + script that produced `/tmp/MKV2_knob_filmstrip_128.png`.
