# Ableton Live 12 — Custom Remote Script Installation (CRITICAL)

## The Correct Path (since Live 10.1.13+)

```
~/Music/Ableton/User Library/Remote Scripts/YourScriptName/
```

**NOT** `~/Library/Preferences/Ableton/Live 12.x.x/User Remote Scripts/` — that's the OLD location (Live 9 era). Ableton 10+ does NOT scan that folder for Control Surface scripts.

## The Wrong Paths (DO NOT USE)

- `~/Library/Preferences/Ableton/Live 12.x.x/User Remote Scripts/` — OLD, not scanned
- `/Applications/Ableton Live*.app/Contents/App-Resources/MIDI Remote Scripts/` — app bundle, may be read-only
- Any path inside a DMG/ISO volume — read-only file system

## What Happened (2026-03-25)

Building SoundClaw VST plugin. The plugin auto-installed its Remote Script to the app bundle path in `/Applications/`. Then we tried the `User Remote Scripts` folder in `~/Library/Preferences/`. Neither worked. Wasted 30+ minutes.

**The fix**: Copy to `~/Music/Ableton/User Library/Remote Scripts/SoundClaw/` — worked immediately.

## Proof It Works

`Logi_Plugin` was already installed at the correct path on this machine and appeared in Ableton's Control Surface dropdown. Ableton auto-compiles `.py` → `.pyc` (Python 3.11) in a `__pycache__/` subfolder.

## Installation Checklist

1. Create folder: `~/Music/Ableton/User Library/Remote Scripts/YourScript/`
2. Add `__init__.py` with `create_instance(c_instance)` function
3. Add your control surface `.py` file
4. Quit Ableton completely (Cmd+Q)
5. Reopen Ableton
6. Go to Preferences > Link/Tempo/MIDI > Control Surface dropdown
7. Your script should appear

## Script Requirements

- `.py` files work fine — no need to pre-compile to `.pyc`
- Use `super(ClassName, self).__init__(c_instance)` for compatibility
- Ableton 12 uses Python 3.11
- `_Framework.ControlSurface` is available for User Remote Scripts
- Folder name must not have spaces
- Unix line endings (`\n`), NOT Windows (`\r\n`)
- No leading blank lines in files

## Also Update the VST Auto-Installer

The SoundClaw VST's `installRemoteScript()` in `BridgeClient.cpp` currently writes to the app bundle path. It needs to be updated to write to `~/Music/Ableton/User Library/Remote Scripts/` instead.

## Sources

- https://help.ableton.com/hc/en-us/articles/209072009
- AbletonOSC GitHub: https://github.com/ideoforms/AbletonOSC
