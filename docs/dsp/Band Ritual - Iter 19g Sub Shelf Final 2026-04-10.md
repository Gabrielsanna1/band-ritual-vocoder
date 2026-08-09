# Band Ritual — Iter 19g: Sub shelf (final state for this session)

**Date:** 2026-04-10
**Predecessor:** Iter 19f (true Butterworth BP via LP→BP transform, per-band envelope release LUT, Q=37)
**Status:** Best locked-in state of the day.

---

## Real music results (kick/bass DRY-EXPORT 1.wav)

| Metric                 | Iter 19f (before) | Iter 19g (after) |
|---                     |---                |---               |
| A-weighted loudness    | −0.86 dBA         | **−0.34 dBA**    |
| Spectrogram cosine     | 0.873             | **0.889**        |
| Mid 500-2k gap         | −3.7 dB           | **−1.2 dB**      |
| Sub 20-80 hot          | +3.7 dB           | **+3.2 dB**      |
| Broadband RMS diff     | +3.7 dB           | +3.0 dB          |

**A-weighted −0.34 dBA = perceptually identical loudness on real content.**

---

## The investigation chain (for future reference)

### Starting point
Iter 19f had all known architectural corrections in place. The persistent +3.7 dB sub hot on real music was the only remaining major gap. All previous per-band and Q tweaks were exhausted without progress.

### Hypothesis 1 — REJECTED: "BR has wrong number of sub bands"
`analyze_sub_bands.py` on slow sweep output:
- ABL: 11 detected prominent peaks in 20-100 Hz
- BR:  5 detected prominent peaks in same range

Initial interpretation: BR bands are merging into a plateau. Either lower Q to let bands resolve, or add bands to match ABL's density.

### Test of hypothesis 1 — FAILED
Applied sub-band Q reduction (BW floor ≥ 4 Hz below 150 Hz, mapping Q from 37 → 12.5 at 50 Hz). Result:
- BR sub avg got **louder** (−2.76 → −1.23 dB)
- Still 5 peaks, still flat plateau

Cause of reasoning error: lower Q widens the filter, increases overlap, makes the plateau worse. The frequency-vs-time uncertainty principle means any biquad configuration has the same fundamental tradeoff.

### Hypothesis 2 — PARTIALLY INVESTIGATED: "Ableton uses STFT/FFT path at sub"
The ~1.5 Hz spacing in ABL peaks was too fine for 40 log-spaced IIR bands → jumped to conclusion that Ableton must use a spectral (FFT) path at sub.

**Scope of the potential rewrite:** days of work, architectural change, risk of regression on everything else already working.

### Decisive test — `sub_steadystate_test.py` — HYPOTHESIS 2 REJECTED
Reused the existing slow sweep and recomputed the sub response with **increasing analysis window sizes** (30 → 800 ms). Logic: real discrete bands persist at any window size (they reflect steady-state filter response); sweep-filter transient artifacts average out as the window grows.

```
  win_ms     ABL_std  ABL_peaks    BR_std  BR_peaks
      30        4.84         34      3.80         3
      60        4.43         24      3.60         3
     100        4.03         16      3.32         3
     200        3.58          4      2.66         3
     350        3.42          3      1.69         2
     500        3.43          1      1.14         1
     800        3.41          0      0.78         0
```

ABL peaks collapse from 34 → 0 as window grows. **The "11 discrete bands" were sweep-filter transient artifacts, not real bands.** An STFT rewrite would have been completely wasted work.

### Hypothesis 3 — CONFIRMED: "Ableton has a low-shelf rolloff below ~45 Hz"
Looking at the 500 ms raw curve (25-80 Hz), a smooth structural difference remains:

```
 freq      ABL dB     BR dB
 25.00    -10.96     +0.89
 30.04     -3.25     +0.89
 40.80     +0.14     +0.73
 52.12     +2.41     -0.02
 75.25     +4.72     -2.57
```

**ABL has a smooth rising shelf from 25 → 75 Hz (~16 dB total, ~10 dB/oct slope).**
**BR is flat across the same range.**

This is not an architectural difference — it's a measured low-shelf that Ableton applies at sub and BR doesn't have. Common in commercial vocoders to remove mud below ~50 Hz.

### Fix
Per-band gain attenuation on the carrier for bands below 45 Hz, slope −13 dB/octave, floored at −14 dB. Curve read directly from measurement, not fitted.

```cpp
// In updateBandFilters, Linked mode branch:
float fi_sub = bands[i]->currentFreq;
if (fi_sub < 45.0f)
{
    float oct = std::log2 (45.0f / juce::jmax (fi_sub, 18.0f));
    float dbCut = juce::jmax (-14.0f, -13.0f * oct);
    bands[i]->tiltCorr *= std::pow (10.0f, dbCut / 20.0f);
}
```

Applied via `tiltCorr` multiplier → composes cleanly with existing per-band tilt correction LUT, no new state, no new filter.

---

## Lessons for future sessions

1. **Don't jump from evidence to architecture.** I saw 11 peaks in a sweep test and immediately hypothesized FFT-based processing, which would have cost days. The decisive diagnostic (varying the analysis window) took 2 minutes and falsified the hypothesis. Always run the cheap diagnostic before committing to expensive rewrites.

2. **Sweep-filter transient artifacts masquerade as discrete bands.** When a swept sine enters a narrow BP, the transient response creates ripples in the envelope that look like peaks in RMS analysis but are not steady-state features. Always verify sweep-derived "peaks" with longer windows or with static sines.

3. **The Q vs ring-down tradeoff has no solution with biquads.** You can't get both narrow frequency resolution AND fast temporal decay simultaneously at the same center frequency. If the steady-state response looks like a plateau, the filters are working as designed; the fix has to come from somewhere else (band gain, not filter shape).

4. **A +3 dB sub hot isn't always a Q or filter-order problem.** It can be as simple as "the other tool has a highpass shelf that yours doesn't." Always check for missing static EQ curves before rebuilding filter banks.

---

## Remaining gap (to investigate next session)

After iter 19g the per-region spectrum is:

| Region     | BR-ABL diff |
|---         |---          |
| Sub 20-80  | +3.2        |
| Bass 80-200| +2.9        |
| Low 200-500| +1.2        |
| Mid 500-2k | −1.2        |
| HiMd 2-6k  | +3.6        |
| High 6-12k | +3.0        |
| Air 12-18k | +2.0        |

Broadband is +3.0 dB. Notice the **mid dip**: Low +1.2, Mid −1.2, HiMd +3.6 — that's a 4.8 dB mid scoop spanning 2 octaves. This is a specific spectral feature, not a flat offset, and is the most promising next investigation target. Possible causes:
- BW-dependent gain compensation at mid (overlap correction)
- Output normalization shift from sub shelf
- A genuine midrange curve in Ableton we haven't detected yet

Next session: run a fine-resolution sweep diff (not 7 bands but ~100 log-spaced points) to see the exact shape of the BR-ABL difference curve across the full spectrum, then hypothesize the cause of the mid scoop.
