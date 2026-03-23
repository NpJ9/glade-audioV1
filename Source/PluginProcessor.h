#pragma once

#include <JuceHeader.h>
#include "Engine/GranularEngine.h"
#include "Engine/FXChain.h"
#include "Engine/GrainParams.h"

//==============================================================================
/** Snapshot of runtime engine state, safe to read from the message thread.
 *  GladeAudioProcessor::getState() fills this struct by loading the engine's
 *  atomics.  The editor uses only this struct rather than accessing the engine
 *  directly — the engine's internal layout is an implementation detail. */
struct PluginState
{
    // LFO visualisation (phase 0..1, output -1..1)
    float lfoPhase[3]  = {};
    float lfoOutput[3] = {};

    // Modulation
    float envFollowValue    = 0.f;
    float modulatedPosition = 0.5f;

    // MIDI / pitch
    int currentMidiNote  = -1;
    int detectedRootNote = 60;

    // Sequencer
    int seqCurrentStep = -1;

    // Grain cloud
    int activeGrainCount = 0;

    // Output
    float outputRms = 0.f;

    // Sample loaded?
    bool sampleLoaded = false;
};

//==============================================================================
class GladeAudioProcessor : public juce::AudioProcessor
{
public:
    GladeAudioProcessor();
    ~GladeAudioProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==========================================================================
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    //==========================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Parameter tree — all knobs/controls live here, DAW automation-ready.
    // The editor accesses this directly via APVTS attachments.
    juce::AudioProcessorValueTreeState apvts;

    // Current host BPM (updated each processBlock)
    double currentBpm { 120.0 };

    // Output RMS for the visualizer (updated each processBlock, read on message thread)
    std::atomic<float> outputRms { 0.f };

    //==========================================================================
    /** Thread-safe snapshot of engine state for the editor's timer callback.
     *  Never accesses the engine directly — reads only pre-exported atomics. */
    PluginState getState() const noexcept;

    //==========================================================================
    // Sample management — call from message thread only.
    bool loadSample (const juce::File& file);
    bool isSampleLoaded() const { return granularEngine.isReady(); }
    const juce::AudioBuffer<float>& getSampleBuffer() const
    {
        return granularEngine.getSampleBuffer();
    }

    // Converts the detected root MIDI note to a human-readable string (e.g. "C4").
    // Safe to call from the message thread; reads only the atomic stored in getState().
    juce::String getRootNoteName() const;

    juce::File   lastLoadedFile;
    juce::String currentPresetName { "INIT" };

    //==========================================================================
    // Exposed for APVTS attachment construction in the editor and for
    // preset manager operations.  Not intended for audio-thread use.
    GranularEngine granularEngine;
    std::unique_ptr<FXChain> fxChain;

private:
    // Pre-allocated dry buffer for dry/wet blending (no audio-thread alloc)
    juce::AudioBuffer<float> dryBuffer;

    // Smoothed dry/wet — prevents zipper noise when dryWet is automated
    juce::LinearSmoothedValue<float> dryWetSmoothed;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    /** Read all APVTS parameters into a GrainParams struct.  All APVTS reads
     *  happen here, once per block, so the engine layer is decoupled. */
    GrainParams buildGrainParams() const;

    /** Read all FX slot parameters from APVTS into a flat array. */
    std::array<FXSlotParams, FXChain::numSlots> buildFXParams() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GladeAudioProcessor)
};
