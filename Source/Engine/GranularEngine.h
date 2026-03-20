#pragma once
#include "SampleBuffer.h"
#include "GrainPool.h"
#include "GrainScheduler.h"
#include "PitchDetector.h"
#include "EnvelopeFollower.h"
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
    float getEnvFollowValue()  const { return envFollowValueAtomic.load(); }
    int   getSeqCurrentStep()  const { return stepSequencer.getCurrentStep(); }

    // Final modulated grain position (after all LFO / env / seq modulation)
    float getModulatedPosition() const { return modulatedPositionAtomic.load(); }

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
    std::atomic<float> envFollowValueAtomic    { 0.f };
    std::atomic<float> modulatedPositionAtomic { 0.5f };

    // Envelope follower
    EnvelopeFollower envFollower;

    // Step sequencer
    StepSequencer stepSequencer;

    /** Advance one LFO phase and return the output sample.
     *  Delegates to LFOFunction::evaluate for the waveform shape so audio and
     *  UI always compute identical values from the same function. */
    float calcLFO (float rate, int shape, int numSamples,
                   double& phase, float& lastSH) noexcept;

    // Wet-signal scratch buffer; sized to maxBlockSize in prepare()
    juce::AudioBuffer<float> wetBuffer;

    // Smoothed output gain — prevents zipper noise on gain automation
    juce::LinearSmoothedValue<float> outputGainSmoothed;

    void   handleMidi (juce::MidiBuffer& midi);
    double midiNoteToPitchRatio (int note) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranularEngine)
};
