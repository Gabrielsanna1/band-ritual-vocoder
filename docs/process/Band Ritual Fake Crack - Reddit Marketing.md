# Band Ritual Fake Crack - Reddit Marketing Strategy

## What This Is
A fake "cracked" version of Band Ritual VST3 for guerrilla marketing. The plugin loads in DAWs, shows the Ritual Audio Labs auth screen with logo, but every license key always fails. Zero real DSP code inside — just a JUCE shell with branding.

## Files Location
`/Volumes/Ricosan HD/AI JOURNEY/DEV APPS/05_MUSIC_AUDIO/BAND_RITUAL_FAKE_CRACK/`

### Ready-to-share zips:
- `BandRitual_v0.0.1_CR4CK_macOS.zip` (2.8MB) — Intel + Apple Silicon universal
- `BandRitual_v0.0.1_CR4CK_Windows.zip` (2.2MB) — Windows 64-bit

### Source/build files:
- `BandRitualFake/` — JUCE project source code (CMake + C++)
- `BandRitual_CR4CK/` — macOS package contents (vst3 + README + keygen.txt)
- `BandRitual_CR4CK_WIN/` — Windows package contents
- `BandRitual-Windows-VST3/` — raw GitHub Actions build output

### Post text:
- `BandRitual_CR4CK/POST_TEXT.txt` — ready-to-paste Reddit post (needs Google Drive links)

## How It Works
1. User downloads zip, copies .vst3 to plugin folder
2. DAW scans and finds "Band Ritual" — loads normally
3. Opens auth screen: Ritual Audio Labs logo, "BAND RITUAL" title, key input
4. User tries keys from keygen.txt or any key → fake "Verifying with Gumroad..." delay (1.5-3s) → random error message every time
5. Error messages rotate: "License key not found", "Already activated on another machine", "Server verification failed", "License key is invalid or has been revoked"

## What's Inside (Zero IP Risk)
- No real vocoder DSP code
- processBlock just outputs silence (or passthrough if auth bypassed)
- Fake DSP arrays/matrices to pad binary size (~5.7MB macOS, ~2.2MB Windows)
- Stripped debug symbols
- If someone patches past auth → plugin "opens" but does nothing → looks like broken crack, not fake

## Reddit Post Strategy
- Target subreddit: r/CrackedPluginsXI (check rules before posting)
- Post as throwaway account, not real identity
- Frame as: "New vocoder plugin, includes keygen with working keys"
- When people complain keys don't work → engage from real account as dev → free advertising
- Every comment bumps the thread = more visibility

## GitHub Private Repo (for Windows builds)
- `eqenrico-hub/bandritual-build` (PRIVATE)
- Used GitHub Actions to cross-compile Windows VST3
- **DELETE THIS REPO** after you're done: `gh repo delete eqenrico-hub/bandritual-build --yes`

## To Deploy
1. Upload both zips to Google Drive
2. Set sharing to "Anyone with the link"
3. Paste links into POST_TEXT.txt
4. Take screenshot of plugin loaded in DAW (install fake version, open in Ableton, screenshot auth screen)
5. Post on Reddit with screenshot + post text
