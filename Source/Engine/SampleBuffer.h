#pragma once
#include <JuceHeader.h>

// Holds the loaded audio sample and exposes it for grain reading.
// loadFile() can be called from the message thread; the audio thread
// reads via getBuffer() which returns the currently committed buffer.
class SampleBuffer
{
public:
    SampleBuffer() = default;

    // Call from message thread. Loads file, resamples to targetSampleRate.
    // Returns true on success.
    bool loadFile (const juce::File& file, double targetSampleRate);

    // Safe to call from audio thread.
    const juce::AudioBuffer<float>& getBuffer() const { return buffer; }
    bool hasContent() const { return buffer.getNumSamples() > 0; }

    double getOriginalSampleRate() const { return originalSampleRate; }

private:
    juce::AudioBuffer<float> buffer;
    double originalSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleBuffer)
};
