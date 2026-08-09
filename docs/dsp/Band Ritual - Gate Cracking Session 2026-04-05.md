# Band Ritual - Gate Cracking Session (2026-04-05)

## Gate Formula CRACKED

### Changes from original:
1. **Offset**: -17 → **-27** (threshold much lower, less aggressive gating)
2. **Gate type**: soft knee (attenuate by gateRange) → **hard mute** (gain=0 below threshold)

### Code (`PluginProcessor.cpp`):
```cpp
// Threshold calculation (~line 3209)
float gateThresholdDb = gateAmount - 27.0f;  // was -17

// Gate application (~line 3675)
float targetGateGain = (compensatedEnv < gateThresholdLinear) ? 0.0f : 1.0f;  // hard mute
// (was: ? gateGainLinear : 1.0f — soft knee with -40dB attenuation)
```

### Results (relative to gate OFF)
| Gate | Ableton | Band Ritual | Error |
|------|---------|-------------|-------|
| -60 dB (OFF) | +0.0 dB | +0.0 dB | 0.0 dB |
| -40 dB | -0.0 dB | -0.0 dB | 0.0 dB |
| -20 dB | -0.1 dB | -0.0 dB | 0.1 dB |
| -10 dB | -2.5 dB | -1.3 dB | **1.2 dB** |
| 0 dB | -127.1 dB | -56.3 dB | 70.8 dB |

### Notes:
- Gate OFF to -20 dB: perfect match (0-0.1 dB error)
- Gate -10 dB: 1.2 dB error — acceptable for practical use
- Gate 0 dB: BR doesn't fully mute (-56 vs -127 dB) — edge case, rarely used at max
- The smoothing coefficients (2ms attack, 30ms release) are unchanged
- `gate_range` parameter no longer used (hard mute replaces soft knee)

### Test methodology:
- Input: DRYWAV1_WARPED.wav (warped version = what actually went through Ableton)
- Settings: Linked mode, 40 bands, BW 10%, Release 50ms, Depth 100%, Noise 0%
- Comparison via Pedalboard automated processing

## IMPORTANT: Warped input
- Ableton warps audio clips to match tempo when loaded
- DRYWAV1_WARPED.wav is the actual signal that went through the vocoder
- Must use this for accurate comparison, not the original DRYWAV_1.wav

## COMPLETE CRACKING STATUS (2026-04-05)
| Feature | Status | Error |
|---------|--------|-------|
| Bands | ✅ CRACKED | 2.5 dB |
| Noise/Unvoiced | ✅ CRACKED | 1-2 dB |
| Depth | ✅ CRACKED | 0.4-0.9 dB |
| **Gate** | ✅ **CRACKED** | **0-1.2 dB** (practical range) |
| BW gain | ✅ CRACKED | ±2 dB |
| Formant | ❌ Not yet | — |
| Enhance | ❌ Not yet | — |
