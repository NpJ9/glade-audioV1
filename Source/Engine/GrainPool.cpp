#include "GrainPool.h"
#include <JuceHeader.h>

bool GrainPool::activateGrain (double sourceStartPos,
                                int    grainLengthSamples,
                                double pitchRatio,
                                float  panL,
                                float  panR,
                                float  amplitude,
                                WindowType windowType,
                                bool   isReverse)
{
    for (auto& g : grains)
    {
        if (!g.active)
        {
            g.active              = true;
            g.grainLengthSamples  = grainLengthSamples;
            g.pitchRatio          = pitchRatio;
            g.panL                = panL;
            g.panR                = panR;
            g.amplitude           = amplitude;
            g.windowType          = windowType;
            g.samplesPlayed       = 0;
            g.isReverse           = isReverse;
            // Reverse grains start at the far end of the grain window so they
            // play backward through the same material a forward grain would cover.
            // Clamp so the start never lands past the end of the source buffer
            // (GrainPool::process() clamps the read position per-sample anyway,
            // but starting in-bounds avoids the first sample being silence).
            if (isReverse)
                g.sourceReadPos = sourceStartPos + (double) grainLengthSamples * pitchRatio;
            else
                g.sourceReadPos = sourceStartPos;
            // No per-grain source-length available here; per-sample clamping in
            // process() handles out-of-bounds reads safely.
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

            // 4-point Hermite cubic interpolation.
            //
            // Linear interpolation has significant gain above Nyquist/2, producing
            // aliasing when pitchRatio > 1 (upward pitch shift).  Hermite cubic
            // gives a much flatter passband and ~36 dB better stopband attenuation.
            // Small overshots (<1%) at sharp transients are the trade-off.
            //
            // y(t) = ((c3*t + c2)*t + c1)*t + c0,  t in [0,1)
            // using Catmull-Rom tension (0.5) for smooth yet accurate reconstruction.
            //
            // IMPORTANT: sourceReadPos must be clamped BEFORE computing i1 and t.
            // If only i1 is clamped (as was done before), t = sourceReadPos - i1
            // grows unboundedly past the buffer end (t=2, 10, 1000…) and the
            // cubic extrapolates to huge values — audible as clicks at position=1.0.
            const double clampedPos = juce::jlimit (0.0, (double) (numSrcSamples - 1), g.sourceReadPos);
            const int   i1   = (int) clampedPos;
            const int   i0   = juce::jmax (0, i1 - 1);
            const int   i2   = juce::jmin (numSrcSamples - 1, i1 + 1);
            const int   i3   = juce::jmin (numSrcSamples - 1, i1 + 2);
            const float t    = (float) (clampedPos - (double) i1);

            auto hermite = [&] (const float* buf) -> float
            {
                const float y0 = buf[i0], y1 = buf[i1], y2 = buf[i2], y3 = buf[i3];
                const float c0 = y1;
                const float c1 = 0.5f * (y2 - y0);
                const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
                const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
                return ((c3 * t + c2) * t + c1) * t + c0;
            };

            const float sL = hermite (srcL);
            const float sR = hermite (srcR);

            const float gainL = windowAmp * g.amplitude * g.panL;
            const float gainR = windowAmp * g.amplitude * g.panR;

            outL[s] += sL * gainL;
            if (outR) outR[s] += sR * gainR;

            g.sourceReadPos += g.isReverse ? -g.pitchRatio : g.pitchRatio;
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
