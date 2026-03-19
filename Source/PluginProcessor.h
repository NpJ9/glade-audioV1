#pragma once

#include <JuceHeader.h>
#include "Engine/GranularEngine.h"
#include "Engine/FXChain.h"
#include "Engine/PresetManager.h"

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
    // Parameter tree — all knobs/controls live here, DAW automation-ready
    juce::AudioProcessorValueTreeState apvts;

    // Current host BPM (updated each processBlock)
    double currentBpm { 120.0 };

    // Output RMS for the visualizer (updated each processBlock, read on message thread)
    std::atomic<float> outputRms { 0.f };

    // Pre-allocated dry buffer for dry/wet blending (no audio-thread alloc)
    juce::AudioBuffer<float> dryBuffer;

    GranularEngine             granularEngine;
    std::unique_ptr<FXChain>   fxChain;
    PresetManager              presetManager;

    // Load a sample — call from message thread.
    bool loadSample (const juce::File& file)
    {
        const bool ok = granularEngine.loadSample (file);
        if (ok) lastLoadedFile = file;
        return ok;
    }

    juce::File lastLoadedFile;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GladeAudioProcessor)
};
