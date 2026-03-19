#pragma once
#include <JuceHeader.h>
#include "Grain.h"
#include <array>

class GrainPool
{
public:
    static constexpr int maxGrains = 256;

    // Activate the next free grain with the given parameters.
    // Returns false if pool is full.
    bool activateGrain (double sourceStartPos,
                        int    grainLengthSamples,
                        double pitchRatio,
                        float  panL,
                        float  panR,
                        float  amplitude,
                        WindowType windowType);

    // Process all active grains into output, reading from source.
    // output must already be zeroed before calling.
    void process (juce::AudioBuffer<float>& output,
                  const juce::AudioBuffer<float>& source);

    int  getActiveCount() const;
    void reset();

private:
    std::array<Grain, maxGrains> grains;
};
