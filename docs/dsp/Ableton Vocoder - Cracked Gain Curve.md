---
tags:
  - band-ritual
  - vocoder
  - ableton
  - cracked
  - gain-curve
date: '2026-03-19'
---
# Ableton Vocoder — CRACKED: Gain Curve (2026-03-19)

## THE KEY DISCOVERY

**Ableton's vocoder has NO envelope compression.** Output is purely quadratic:

```
output ∝ input²  (12 dB output per 6 dB input, ratio = 2.00)
```

This means: `output = carrier_filtered × envelope` where both scale linearly with input.
In self-carrier mode: carrier ∝ input, envelope ∝ input → output ∝ input².

### Measured Data (4 bands, LOWBW, 1000 Hz sine)

| Input (dBFS) | Output (dBFS) | Gain (dB) | Step ratio |
|---|---|---|---|
| -63 | -600 (zero) | — | — |
| -57 | -600 (zero) | — | — |
| -51 | -600 (zero) | — | — |
| -45 | -600 (zero) | — | — |
| -39 | -600 (zero) | — | — |
| -33 | -118.5 | -85.5 | (gate opens) |
| -27 | -107.5 | -80.5 | 1.83 |
| -21 | -95.6 | -74.6 | 1.99 |
| -15 | -83.6 | -68.6 | 2.00 |
| -9 | -71.6 | -62.6 | 2.00 |
| -3 | -59.6 | -56.6 | 2.00 |

**Step ratio = 2.00 consistently from -27 to -3 dBFS** = exactly quadratic.

### Gate Threshold
Below ~-33 dBFS input, output is zero. This could be:
- Ableton's internal gate (set to OFF but may have a minimum floor)
- 32-bit float noise floor
- The envelope at 4 bands LOWBW being below detection threshold

### What This Means for V3

1. **REMOVE fastTanh** — Ableton does NOT use it
2. **Use linear envelope**: `envGain = envelope` (no shaping function)
3. **The quadratic response is NATURAL** — it comes from multiplying carrier × envelope, both proportional to input
4. **No tonal/noise detection needed** — linear multiplication handles both identically
5. The depth parameter likely just scales the envelope: `envGain = (1-depth) + depth * envelope`

### Why V2's fastTanh Seemed to Work
fastTanh compressed loud signals, which accidentally compensated for the fact that our normalization was wrong. With correct normalization AND linear envelope, the levels should match without compression.
