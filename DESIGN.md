# Glade — Granular Synth & Effects VST
### Design Document v0.2

---

## 1. Overview

**Glade** is a dual-mode VST3/AU audio plugin for Ableton Live combining:
- **SYNTH mode** — granular synthesizer driven by a loaded sample, triggered by MIDI
- **FX mode** — granular processor applied to live audio input or a loaded sample

Target DAW: Ableton Live 11/12 (Windows & Mac)
Format: VST3 primary, AU secondary

---

## 2. Core Concept

The plugin has two tabs at the top: **SYNTH** and **FX**.

Both share the same granular engine — the difference is the audio source:
- **SYNTH**: plays grains from a user-loaded sample, pitched by MIDI input
- **FX**: reads grains from a live audio buffer (with optional freeze/capture)

Both modes feed into a shared **FX Chain** at the output stage.

---

## 3. Granular Engine — Parameters

### 3.1 Grain Controls
| Parameter | Range | Description |
|---|---|---|
| Grain Size | 5 – 500 ms | Duration of each grain |
| Density | 1 – 200 grains/s | Grains spawned per second |
| Position | 0 – 100% | Playhead position in sample/buffer |
| Position Jitter | 0 – 100% | Random scatter around playhead |
| Pitch Shift | -24 – +24 semitones | Pitch offset per grain |
| Pitch Jitter | 0 – 100% | Random pitch scatter per grain |
| Pan Spread | 0 – 100% | Stereo randomisation per grain |
| Window | Gaussian / Hanning / Trapezoid / Rectangular | Grain envelope shape |

### 3.2 Randomise
Each section has a small **[RND]** button that randomises all parameters within that section to musically sensible ranges. There is also a **[RANDOMISE ALL]** button in the header that randomises the entire granular engine at once.

Randomise ranges are constrained — e.g. Grain Size won't randomly jump to 1ms or 500ms, it stays in a playable zone (20–200ms) unless a "Wild" mode toggle is enabled.

### 3.3 Amplitude Envelope (SYNTH mode only)
| Parameter | Range |
|---|---|
| Attack | 0 – 10 s |
| Decay | 0 – 10 s |
| Sustain | 0 – 100% |
| Release | 0 – 20 s |

### 3.4 Output (pre FX chain)
| Parameter | Range | Description |
|---|---|---|
| Dry/Wet | 0 – 100% | Blend of granular vs dry signal |
| Output Gain | -24 – +12 dB | Level into the FX chain |

---

## 4. SYNTH Mode — Additional Features

- **Sample Loader** — drag & drop or file browser (WAV, AIFF, FLAC)
- **Waveform Display** — loaded sample with draggable position marker + jitter band overlay
- **Polyphony** — 1 (mono) to 16 voices
- **Glide/Portamento** — 0 – 2000 ms (mono only)
- **MIDI learn** on all knobs (right-click)

---

## 5. FX Mode — Additional Features

- **Source** toggle: Live Input | Loaded Sample
- **Buffer Length** — 0.1 – 10 s (captured audio held for grain reading)
- **Freeze** button — locks buffer (no new audio written in)
- **Feedback** — 0 – 95% (feeds processed output back into buffer)
- **Record** — captures current live audio into the sample slot

---

## 6. FX Chain (Post-Granular)

A linear chain of up to **4 effect slots**, each slot can be one of:

| Effect | Key Parameters |
|---|---|
| **Reverb** | Size, Damping, Dry/Wet, Pre-delay |
| **Delay** | Time (ms or host-sync), Feedback, Ping-pong toggle, Dry/Wet |
| **Chorus** | Rate, Depth, Voices (2/4/6), Spread, Dry/Wet |
| **Distortion** | Drive, Tone, Type (Soft Clip / Hard Clip / Fold), Dry/Wet |
| **Filter** | Cutoff, Resonance, Type (LP/HP/BP), Drive |
| *(Empty)* | Slot is bypassed |

Each slot has:
- **On/Off bypass toggle**
- **Type selector** (dropdown)
- A compact row of **3–4 knobs** for the key parameters
- **Drag to reorder** slots

The chain is displayed as a horizontal strip below the granular section, always visible regardless of SYNTH/FX mode.

---

## 7. BPM Sync

Glade reads the host BPM from Ableton Live via JUCE's `AudioPlayHead`. This drives:

### 7.1 Delay — Sync Mode
Time parameter switches between **Free (ms)** and **Sync** mode:
| Division | Beats |
|---|---|
| 1/32, 1/16, 1/8, 1/4, 1/2, 1 bar, 2 bars | Standard |
| 1/16T, 1/8T, 1/4T | Triplet |
| 1/16D, 1/8D, 1/4D | Dotted |

