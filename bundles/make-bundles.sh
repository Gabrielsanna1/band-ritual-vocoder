#!/bin/bash
# Regenerates the AI context bundles from the repo contents.
# Run from the repo root:  bash bundles/make-bundles.sh
set -e
cd "$(dirname "$0")/.."
OUT=bundles

hdr () {   # hdr <file> <title> <blurb>
cat > "$1" <<EOF
# BAND RITUAL — $2

Auto-generated context bundle. Do not edit by hand; regenerate with \`bundles/make-bundles.sh\`.
Source repo: https://github.com/eqenrico-hub/band-ritual-prototype

$3

---------------------------------------------------------------------------
HOW TO USE THIS FILE
---------------------------------------------------------------------------
Upload or paste this whole file into any AI chatbot (Gemini, ChatGPT, Claude,
Copilot, Mistral, DeepSeek...) and then ask it questions about the project.
Everything it needs is inside this one file.

Good opening prompt:

  "This is the complete development record of a vocoder plugin called Band
   Ritual, which reverse-engineered Ableton Live's stock Vocoder. Read all of
   it, then give me a summary of the final architecture and the list of things
   that were tried and rejected."

---------------------------------------------------------------------------
READ THIS BEFORE TRUSTING ANY NUMBER IN HERE
---------------------------------------------------------------------------
1. These notes were written across three months and CONTRADICT EACH OTHER,
   because the author's understanding kept changing. When two notes disagree,
   THE LATER ONE WINS. Dates are in the filenames and headers.

2. Notes titled "FINAL", "DEFINITIVE", "v12", "v13" were NOT final. Trust the
   dates, not the titles.

3. Anything measured BEFORE 2026-04-11 was measured on clipped 16-bit audio
   and is INVALID. See the note "Band Ritual - WAV Clipping Bug Discovery
   2026-04-11". Numbers from before that date must not be quoted as fact.

4. The single most reliable summary is the first section below (CONTEXT.md).
   The definitive measured constants are in "Iter 22 Final Architecture".

---------------------------------------------------------------------------

EOF
}

emit () {  # emit <bundlefile> <path> ...
  local out="$1"; shift
  for f in "$@"; do
    [ -f "$f" ] || continue
    {
      echo
      echo "================================================================================"
      echo "FILE: $f"
      echo "================================================================================"
      echo
      cat "$f"
      echo
    } >> "$out"
  done
}

# ---------- ordered file lists ----------
CORE=(docs/CONTEXT.md docs/FILTERS.md docs/00-INDEX.md)

NOTES=()
while IFS= read -r f; do NOTES+=("$f"); done < <(
  find docs/dsp docs/gui docs/process -name "*.md" | sort
)
NOTES+=(docs/PROTOTYPE_GIT_HISTORY.md)

LIVE_SRC=(
  Source/PluginProcessor.h Source/PluginProcessor.cpp
  Source/PluginEditor.h Source/PluginEditor.cpp
  Source/CarrierGenerator.h Source/CarrierGenerator.cpp
  Source/PresetManager.h
  Source/FilmstripLookAndFeel.h Source/FilmstripLookAndFeel.cpp
  Source/STFTVocoder.h Source/STFTVocoder.cpp
  Source/WebViewEditor.h Source/WebViewEditor.cpp
  Source/LicenseManager.h Source/LicenseManager.cpp
  CMakeLists.txt
)

DEAD_SRC=(
  Source/PluginProcessor_v1_calibrated_backup.cpp
  Source/PluginProcessor_stft_backup.h Source/PluginProcessor_stft_backup.cpp
  Source/PluginEditor_backup.h Source/PluginEditor_backup.cpp
  Source/PluginEditor_stft_backup.h Source/PluginEditor_stft_backup.cpp
)

# ---------- 1. NOTES ONLY ----------
A="$OUT/1-BAND-RITUAL-notes-only.md"
hdr "$A" "ALL DEVELOPMENT NOTES" \
"CONTAINS: all 59 development notes + the distilled CONTEXT + the git history.
NO source code. This is the RECOMMENDED bundle - it is the reasoning, which is
the valuable part, and it fits in every chatbot's context window."
emit "$A" "${CORE[@]}" "${NOTES[@]}"

# ---------- 2. NOTES + LIVE CODE ----------
B="$OUT/2-BAND-RITUAL-notes-and-code.md"
hdr "$B" "NOTES + LIVE SOURCE CODE" \
"CONTAINS: everything in bundle 1, PLUS the live C++ source (the code that
actually shipped). Dead/abandoned backup files are excluded.

NOTE ON THE CODE: STFTVocoder.* is an abandoned FFT experiment that is NOT
wired in. WebViewEditor.* + the webui folder are DISABLED (PluginProcessor.cpp
sets USE_WEBVIEW_GUI 0). Both are included because they are interesting, but
neither runs."
emit "$B" "${CORE[@]}" "${NOTES[@]}" "${LIVE_SRC[@]}"

# ---------- 3. ABSOLUTELY EVERYTHING ----------
C="$OUT/3-BAND-RITUAL-everything.md"
hdr "$C" "EVERYTHING (incl. abandoned code)" \
"CONTAINS: bundle 2, PLUS the four abandoned backup source files (~934 KB of
dead code from the STFT era and older GUI attempts).

ONLY use this if you specifically want to study the roads that were not taken.
The dead files are clearly marked. For normal questions, bundle 1 or 2 is
better - this one wastes context on code that was deleted for good reason."
emit "$C" "${CORE[@]}" "${NOTES[@]}" "${LIVE_SRC[@]}" "${DEAD_SRC[@]}"

echo "=== generated ==="
for f in "$A" "$B" "$C"; do
  printf "%-46s %6s KB  ~%sk tokens\n" "$f" \
    "$(( $(wc -c < "$f") / 1024 ))" "$(( $(wc -c < "$f") / 4000 ))"
done
