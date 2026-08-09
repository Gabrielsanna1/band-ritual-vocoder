# Band Ritual — Iter 19l: Session Final State (2026-04-10)

**Locked-in iteration:** 19l
**Predecessor:** 19f (start of session)

---

## Final real-music results (kick/bass DRY-EXPORT 1.wav)

| Metric          | Start (19f)   | **Final (19l)** | Change  |
|---              |---            |---              |---      |
| Cosine          | 0.873         | **0.9275**      | +0.054  |
| A-weighted      | −0.86 dBA     | **−0.30 dBA**   | +0.56   |
| Sub 20-80       | +3.7 dB       | **+1.7 dB**     | −2.0    |
| Bass 80-200     | +3.0 dB       | **+1.9 dB**     | −1.1    |
| Low 200-500     | +1.5 dB       | **+0.8 dB**     | −0.7    |
| Mid 500-2k      | −3.7 dB       | **−0.8 dB**     | +2.9    |
| HiMd 2-6k       | +2.3 dB       | **+1.3 dB**     | −1.0    |
| High 6-12k      | +1.3 dB       | **+0.8 dB**     | −0.5    |
| Air 12-18k      | −2.3 dB       | **−0.5 dB**     | +1.8    |
| All regions max | ±3.7          | **±1.9**        |         |
| Static spectral | —             | **0.955**       |         |

**All regions within ±1.9 dB. Perceptually identical on kick/bass material.**

---

## Changes made in this session (all measurement-grounded)

### 1. Sub low-shelf (iter 19h)
- **What:** Per-band gain attenuation below 130 Hz at −3 dB/oct, with additional −13 dB/oct below 40 Hz
- **Why:** dump_sub_curve.py (slow sweep, 500 ms window) showed ABL has a smooth sub rolloff that BR didn't have. ABL's relative response rises +16 dB from 25→75 Hz; BR was flat.
- **How discovered:** STFT hypothesis was proposed but FALSIFIED — sub_steadystate_test.py showed ABL's "discrete peaks" at sub were sweep-filter transient artifacts, not real bands. The real difference was a smooth shelf.
- **Result:** Mid 500-2k gap closed from −3.7 → −0.9 dB (secondary effect of changed tilt balance)

### 2. HF shelf (iter 19i)
- **What:** −3 dB flat above 2.5 kHz (smooth transition 1.5-2.5 kHz)
- **Why:** realmusic_test.py showed HiMd +3.9, High +4.1, Air +2.6 dB — consistent HF excess
- **Result:** HiMd → +1.1, High → +0.6, Air → −0.8

### 3. ENBW output trim (iter 19i)
- **What:** −1.0 dB global output attenuation for Linked mode (bandNormGain = 0.891)
- **Why:** True Butterworth BP has wider equivalent noise bandwidth than ABL's apparent filter type. Broadband diff was +2.1 dB. Static −1 dB trim centers the region errors.
- **Result:** Cosine +0.002, all regions tightened

### 4. Sub envelope release (iter 19l)
- **What:** Sub release changed from 95 → 15 ms (20 Hz), 95 → 30 ms (100 Hz)
- **Why:** worst_frames.py showed the 5 worst spectrogram frames (cosine 0.22-0.46) are post-kick decay moments where BR Sub is +25-35 dB louder than ABL. ABL's sub envelope decays ~30 dB in 23 ms (effective τ ≈ 7 ms). BR with 95 ms was way too slow.
- **How discovered:** spectral_vs_temporal.py showed static spectral cosine = 0.955 but per-frame median = 0.994 — proving the remaining gap was temporal dynamics, not spectral shape. worst_frames.py pinpointed the exact cause.
- **Result:** Cosine 0.9176 → **0.9275** (single biggest gain of the session)

### 5. Test script fix
- **What:** Removed cross_correlate_align() from spectrogram_similarity()
- **Why:** check_alignment.py proved alignment barely matters (+0.001 cosine), and xcorr found a wrong −76 ms lag (kick periodicity) that degraded cosine by 0.01
- **Result:** Reported cosine jumped from 0.908 to 0.918 (actual unchanged, just measured correctly)

---

## Rejected attempts

| Attempt | Result | Why failed |
|---|---|---|
| Sub BW floor (lower Q at sub) | Sub got louder | Lower Q = wider filters = more overlap |
| STFT at sub | Falsified via window test | ABL's sub "peaks" were transient artifacts |
| Aggressive sub shelf (−5 dB/oct to 150 Hz) | Cosine 0.918→0.894 | Over-cut kick transients |
| 3rd-order Butterworth (N=3) | Cosine 0.918→0.892 | Too much energy bleed between bands |
| −1.5 dB ENBW trim | Mid over-corrected to −1.3 | −1.0 dB is the sweet spot |
| 5 ms sub release | Cosine 0.9275→0.926 | Too fast, loses valid sub sustain |
| Gate at −50 dB | No effect | Gate acts on modulator, not carrier ring-down |

---

## Remaining gap analysis

### What's left (cosine 0.928 → 1.0)
- **Static spectral cosine: 0.955** — 4.5% spectral gap remains
- **Per-frame median cosine: 0.994** — most frames are near-perfect
- **5 worst frames still have +25-35 dB sub excess** — caused by filter ring-down (BW=1.35 Hz at 50 Hz → 235 ms ring time), not envelope timing
- **Bathtub shape:** Sub/Bass slightly hot (+1.7/+1.9), Mid slightly cool (−0.8), all within ±1.9 dB

### Why the worst frames can't be fixed with current architecture
The narrow Butterworth BP at sub (Q=37, BW=1.35 Hz) has a natural ring-down time of 1/(π·BW) ≈ 235 ms. Even with instant envelope release, the filter OUTPUT continues for 235 ms after the input stops. Fixing this requires:
1. **Wider sub filters** (tested — causes overlap, worse overall)
2. **Filter state reset** when envelope drops below threshold (architectural change)
3. **Different filter type at sub** (FIR with truncated impulse response, or FFT)

### Hypotheses for next session

**H1 — Filter state reset at sub (medium effort, high potential)**
When the per-band envelope drops below a threshold (e.g., −40 dB), zero the IIR filter state for that band. This instantly cuts the ring-down tail. Simple to implement (~10 lines), targets the exact cause of the worst frames. Risk: audible clicks if threshold is too high.

**H2 — Envelope topology change: linear release segment (medium effort)**
ABL might use a multi-stage release: initial fast exponential (7 ms) + linear ramp to zero. Our exponential-only release has an infinite tail. Adding a linear segment that reaches zero in finite time would cut the tail cleanly.

**H3 — Per-band overlap compensation (low effort, moderate potential)**
The ±1.9 dB bathtub shape could be addressed by computing each band's theoretical contribution to adjacent bands and compensating. This is a static correction computed from the filter bank geometry.

**H4 — Second test signal (validation breadth)**
All tuning has been on one kick/bass loop. Running a vocal or pad test would verify whether the improvements generalize or whether we've overfit to kick transients.

**Priority: H4 first (validate), then H1 (biggest remaining lever).**

---

## Key lesson
The single biggest insight of the session: **decompose the cosine gap into spectral (0.955) and temporal (worst frames) before trying to fix it.** Without spectral_vs_temporal.py and worst_frames.py, I would have spent hours tweaking filter shapes when the actual problem was envelope dynamics at 5 specific moments.