### 7.2 LFO — Sync Mode
Already in design. Sync options: 1/32, 1/16, 1/8, 1/4, 1/2, 1 bar, 2 bars, 4 bars.

### 7.3 Grain Scheduler — Rhythmic Mode
Optional **BEAT SYNC** toggle on the Grain section. When active:
- Grain density snaps to a beat division (grains fire on the beat grid)
- Grain Size can also be locked to a beat division (e.g. 1/16 = one sixteenth note at current BPM)
- Creates rhythmic, stuttered textures in sync with the track

| Parameter | Options |
|---|---|
| Density Division | 1/32 – 1 bar |
| Size Division | Free ms / 1/32 – 1/4 |

### 7.4 Implementation
JUCE provides BPM via `AudioPlayHead::PositionInfo`:
```cpp
if (auto pos = getPlayHead()->getPosition())
    if (auto bpm = pos->getBpm())
        currentBpm = *bpm;
```
Beat division in samples = `(60.0 / bpm) * sampleRate * divisionMultiplier`

---

## 8. Modulation

One **LFO** per mode, assignable to any parameter via right-click → "Assign LFO".

| Parameter | Range |
|---|---|
| Rate | 0.01 – 20 Hz |
| Depth | 0 – 100% |
| Shape | Sine / Triangle / Saw / Square / Sample & Hold |
| Sync | Free / Host BPM sync (1/32 – 4 bars) |

Modulated parameters show a thin coloured ring animation on their knob.

---

## 8. UI Design System

### 8.1 Aesthetic

Two references fused together:
- **Abletunes RVRB**: dense professional layout, section panels, RVRB-style glowing knobs, coloured value text
- **Fractal cube image**: chromatic aberration colour split (cyan / green / red-magenta), recursive geometric depth, dark starfield background

The plugin feels like a scientific instrument found in a forest — precise, glowing, alive.

### 8.2 Colour Palette

| Role | Hex | Usage |
|---|---|---|
| Background | `#090b10` | Main plugin body, near-black with slight blue tint |
| Panel | `#111318` | Section panels |
| Panel raised | `#191d28` | Knob fields, input areas |
| Border | `#1e2436` | Section dividers |
| Text primary | `#dde4f0` | Parameter labels |
| Text dim | `#505878` | Unit labels, secondary text |
| **Cyan** | `#00e5d4` | Grain section, primary accent, visualiser channel 1 |
| **Green** | `#1aff8c` | FX chain accent, modulation, visualiser channel 2 |
| **Magenta/Red** | `#ff2a6d` | Randomise, feedback, clip, visualiser channel 3 |
| **Purple** | `#9b5de5` | SYNTH tab, envelope section |
| **Yellow** | `#f5c518` | Output gain, special handles |
| **White** | `#ffffff` | Position markers, waveform peaks |

The three chromatic aberration colours (Cyan, Green, Magenta) come directly from the fractal cube image and are used consistently throughout the visualiser and as accent colours in the FX chain.

### 8.3 Typography

- **Font**: Inter (or system fallback: Segoe UI)
- **Plugin name "GLADE"**: top-left, 13px, bold, letter-spacing 0.2em, `#ffffff`
- **Section labels**: 9px, bold, letter-spacing 0.18em, ALL CAPS, coloured accent
- **Parameter labels**: 9px, regular, `#505878`
- **Values**: 11px, medium, coloured to match section accent
- **Preset name**: 13px, medium, centred in top bar

### 8.4 Knobs (RVRB-style)

- Dark circular body (`#191d28`) with subtle inner drop shadow
- Thin coloured arc ring (270° sweep, starts bottom-left)
- Small coloured dot indicator at current value
- Double-click to type an exact value
- Right-click → Set to Default | MIDI Learn | Assign LFO
- Hover: value tooltip in section accent colour

### 8.5 Fractal Visualiser

Centrepiece of the UI — a compact square display (~200×200px) showing a **real-time animated fractal**.

**Concept**: A recursive cube/box fractal (Menger-sponge-inspired) drawn as glowing wireframe edges, rendered in 2D as an isometric projection. Responds to audio in real time.

**Visual properties:**
- Black background with subtle noise/grain texture (starfield feel)
- Three colour channels drawn with slight positional offset — **chromatic aberration** effect:
  - Cyan edges → mapped to grain position
  - Green edges → mapped to grain density
  - Magenta edges → mapped to pitch / feedback
