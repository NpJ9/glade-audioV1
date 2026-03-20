#include "SampleBuffer.h"

bool SampleBuffer::loadFile (const juce::File& file, double targetSampleRate)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
        return false;

    originalSampleRate = reader->sampleRate;

    // Read raw audio into a temporary buffer
    juce::AudioBuffer<float> rawBuffer ((int) reader->numChannels,
                                        (int) reader->lengthInSamples);
    reader->read (&rawBuffer, 0, (int) reader->lengthInSamples, 0, true, true);

    // Ensure stereo output
    if (rawBuffer.getNumChannels() == 1)
    {
        juce::AudioBuffer<float> stereo (2, rawBuffer.getNumSamples());
        stereo.copyFrom (0, 0, rawBuffer, 0, 0, rawBuffer.getNumSamples());
        stereo.copyFrom (1, 0, rawBuffer, 0, 0, rawBuffer.getNumSamples());
        rawBuffer = std::move (stereo);
    }

    // Resample if needed
    juce::AudioBuffer<float> result;
    if (std::abs (originalSampleRate - targetSampleRate) > 1.0)
    {
        const double ratio = targetSampleRate / originalSampleRate;
        const int newLength = juce::roundToInt ((double) rawBuffer.getNumSamples() * ratio);

        juce::AudioBuffer<float> resampled (rawBuffer.getNumChannels(), newLength);

        for (int ch = 0; ch < rawBuffer.getNumChannels(); ++ch)
        {
            juce::LagrangeInterpolator resampler;
            resampler.process (originalSampleRate / targetSampleRate,
                               rawBuffer.getReadPointer (ch),
                               resampled.getWritePointer (ch),
                               newLength);
        }

        result = std::move (resampled);
    }
    else
    {
        result = std::move (rawBuffer);
    }

    // Write into the inactive slot then flip the read index atomically.
    // The audio thread always reads from buffers[readIdx] — no lock needed.
    // Peak memory: the old buffer stays alive until the next load overwrites it.
    const int writeIdx = 1 - readIdx.load (std::memory_order_acquire);
    buffers[writeIdx] = std::move (result);
    readIdx.store (writeIdx, std::memory_order_release);

    return true;
}
