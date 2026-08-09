---
tags:
  - band-ritual
  - launch
  - checklist
date: '2026-03-20'
---
# Band Ritual — Pre-Launch Checklist

## Before uploading to Gumroad

### Discord Link
- [ ] Update the Discord link that appears in the VST GUI (clickable icon)
- [ ] Check all places it appears: PluginEditor, WebView UI, any popup/about screen
- [ ] Test that the link opens correctly after build

### Other pre-launch items
- [ ] Update version number
- [ ] Update LicenseManager DEV OVERRIDE (must be OFF for shipping)
- [ ] Code sign for Ableton: `codesign --force --deep --sign -`
- [ ] Test in Ableton (rescan plugins)
- [ ] Windows build if needed