- Recursion depth: 3 levels (performant, still visually rich)
- Edges glow with additive blending — brighter where channels overlap
- The cube slowly rotates at a base speed, with rotation rate driven by **density** parameter
- Recursion "breathes" — inner subdivisions pulse with **grain size**
- On **Randomise All**: visualiser briefly glitches / re-seeds the fractal geometry

**Implementation options (in order of complexity):**
1. Pre-computed fractal geometry, animated via parameter-driven transforms (easiest, good result)
2. GLSL shader rendered via JUCE OpenGLContext (best visual quality, more work)
3. CPU-rendered pixel buffer via JUCE Graphics (middle ground)

Start with option 1 or 3, upgrade to shader later.

### 8.6 Waveform Display

- Shows loaded sample (SYNTH) or live capture buffer (FX)
- Background `#111318`, waveform fill semi-transparent Cyan (SYNTH) or Green (FX)
- White vertical position marker — draggable
- Shaded band overlay showing jitter range around position
- In FX freeze mode: waveform tints to a faint magenta to indicate locked state

### 8.7 FX Chain Strip

Horizontal strip below the main controls. Each of 4 slots is a small panel:

```
┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐
│ ● REVERB     │ │ ● DELAY      │ │ ○ ──────     │ │ ○ ──────     │
│ [k] [k] [k]  │ │ [k] [k] [k]  │ │    empty     │ │    empty     │
└──────────────┘ └──────────────┘ └──────────────┘ └──────────────┘
```

- `●` = active, `○` = empty/bypassed
- Slot border colour matches effect type (Reverb = purple, Delay = cyan, Chorus = green, Distortion = magenta)
- Click empty slot → dropdown to add an effect

### 8.8 Full Layout

```
┌──────────────────────────────────────────────────────────────────┐
│ GLADE   [◄  MEADOW DREAM  ►]   [RND ALL]   [SYNTH | FX]  [MIDI] │  ← Header
├─────────────────────┬──────────────────┬─────────────────────────┤
│   WAVEFORM          │   FRACTAL        │   ENVELOPE (SYNTH)      │
│   ▓▓▒▒░░▓▓▒▒░░▓▓▒   │   VISUALISER     │   or BUFFER (FX)        │
│                     │   [fractal cube] │                         │
│  [LOAD]  [FREEZE]   │                  │  A    D    S    R       │
│                     │                  │  [k]  [k]  [k]  [k]    │
├──────────┬──────────┴──────┬───────────┴──────────┬─────────────┤
│  GRAIN   [RND]             │  PITCH        [RND]  │  OUTPUT     │
│  Size  Density  Pos  Jitr  │  Shift  Jitr   Pan   │  Gain  Mix  │
│  [k]   [k]    [k]   [k]   │  [k]    [k]    [k]   │  [k]   [k]  │
├──────────────────┬─────────┴──────────────────────┴─────────────┤
│  WINDOW          │  LFO                                          │
│  [Gaussian ▼]    │  Rate    Depth   Shape      Sync             │
│  Jitter [k]      │  [k]     [k]     [Sine ▼]   [1/4 ▼]         │
├──────────────────┴───────────────────────────────────────────────┤
│  FX CHAIN                                                         │
│  [REVERB ●][k][k][k]  [DELAY ●][k][k][k]  [+ ADD]  [+ ADD]     │
└──────────────────────────────────────────────────────────────────┘
```

---

## 9. Technical Stack

### 9.1 Framework
**JUCE 7** (C++) — industry standard for VST development.
- VST3, AU, AAX format output
- Cross-platform (Windows + Mac)
- Custom `LookAndFeel` for all UI components
- `AudioProcessorValueTreeState` (APVTS) for all parameters + DAW automation

### 9.2 Build System
- CMake + JUCE CMake API
- Targets: `GladeVST3`, `GladeStandalone` (for development testing without Ableton)

### 9.3 Audio Engine Architecture

```
GladeAudioProcessor (AudioProcessor)
├── GranularEngine
│   ├── GrainPool           (fixed-size pool, ~512 grains max)
│   │   └── Grain           (position, size, pitch, pan, window phase, amplitude)
│   ├── SampleBuffer        (loaded file or captured live audio)
│   ├── GrainScheduler      (triggers new grains at density interval ± jitter)
│   └── WindowFunction      (Gaussian / Hanning / Trapezoid / Rect)
├── ADSR                    (SYNTH mode — per voice)
├── LFO                     (assignable modulation source)
├── FXChain
│   ├── FXSlot[0..3]        (polymorphic — Reverb / Delay / Chorus / Distortion / Filter)
│   └── MixBlender          (dry/wet per slot)
├── MasterOutput            (final gain, clip protection)
└── ParameterLayout         (APVTS — all params, automation-ready)
```

