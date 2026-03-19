#include "GrainPool.h"
#include <JuceHeader.h>

bool GrainPool::activateGrain (double sourceStartPos,
                                int    grainLengthSamples,
                                double pitchRatio,
                                float  panL,
                                float  panR,
                                float  amplitude,
                                WindowType windowType)
{
    for (auto& g : grains)
    {
        if (!g.active)
        {
            g.active              = true;
            g.sourceReadPos       = sourceStartPos;
            g.grainLengthSamples  = grainLengthSamples;
            g.pitchRatio          = pitchRatio;
            g.panL                = panL;
            g.panR                = panR;
            g.amplitude           = amplitude;
            g.windowType          = windowType;
            g.samplesPlayed       = 0;
            return true;
        }
    }
    return false; // pool full
}

void GrainPool::process (juce::AudioBuffer<float>& output,
                         const juce::AudioBuffer<float>& source)
{
    const int numOutSamples   = output.getNumSamples();
    const int numSrcSamples   = source.getNumSamples();
    const int numSrcChannels  = source.getNumChannels();

    if (numSrcSamples == 0) return;

    auto* outL = output.getWritePointer (0);
    auto* outR = output.getNumChannels() > 1 ? output.getWritePointer (1) : nullptr;

    const auto* srcL = source.getReadPointer (0);
    const auto* srcR = numSrcChannels > 1 ? source.getReadPointer (1) : srcL;

    for (auto& g : grains)
    {
        if (!g.active) continue;

        for (int s = 0; s < numOutSamples; ++s)
        {
            if (g.samplesPlayed >= g.grainLengthSamples)
            {
                g.active = false;
                break;
            }

            // Window envelope
            const float phase = (float) g.samplesPlayed / (float) g.grainLengthSamples;
            const float windowAmp = WindowFunction::get (g.windowType, phase);

            // Linear interpolation into source
            const int   idx0 = juce::jlimit (0, numSrcSamples - 1, (int) g.sourceReadPos);
            const int   idx1 = juce::jlimit (0, numSrcSamples - 1, idx0 + 1);
            const float frac = (float) (g.sourceReadPos - (double) idx0);

            const float sL = srcL[idx0] + frac * (srcL[idx1] - srcL[idx0]);
            const float sR = srcR[idx0] + frac * (srcR[idx1] - srcR[idx0]);

            const float gainL = windowAmp * g.amplitude * g.panL;
            const float gainR = windowAmp * g.amplitude * g.panR;

            outL[s] += sL * gainL;
            if (outR) outR[s] += sR * gainR;

            g.sourceReadPos += g.pitchRatio;
            ++g.samplesPlayed;
        }
    }
}

int GrainPool::getActiveCount() const
{
    int count = 0;
    for (const auto& g : grains)
        if (g.active) ++count;
    return count;
}

void GrainPool::reset()
{
    for (auto& g : grains)
        g.reset();
}
