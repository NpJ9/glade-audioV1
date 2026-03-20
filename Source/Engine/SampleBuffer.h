#pragma once
#include <JuceHeader.h>
#include <atomic>

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
    const juce::AudioBuffer<float>& getBuffer() const
    {
        return buffers[readIdx.load (std::memory_order_acquire)];
    }
    bool hasContent() const { return getBuffer().getNumSamples() > 0; }

    double getOriginalSampleRate() const { return originalSampleRate; }

private:
    // Double-buffered to allow wait-free audio-thread reads while loadFile() runs
    // on the message thread.  loadFile() writes to the inactive slot then flips
    // readIdx atomically — the audio thread always reads from the current readIdx.
    juce::AudioBuffer<float> buffers[2];
    std::atomic<int>         readIdx { 0 };
    double originalSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleBuffer)
};
