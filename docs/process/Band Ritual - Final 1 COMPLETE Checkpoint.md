---
tags:
  - band-ritual
  - vocoder
  - complete
  - final-1
date: '2026-03-24'
---
# Band Ritual Final 1 — COMPLETE (2026-03-24)

## This version is DONE. Do not modify further.
All vocoder cracking work is finalized in this build.

## Summary: 1.8 dB avg RMS match to industry-standard vocoder
- Band grid: logspace(20, 18000, N)
- Filter: 4th order bandpass (2 cascaded biquads)
- Phase alternation: every other band inverted
- Envelope: pow(0.01, 1/(sr*t))
- Q: 11-entry table with cos()^1.29 freq-dep correction
- Tilt: alpha = -0.003*norm - 0.090 + LF shelf
- bwGain: 8-entry calibrated table
- Analysis Qa: 0.85 - 0.55*widthNorm
- Gate: -inf(-60) to +12 dB, offset -17, soft gate

## Location
`/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV3/`

## Next version TODO (Final 2)
1. **Formant: band remapping** — replace filter shifting with envelope remapping + interpolation (fix crackles)
2. **Spectral freeze** — hold spectral envelope as sustained pad
3. **Further gate fine-tuning** — smoother fade behavior
4. **Noise/Unvoiced system** — match Ableton's unvoiced detection
