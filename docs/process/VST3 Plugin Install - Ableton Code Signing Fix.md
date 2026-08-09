# VST3 Plugin Install — Ableton Code Signing Fix

## The Problem
After building a VST3 with CMake + JUCE, the plugin **doesn't appear in Ableton Live** even though the `.vst3` bundle is correctly installed in `/Library/Audio/Plug-Ins/VST3/`.

## Root Cause
JUCE's CMake build process ad-hoc signs the binary, then **adds `moduleinfo.json` AFTER signing**, which invalidates the code signature seal. macOS and Ableton reject plugins with broken signatures during scan — the plugin silently fails to load with no error shown to the user.

You can verify this with:
```bash
codesign -vvv /Library/Audio/Plug-Ins/VST3/PluginName.vst3
# Output: "a sealed resource is missing or invalid"
```

## The Fix
**Always re-sign after copying to the install folder.** Add `codesign --force --deep --sign -` to every install command.

## Standard VST3 Install Command (ALWAYS use this)
```bash
sudo rm -rf /Library/Audio/Plug-Ins/VST3/PluginName.vst3 && sudo cp -R "path/to/build/PluginName_artefacts/Release/VST3/PluginName.vst3" /Library/Audio/Plug-Ins/VST3/ && sudo codesign --force --deep --sign - /Library/Audio/Plug-Ins/VST3/PluginName.vst3
```

## After Installing
1. Open Ableton Live
2. Go to **Preferences** (Cmd+,) → **Plug-ins** tab
3. Make sure **"Use VST3 Plug-in System Folders"** is ON
4. Click **Rescan**

## Checklist
- [ ] Build with `cmake --build build --config Release`
- [ ] Copy `.vst3` to `/Library/Audio/Plug-Ins/VST3/` (with sudo)
- [ ] **Re-sign with `codesign --force --deep --sign -`** ← THIS IS THE KEY STEP
- [ ] Rescan in Ableton

## Discovered
- **Date:** 2026-03-10
- **Plugin:** NoteShaper (first time this was diagnosed)
- **Applies to:** ALL JUCE VST3 plugins built with CMake on macOS