### 9.4 Key JUCE Classes
| Class | Purpose |
|---|---|
| `AudioProcessor` | Main plugin class |
| `AudioProcessorValueTreeState` | All parameter management + DAW automation |
| `AudioFormatManager` / `AudioFormatReader` | WAV/AIFF/FLAC loading |
| `dsp::Reverb` | Reverb FX slot |
| `dsp::DelayLine` | Delay FX slot |
| `dsp::Chorus` | Chorus FX slot |
| `dsp::WaveShaper` | Distortion FX slot |
| `dsp::StateVariableTPTFilter` | Filter FX slot |
| `OpenGLContext` (Phase 4) | Fractal visualiser rendering |
| `FileChooser` | File browser for sample loading |
| `Timer` | UI animation tick (60fps) |

---

## 10. Development Phases

### Phase 1 — Foundation (audio engine, no UI)
- [ ] JUCE + CMake project setup, VST3 + Standalone targets
- [ ] APVTS parameter layout (all grain, LFO, envelope, FX chain params)
- [ ] Granular engine: grain pool, scheduler, windowing, sample loading
- [ ] Basic ADSR
- [ ] Test in Standalone with white noise / sine wave output

### Phase 2 — Core UI
- [ ] Custom LookAndFeel (dark theme, knob arc ring style)
- [ ] Knob component (arc ring, dot indicator, double-click edit, right-click menu)
- [ ] Section panels: GRAIN, PITCH, OUTPUT, WINDOW, LFO
- [ ] Waveform display (sample view + position marker)
- [ ] All knobs attached to APVTS parameters
- [ ] SYNTH / FX tab switching

### Phase 3 — FX Chain
- [ ] FX slot component (type selector, knobs, bypass toggle)
- [ ] Reverb, Delay, Chorus implementations
- [ ] Distortion + Filter implementations
- [ ] Drag-to-reorder slots

### Phase 4 — Randomise & Modulation
- [ ] Per-section [RND] buttons with constrained ranges
- [ ] [RANDOMISE ALL] with Wild mode toggle
- [ ] LFO assignment via right-click
- [ ] LFO visual ring animation on modulated knobs

### Phase 5 — Fractal Visualiser
- [ ] Isometric wireframe cube geometry (recursive, 3 levels)
- [ ] Chromatic aberration: 3-channel offset render (cyan/green/magenta)
- [ ] Parameter-driven animation (rotation ↔ density, pulse ↔ grain size)
- [ ] Glitch animation on randomise trigger
- [ ] Upgrade to GLSL shader if performance allows

### Phase 6 — Preset System & Polish
- [ ] Preset browser (◄ ► navigation, save/load from disk)
- [ ] 15–20 factory presets
- [ ] MIDI learn
- [ ] CPU profiling (target: <5% at typical settings, 44.1kHz)
- [ ] Installer: Windows (.exe), Mac (.pkg)

---

## 11. Resolved Decisions

| Decision | Choice | Reason |
|---|---|---|
| Plugin name | **Glade** | Evokes natural space, contrasts the digital aesthetic |
| C++ / JUCE | Yes, proceed | Only real path to a proper VST3. Steep curve but worth it |
| Randomise | Per-section + global | Most flexible, matches how musicians explore sounds |
| FX chain | 4 slots, fixed order display | Simple enough to build, powerful enough to be useful |
| Visualiser style | Fractal wireframe cube, chromatic aberration | Direct inspiration from reference image |
| Visualiser colours | Cyan / Green / Magenta | Match the three channels in the reference image exactly |

## 12. Open Questions

1. **Visualiser rendering**: CPU pixel buffer (simpler) vs OpenGL shader (better). Start CPU, upgrade in Phase 5 based on performance.
2. **Grain interpolation**: Linear (fast) vs Sinc (quality). Offer as a "Quality" toggle — Linear for performance, Sinc for render/export.
3. **Polyphony cap**: 16 voices × density up to 200 = potentially 3200 simultaneous grains. Profile early and set a hard grain cap (e.g. 256 total across all voices).
4. **FX slot order**: Fixed left-to-right, or fully drag-reorderable? Drag is nicer UX but more complex to implement.
5. **Sample rate support**: 44.1k, 48k, 88.2k, 96k — all handled via `prepareToPlay`. Grain size in samples must be recalculated on rate change.
