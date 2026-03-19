# Glade

A granular synthesizer + effects VST3/Standalone plugin built with JUCE 7.

![Glade UI](docs/screenshot.png)

## Features

- **Granular engine** — grain size, density, position, jitter; beat-sync with configurable division
- **Pitch system** — semitone shift (±24), pitch jitter, pan spread, scale quantizer (7 scales), root note
- **ADSR envelope** — attack, decay, sustain, release
- **4-window types** — Gaussian, Hanning, Trapezoid, Rectangular
- **LFO** — 5 shapes (Sine, Triangle, Saw, Square, S&H), 6 targets (Grain Size, Density, Position, Pitch, Pan)
- **Envelope follower** — audio-reactive modulation with configurable attack/release/depth/target
- **Step sequencer** — 8-step grain position sequencer, BPM-synced
- **4-slot FX chain** — Reverb, Delay, Chorus, Distortion, Filter; per-slot bypass + mix
- **15 factory presets** — FOREST, SHIMMER, GLITCH, VOID, CRYSTAL, DRONE, STUTTER, NEBULA, AURORA, COSMOS, TRAP, BOUNCE, ARPFOG, MELT, CHAOS
- **RND / WILD randomise** — randomise grain/pitch/LFO params or everything
- **Fractal visualizer** — RMS-driven 3D wireframe cube animation; glitches on randomise
- **Drag & drop sample loading** — WAV, AIFF, FLAC; automatic root note detection

## Requirements

- Windows 10/11 (64-bit)
- [JUCE 7](https://juce.com/get-juce/) at `C:/JUCE`
- Visual Studio 2022 Community (MSVC v17+)
- CMake 3.22+

## Build

```powershell
# Configure
cmake -S . -B build -G "Visual Studio 17 2022" -A x64

# Build VST3 (Release)
cd build
& "C:\Program Files\Microsoft Visual Studio\17\2022\MSBuild\Current\Bin\MSBuild.exe" Glade_VST3.vcxproj /p:Configuration=Release /t:Rebuild /m

# Build Standalone
& "C:\Program Files\Microsoft Visual Studio\17\2022\MSBuild\Current\Bin\MSBuild.exe" Glade_Standalone.vcxproj /p:Configuration=Release /t:Rebuild /m
```

Use `/t:Rebuild` if the binary appears stale — MSBuild's incremental detection can miss changes.

## Install

Copy the VST3 bundle to your DAW's plugin folder:

```powershell
Copy-Item -Recurse -Force 'build\Glade_artefacts\Release\VST3\Glade.vst3' 'C:\Users\<you>\AppData\Local\Programs\Common\VST3\'
```

Standalone executable: `build\Glade_artefacts\Release\Standalone\Glade.exe`

## Project Structure

```
Source/
  PluginProcessor.h/.cpp     APVTS, GranularEngine, FXChain, PresetManager
  PluginEditor.h/.cpp        1400x880 UI layout

  Engine/
    GranularEngine.h/.cpp    Grain processing, LFO, ADSR, EnvFollower, StepSeq
    GrainPool.h/.cpp         Fixed 256-grain pool (no audio-thread allocation)
    GrainScheduler.h/.cpp    Grain spawning, ScaleQuantizer
    Grain.h                  POD grain state
    SampleBuffer.h/.cpp      WAV/AIFF/FLAC loader, resamples to host rate
    WindowFunction.h         Grain window shapes
    FXChain.h/.cpp           4-slot FX processor
    ScaleQuantizer.h         Pitch jitter scale snapping
    PresetManager.h/.cpp     15 factory presets
    EnvelopeFollower.h       Header-only one-pole follower
    StepSequencer.h/.cpp     8-step BPM-synced sequencer
    PitchDetector.h          YIN-based MIDI note detection

  UI/
    GladeLookAndFeel.h/.cpp  Custom LookAndFeel_V4
    GladeKnob.h/.cpp         Knob + LFO/Env overlay rings
    WaveformDisplay.h/.cpp   Sample display, drag-and-drop loader, root note label
    FXSlotUI.h/.cpp          Per-slot FX panel
    FractalVisualizer.h/.cpp 3 nested wireframe cubes, RMS-driven
    LFODisplay.h             Animated LFO waveform with phase cursor
    StepSequencerUI.h        8-step bar editor
```

## License

Personal / educational use. Not for redistribution.
