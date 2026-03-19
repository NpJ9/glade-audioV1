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

        buffer = std::move (resampled);
    }
    else
    {
        buffer = std::move (rawBuffer);
    }

    return true;
}
