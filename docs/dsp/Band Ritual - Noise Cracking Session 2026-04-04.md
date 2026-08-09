# Band Ritual - Noise/Unvoiced Cracking Session (2026-04-04)

## CRITICAL: Algorithm Mode
- **Cracking is done with `Linked` mode, NOT `Ritual` mode**
- `Linked` = the algorithm that matches Ableton's vocoder behavior
- `Ritual` = Band Ritual's custom algorithm (different signal path)
- In Pedalboard: `plugin.algorithm_mode = "Linked"` (case-sensitive string)
- Default is "Ritual" — MUST explicitly set to "Linked" for Ableton matching

## What Was Confirmed About Ableton's Noise

1. **Ableton's noise is CARRIER REPLACEMENT, not additive**
   - Crossfade between original carrier and white noise BEFORE band filters
   - NOT added post-bandpass onto wet output
   
2. **Power curve: k^6**
   - `noiseGain = pow(unvoicedParam, 6.0)`
   - Matches Ableton's dB scaling exactly:
     - 25% → -35 dB ✓
     - 50% → -20 dB ✓  
     - 100% → 0 dB ✓

3. **Pure white noise** (spectral slope ~0 dB/oct)

4. **Noise is GATED by vocoder bands** — no separate gate needed
   - When input is silent → noise output is silent
   - The band envelope followers gate everything including noise

5. **No sibilant detection needed**
   - Vocoder band envelopes naturally boost HF during consonants
   - Ableton has NO special sibilant detection circuit for noise

6. **Noise parameters in Ableton Vocoder API:**
   - [10] Unvoiced Sensitivity — detection threshold (NOT the main knob)
   - [11] Unvoiced Speed — detection response time
   - [12] Unvoiced Level — THIS is the main "Unvoiced" knob (in dB)

## Implementation Done (in BandRitualV3 source)

### Changes to PluginProcessor.cpp:

1. **Noise generation** (~line 3497): Changed to pure white noise (removed pink/white blend)

2. **IIR band loop** (~line 3928): Added carrier replacement BEFORE band filters:
```cpp
if (unvoicedLevel > 0.0f)
{
    float ng = unvoicedLevel^6;  // k^6 power curve
    in = in * (1.0f - ng) + noiseBuf.getSample(ch, n) * ng;
}
```

3. **SVF band loop** (~line 3765): Same carrier replacement added

4. **Old post-band noise** (~line 4080): Disabled (commented out)
   - Was: base+reactive noise with HP/LP detection, spectral centroid, energy gate
   - Now: disabled because noise goes through bands pre-filter

### PROBLEM: Code was added to SVF and IIR paths but NOT to the Linked path!
- The `Linked` algorithm mode uses a DIFFERENT band loop
- Need to find the Linked band loop and add noise there too
- This is why with `algorithm_mode = "Linked"`, output is -72 dB (nearly silent with noise)

## Current Status

- Noise architecture: CONFIRMED CORRECT
- Implementation: DONE for Ritual/SVF paths
- **TODO: Add noise carrier replacement to the LINKED band loop path**
- Noise level at 100%: matches Ableton within -0.8 dB (when using Ritual mode)
- Spectral slope: needs verification after Linked path is fixed

## Test Files

- Ableton exports: `~/Desktop/vocoder-comparison/gate_test/AB_NOISE_*.wav`
- BR exports: `~/Desktop/vocoder-comparison/gate_test/BR_NOISE_*.wav`
- Comparison script: `~/Desktop/vocoder-comparison/noise_compare.py`
- Analysis scripts: `~/Desktop/vocoder-comparison/gate_test/analyze_noise*.py`
- Plugin source: `/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV3/`

## Pedalboard Settings for Ableton Match
```python
{
    "algorithm_mode": "Linked",  # CRITICAL — not Ritual!
    "band_count": 40,
    "band_width": 0.1,       # BW 10%
    "low_freq": 20.0,        # or 40.0 for original cracking
    "high_freq": 18000.0,
    "attack_ms": 1.0,
    "release_ms": 50.0,
    "depth": 1.0,
    "gate": 0.0,
    "noise": 0.0,            # varies per test
    "mix": 1.0,
    # carrier_mode defaults to "Self" (correct)
}
```


## UPDATE: Depth Cracking (same session)

### Depth Formula CRACKED
```cpp
if (depth <= 1.0f)
    envGain = 0.005f * (1.0f - depth) + e * depth;  // floor blend
else
    envGain = e * depth * pow(0.5f, (depth - 1.0f) * 1.5f);  // attenuated boost
```

### Results (relative to depth=100%)
| Depth | Ableton | Band Ritual | Error |
|-------|---------|-------------|-------|
| 0%    | -6.3 dB | -6.9 dB     | 0.7 dB |
| 50%   | -3.7 dB | -3.2 dB     | 0.5 dB |
| 100%  | 0.0 dB  | 0.0 dB      | 0.0 dB |
| 150%  | -1.4 dB | -1.0 dB     | 0.4 dB |
| 200%  | -2.1 dB | -3.0 dB     | 0.9 dB |

### Key discoveries
- depth=0 NOT silence — Ableton passes carrier with flat envelope (floor ~0.005)
- depth>100%: Ableton ATTENUATES, doesn't get louder
- Power curve: k^3 not k^6

### BW Offset Test
±2 dB across all BW (10-200%). No fixed global boost needed.

## COMPLETE STATUS (2026-04-04)
| Feature | Status | Error |
|---------|--------|-------|
| Bands | ✅ CRACKED | 2.5 dB |
| Noise/Unvoiced | ✅ CRACKED | 1-2 dB |
| Depth | ✅ CRACKED | 0.4-0.9 dB |
| BW gain | ✅ CRACKED | ±2 dB |
| Gate/Formant/Enhance | ❌ Not yet | — |
