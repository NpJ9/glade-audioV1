# Glade

A granular synthesizer + effects VST3/Standalone plugin built with JUCE.

## Features

- **Granular engine** — grain size, density, position, position jitter, reverse amount; beat-sync with configurable division
- **Pitch system** — semitone shift (±24 st), pitch jitter, pan spread, glide/portamento
- **ADSR envelope** — attack, decay, sustain, release with animated shape display and live cursor dot
- **4 window types** — Gaussian, Hanning, Trapezoid, Rectangular
- **3 independent LFOs** — 5 shapes (Sine, Triangle, Saw, Square, S&H); 6 targets (Grain Size, Density, Position, Pitch, Pan); each with its own colour (magenta / teal / orange)
- **4 Macro knobs (M1–M4)** — each assignable to up to 4 parameter targets with modulation ring overlay
- **Step sequencer** — 8-step grain position sequencer, BPM-synced
- **10-type FX chain (4 slots)** — Reverb, Delay, Chorus, L.Chorus, Flanger, Distortion, Filter, Shimmer, Harmonic, Auto-Pan; per-slot bypass + mix + drag-to-reorder
- **FREEZE / BEAT SYNC** — freeze grain position; sync grain density to host BPM divisions
- **15 factory presets** — FOREST, SHIMMER, GLITCH, VOID, CRYSTAL, DRONE, STUTTER, NEBULA, AURORA, COSMOS, TRAP, BOUNCE, ARPFOG, MELT, CHAOS
- **RND / WILD randomise** — per-section (grain, pitch) and global; constrained or wide ranges
- **Fractal visualiser** — RMS-driven 3D wireframe cube animation; glitches on randomise
- **Drag & drop sample loading** — WAV, AIFF, FLAC; automatic root note detection
- **Resizable window** — 1000×620 minimum up to 1800×1100; all layout is relative

## Requirements

- Windows 10/11 (64-bit)
- [JUCE](https://juce.com/get-juce/) at `C:/JUCE`
- Visual Studio 2022 / 2025 Community (MSVC)
- CMake 3.22+

## Build

```powershell
# Configure (one-time)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64

# Build VST3 (Release) — incremental
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" `
    build\GladeV2_VST3.vcxproj /p:Configuration=Release /p:Platform=x64 /m /nologo

# Build Standalone
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" `
    build\GladeV2_Standalone.vcxproj /p:Configuration=Release /p:Platform=x64 /m /nologo
```

> Add `/t:Rebuild` if the binary appears stale.

Output locations:
- **VST3**: `build\GladeV2_artefacts\Release\VST3\GladeV2.vst3`
- **Standalone**: `build\GladeV2_artefacts\Release\Standalone\GladeV2.exe`

## Install

```powershell
Copy-Item -Recurse -Force `
    'build\GladeV2_artefacts\Release\VST3\GladeV2.vst3' `
    "$env:LOCALAPPDATA\Programs\Common\VST3\"
```

## Project Structure

```
Source/
  PluginProcessor.h/.cpp     APVTS, GranularEngine, FXChain, PresetManager
  PluginEditor.h/.cpp        Resizable UI layout (default 1400×880)

  Engine/
    GranularEngine.h/.cpp    Grain processing, 3× LFO, ADSR, StepSequencer
    GrainPool.h/.cpp         Fixed 256-grain pool (no audio-thread allocation)
    GrainScheduler.h/.cpp    Grain spawning
    Grain.h                  POD grain state
    SampleBuffer.h/.cpp      WAV/AIFF/FLAC loader, resamples to host rate
    WindowFunction.h         Grain window shapes
    FXChain.h/.cpp           4-slot FX processor (10 effect types)
    PresetManager.h/.cpp     15 factory presets + user save

  UI/
    GladeLookAndFeel.h/.cpp  Custom LookAndFeel_V4; full colour palette
    GladeKnob.h/.cpp         Knob + LFO/Macro overlay rings + param-aware formatting
    WaveformDisplay.h/.cpp   Sample display, drag-and-drop loader, root note label
    FXSlotUI.h/.cpp          Per-slot FX panel with drag-to-reorder
    FractalVisualizer.h/.cpp 3 nested wireframe cubes, RMS-driven
    LFODisplay.h             Animated LFO waveform with phase cursor
    ADSRDisplay.h            Animated ADSR shape with live envelope cursor
    StepSequencerUI.h        8-step bar editor
```

## Colour Palette

| Token | Hex | Used for |
|---|---|---|
| `cyan` | `#00e5d4` | Grain, Sample, Window sections |
| `green` | `#1aff8c` | Pitch section |
| `yellow` | `#f5c518` | Output section |
| `magenta` | `#ff2a6d` | Macro section, LFO1, RND controls |
| `purple` | `#9b5de5` | Envelope section, LFO section header |
| `teal` | `#00cba5` | LFO2 (distinct from Pitch green) |
| `orange` | `#ff9944` | LFO3 (distinct from Output yellow) |

FX slot accents follow each effect type (purple = Reverb/Shimmer, cyan = Delay, green = Chorus, etc.).

## License

Personal / educational use. Not for redistribution.
