# BEAT FROG

Drum machine plugin (VST3 + AU, macOS) by **REZONANZA**.

- 8 voices, all synthesised (no samples), 16 kits with their own sounds and patterns
- 16 or 32 step sequencer (pages A/B) that follows Ableton's tempo and transport
- Big LED knob: drag = filter (left low-pass, right high-pass), click = play/stop when Ableton is stopped
- 10 global knobs: swing, drive, room, echo, crush, pitch, decay, comp, width, drift
- MIDI notes C1–G1 (36–43) trigger the 8 voices

## Install

Every push to `main` builds the plugin on GitHub Actions. Open the latest run under **Actions**, download
`BEAT-FROG-mac`, unzip and double-click `install.command`. It copies the plugin to `/Library/Audio/Plug-Ins`
and clears the quarantine flag. Restart Ableton and rescan: BEAT FROG appears under **REZONANZA**.

## Build locally

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Font: Silkscreen (SIL Open Font License, see `Resources/OFL.txt`).
