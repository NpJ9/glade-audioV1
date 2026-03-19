#pragma once
#include "SampleBuffer.h"
#include "GrainPool.h"
#include "GrainScheduler.h"
#include "PitchDetector.h"
#include "EnvelopeFollower.h"
#include "StepSequencer.h"
#include <JuceHeader.h>

class GranularEngine
{
public:
    GranularEngine() = default;

    void prepare (double sampleRate, int maxBlockSize);

    void process (juce::AudioBuffer<float>& output,
                  juce::MidiBuffer&         midi,
                  juce::AudioProcessorValueTreeState& apvts,
                  double bpm);

    // Load a sample file — call from message thread only.
    bool loadSample (const juce::File& file);

    bool isReady() const { return sampleBuffer.hasContent(); }

    int  getActiveGrainCount() const { return grainPool.getActiveCount(); }

    const juce::AudioBuffer<float>& getSampleBuffer() const { return sampleBuffer.getBuffer(); }

    // MIDI note currently held (-1 = none)
    int  getCurrentMidiNote()    const { return currentMidiNote.load(); }

    // Detected root note of the loaded sample
    int  getDetectedRootNote()   const { return detectedRootNote.load(); }

    // LFO state for UI visualisation (safe to read from message thread)
    float getLfoPhase()       const { return lfoPhaseAtomic.load();       }
    float getLfoOutput()      const { return lfoOutputAtomic.load();      }
    float getLfoPhase2()      const { return lfoPhase2Atomic.load();      }
    float getLfoOutput2()     const { return lfoOutput2Atomic.load();     }
    float getLfoPhase3()      const { return lfoPhase3Atomic.load();      }
    float getLfoOutput3()     const { return lfoOutput3Atomic.load();     }
    float getEnvFollowValue() const { return envFollowValueAtomic.load(); }
    int   getSeqCurrentStep() const { return stepSequencer.getCurrentStep(); }

    // Final modulated grain position (after all LFO/env/seq modulation)
    float getModulatedPosition() const { return modulatedPositionAtomic.load(); }

private:
    double sampleRate  = 44100.0;

    SampleBuffer   sampleBuffer;
    GrainPool      grainPool;
    GrainScheduler scheduler;

    // MIDI state
    std::atomic<int>  currentMidiNote    { -1 };
    int               lastActiveMidiNote { 60 };

    // Detected root note (updated on load, used for pitch ratio)
    std::atomic<int>  detectedRootNote   { 60 };

    // ADSR envelope
    juce::ADSR adsr;

    // LFO state (3 independent LFOs)
    double lfoPhase    = 0.0,  lfoPhase2    = 0.0,  lfoPhase3    = 0.0;
    float  lfoLastS_H  = 0.f,  lfoLastS_H2  = 0.f,  lfoLastS_H3  = 0.f;
    juce::Random lfoRandom;

    // Exported to UI thread via atomics
    std::atomic<float> lfoPhaseAtomic           { 0.f };
    std::atomic<float> lfoOutputAtomic          { 0.f };
    std::atomic<float> lfoPhase2Atomic          { 0.f };
    std::atomic<float> lfoOutput2Atomic         { 0.f };
    std::atomic<float> lfoPhase3Atomic          { 0.f };
    std::atomic<float> lfoOutput3Atomic         { 0.f };
    std::atomic<float> envFollowValueAtomic     { 0.f };
    std::atomic<float> modulatedPositionAtomic  { 0.5f };

    // Envelope follower
    EnvelopeFollower envFollower;

    // Step sequencer
    StepSequencer stepSequencer;

    float  calcLFO (float rate, int shape, int numSamples,
                    double& phase, float& lastSH) noexcept;

    // Dry/wet scratch buffer
    juce::AudioBuffer<float> wetBuffer;

    void handleMidi (juce::MidiBuffer& midi);
    double midiNoteToPitchRatio (int note) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranularEngine)
};
