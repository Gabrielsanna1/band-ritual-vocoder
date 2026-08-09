# Band Ritual — DSP Iteration Workflow

## When to Simulate vs Record in Ableton

### Simulate in Python (fast iteration)
- **When tuning filter parameters** (Q, stage count, Qa ratio, gain curves)
- PSD **shape** (peak width, valley depth) depends almost entirely on filter math
- The JUCE SVF math is deterministic and can be replicated exactly:
  - `g = tan(π * fc / fs)`
  - `R2 = 1 / Q`
  - `h = 1 / (1 + R2*g + g²)`
  - Same state variable update equations
- Level normalization in PSD comparison cancels out any gain differences
- **Saves hours** — no need to rebuild plugin + install + rescan + re-record for every Q tweak

### Record in Ableton (ground truth verification)
- After simulation finds the best parameters → ONE final recording to verify
- When testing things simulation can't capture:
  - Parameter smoothing / block processing artifacts
  - Plugin GUI interactions (gate, EQ, complement mode)
  - Full signal chain behavior (mix, mod level, depth interactions)
  - Edge cases at extreme settings

### Workflow
1. Simulate multiple parameter values in Python (batch sweep)
2. Find the best match via PSD comparison
3. Apply winning parameters to C++ code
4. Build plugin → install → record in Ableton
5. Verify with real recording PSD comparison
6. If mismatch > threshold → investigate what simulation missed

## SVF vs IIR — Key Differences (learned 2026-03-21)

### SVF (StateVariableTPTFilter) bandpass
- **Peak gain = Q** (not unity like IIR makeBandPass)
- Must compensate: `bandNormGain = 1 / (Qa^n_analysis × Qc^n_carrier)`
- No bilinear transform bandwidth compression at HF
- Deeper valleys between bands (better stopband rejection)
- "Too perfect" for matching Ableton — Ableton uses IIR

### IIR (makeBandPass) bandpass
- Peak gain = 1.0 (unity at center frequency)
- Bilinear transform compresses BW at HF: `actual_BW = designed × sin(2πf/fs)/(2πf/fs)`
- Natural inter-band leakage at HF (shallower valleys)
- Matches Ableton's vocoder filter type

### Ableton Vocoder Range
- Frequency range: **20 Hz to 18 kHz** (NOT 20 kHz)
- Band Ritual must match this for accurate comparison

## Test Infrastructure
- White noise source: `~/Downloads/TEST_white_noise_10s.wav`
- Ableton crack recordings: `~/Desktop/vocoder-comparison/ableton_crack/crack_bw{N}_noise.wav`
- Simulation script: `~/Desktop/vocoder-comparison/simulate_final2.py`
- PSD comparison script: `~/Desktop/vocoder-comparison/compare_final2.py`
- BW mapping: Ableton BW% ÷ 100 = Band Ritual `bandWidth` parameter value (range 0.1–2.0)

## Recording Settings (for Ableton exports)
- Carrier mode: SELF
- Scale Amount: 0%
- Algorithm: Linked mode
- 40 bands
- Depth: 100%, Mix: 100%, Mod Level: 100%
- Gate: 0 (off — matches Ableton's -∞)
- EQ: OFF
