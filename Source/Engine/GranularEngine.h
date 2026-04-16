#pragma once
#include "SampleBuffer.h"
#include "GrainPool.h"
#include "GrainScheduler.h"
#include "PitchDetector.h"
#include "StepSequencer.h"
#include "GrainParams.h"
#include "LFOFunction.h"
#include <JuceHeader.h>

class GranularEngine
{
public:
    GranularEngine() = default;

    void prepare (double sampleRate, int maxBlockSize);
    void releaseResources();

    /** Main audio callback.  Receives a pre-built GrainParams snapshot rather
     *  than a live APVTS reference — the engine has no knowledge of JUCE's
     *  parameter system and can be driven from unit tests with plain structs. */
    void process (juce::AudioBuffer<float>& output,
                  juce::MidiBuffer&         midi,
                  const GrainParams&        params);

    // Load a sample file — call from message thread only.
    bool loadSample (const juce::File& file);

    bool isReady()            const { return sampleBuffer.hasContent(); }
    int  getActiveGrainCount() const { return grainPool.getActiveCount(); }

    const juce::AudioBuffer<float>& getSampleBuffer() const
    {
        return sampleBuffer.getBuffer();
    }

    // MIDI note currently held (-1 = none)
    int  getCurrentMidiNote()  const { return currentMidiNote.load(); }

    // Detected root note of the loaded sample
    int  getDetectedRootNote() const { return detectedRootNote.load(); }

    // LFO state for UI visualisation (safe to read from message thread)
    float getLfoPhase()        const { return lfoPhaseAtomic.load();       }
    float getLfoOutput()       const { return lfoOutputAtomic.load();      }
    float getLfoPhase2()       const { return lfoPhase2Atomic.load();      }
    float getLfoOutput2()      const { return lfoOutput2Atomic.load();     }
    float getLfoPhase3()       const { return lfoPhase3Atomic.load();      }
    float getLfoOutput3()      const { return lfoOutput3Atomic.load();     }
    int   getSeqCurrentStep()  const { return stepSequencer.getCurrentStep(); }

    // Final modulated grain position (after all LFO / env / seq modulation)
    float getModulatedPosition() const { return modulatedPositionAtomic.load(); }

    /** ADSR cursor: 0..1 position along the A/D/S/R shape for UI display.
     *  Encoding: 0–0.25 = attack, 0.25–0.5 = decay, 0.5 = sustain held,
     *            0.75–1.0 = release.  Returns -1 when the envelope is idle. */
    float getAdsrCursor() const { return adsrCursorAtomic.load(); }

private:
    double sampleRate = 44100.0;

    SampleBuffer   sampleBuffer;
    GrainPool      grainPool;
    GrainScheduler scheduler;

    // MIDI state
    std::atomic<int>  currentMidiNote    { -1 };
    int               lastActiveMidiNote { 60 };

    // Last-note-priority monophonic note stack (audio-thread only, no atomics needed)
    static constexpr int kNoteStackSize = 16;
    std::array<int, kNoteStackSize> noteStack {};
    int noteStackSize = 0;

    // Detected root note (updated on load, used for pitch ratio)
    std::atomic<int>  detectedRootNote   { 60 };

    // ADSR envelope
    juce::ADSR adsr;

    // LFO state (3 independent LFOs).  lfoSH* = last sample-and-hold value.
    double lfoPhase  = 0.0,  lfoPhase2  = 0.0,  lfoPhase3  = 0.0;
    float  lfoSH1    = 0.f,  lfoSH2     = 0.f,  lfoSH3     = 0.f;
    juce::Random lfoRandom;

    // Exported to UI thread via atomics
    std::atomic<float> lfoPhaseAtomic          { 0.f };
    std::atomic<float> lfoOutputAtomic         { 0.f };
    std::atomic<float> lfoPhase2Atomic         { 0.f };
    std::atomic<float> lfoOutput2Atomic        { 0.f };
    std::atomic<float> lfoPhase3Atomic         { 0.f };
    std::atomic<float> lfoOutput3Atomic        { 0.f };
    std::atomic<float> modulatedPositionAtomic { 0.5f };
    std::atomic<float> adsrCursorAtomic        { -1.f };  // -1 = idle/hidden

    // ADSR visualisation state (audio thread only — no atomics needed for these)
    enum class AdsrVizStage { Idle, Attack, Decay, Sustain, Release };
    AdsrVizStage adsrVizStage   { AdsrVizStage::Idle };
    double       adsrVizTimeSec { 0.0 };
    bool         adsrVizNoteOn  { false };   // set by handleMidi, consumed by process()
    bool         adsrVizNoteOff { false };

    // Step sequencer
    StepSequencer stepSequencer;

    /** Advance one LFO phase and return the output sample.
     *  Delegates to LFOFunction::evaluate for the waveform shape so audio and
     *  UI always compute identical values from the same function. */
    float calcLFO (float rate, int shape, int numSamples,
                   double& phase, float& lastSH) noexcept;

    // Set by loadSample() (message thread) so the audio thread can safely reset
    // the pool and scheduler at the top of the next process() call, rather than
    // having the message thread reach into audio-thread-owned objects directly.
    std::atomic<bool> resetRequested { false };

    // Wet-signal scratch buffer; sized to maxBlockSize in prepare()
    juce::AudioBuffer<float> wetBuffer;

    // Smoothed output gain — prevents zipper noise on gain automation
    juce::LinearSmoothedValue<float> outputGainSmoothed;

    void   handleMidi (juce::MidiBuffer& midi);
    double midiNoteToPitchRatio (int note) const;

    // ── Feature state (audio thread only — no atomics needed) ─────────────────
    float  lastVelocity      { 1.0f };   // latest noteOn velocity (0–1)
    float  frozenPosition    { 0.5f };   // position snapshot for FREEZE
    bool   prevFreeze        { false };  // edge-detect freeze activation
    double glidePitchRatio   { 1.0 };   // current smoothed MIDI pitch ratio

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranularEngine)
};
