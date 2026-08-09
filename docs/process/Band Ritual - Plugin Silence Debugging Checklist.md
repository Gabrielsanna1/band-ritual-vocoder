# Band Ritual - Plugin Silence Debugging Checklist

## CRITICAL: Read This FIRST When Plugin Outputs Silence

### Step 1: Check License Override (90% of cases)
The plugin has a license manager that **silently returns zero output** when the trial expires.

**File**: `/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV2-Improved/Source/LicenseManager.h`

**Check these lines are overridden for dev:**
```cpp
bool canProduceAudio() const noexcept      { return true; /* DEV OVERRIDE */ }
bool shouldApplyTrialNag() const noexcept  { return false; /* DEV OVERRIDE */ }
```

**If they're NOT overridden** (returning actual license check results), the plugin WILL output silence after trial expiry. This is the #1 cause of "my code change broke the plugin" false alarms.

**Where it blocks audio** in PluginProcessor.cpp (~line 2887):
```cpp
if (! licenseManager.canProduceAudio())
{
    buffer.clear();
    return;  // <-- ALL AUDIO ZEROED
}
```

### Step 2: Check Build Was Actually Installed
```bash
# Check timestamps - plugin should be recent
ls -la /Users/ricosan/Library/Audio/Plug-Ins/VST3/Band\ Ritual\ Improved.vst3/Contents/MacOS/
# Also check the build artefacts
ls -la "/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV2-Improved/build/BandRitual_artefacts/Release/VST3/Band Ritual Improved.vst3/Contents/MacOS/"
```

### Step 3: Check Header Changes Need Full Rebuild
If you changed ANY `.h` file (STFTVocoder.h, PluginProcessor.h, etc.), ALL `.cpp` files that include it need rebuilding:
```bash
cd "/Volumes/Ricosan HD/FREE STORAGE PROJECTS/PLUGIN JAI 2/ooo/COMBS/BandRitualV2-Improved/build"
find CMakeFiles -name "*.o" -delete
cmake --build . --config Release -j8
```
Just deleting one `.o` file is NOT enough when headers change.

### Step 4: Pedalboard Test Basics
If auto_compare.py produces zero/wrong output:
- Plugin parameters have **correct names** (check with `dir(plugin)`)
- Parameters are set BEFORE processing: `setattr(plugin, 'param_name', value)`
- 1-second warmup is processed before test audio
- Plugin defaults: `algorithm_mode=Ritual`, `band_count=16` — must be explicitly set to `Linked`/`40`

### What This Is NOT
- It's NOT a Pedalboard bug (Pedalboard works fine)
- It's NOT a code bug (usually) — if it worked before and silence appeared after a minor change, check license first
- It's NOT a CMake cache issue (usually) — but do a full rebuild if header changed

## Timeline of License Incidents
- **2026-03-13 ~03:30 UTC**: Trial expired mid-calibration session. Spent ~30 min debugging thinking code changes caused silence. Root cause: `canProduceAudio()` returning false.
- **Fix**: Added DEV OVERRIDE to LicenseManager.h. Must revert before shipping.

## REMEMBER: Revert Before Shipping
Before ANY release build, restore the original license checks in LicenseManager.h:
```cpp
// REMOVE these dev overrides:
// bool canProduceAudio() const noexcept      { return true; /* DEV OVERRIDE */ }
// bool shouldApplyTrialNag() const noexcept  { return false; /* DEV OVERRIDE */ }
```
