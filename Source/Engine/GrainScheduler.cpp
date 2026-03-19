#include "GrainScheduler.h"
#include "ScaleQuantizer.h"
#include <cmath>

void GrainScheduler::process (int        numSamples,
                               double     sampleRate,
                               GrainPool& pool,
                               int        numSourceSamples,
                               float      grainSizeMs,
                               float      density,
                               float      position,
                               float      positionJitter,
                               double     pitchRatio,
                               float      pitchJitter,
                               float      panSpread,
                               WindowType windowType,
                               bool       isActive,
                               int        scaleIdx,
                               int        rootNote)
{
    if (!isActive || numSourceSamples == 0) return;

    // Interval between grain births in samples
    const double densityClamped = juce::jlimit (0.5f, 500.f, density);
    const double intervalSamples = sampleRate / (double) densityClamped;

    const int grainLengthSamples = juce::jlimit (
        1,
        (int) (sampleRate * 5.0), // max 5 s
        (int) (grainSizeMs * 0.001 * sampleRate));

    // Advance clock; spawn one grain per interval that falls within this block
    double sampleClock = 0.0;

    while (sampleClock < (double) numSamples)
    {
        if (samplesUntilNextGrain <= 0.0)
        {
            // ── Position ─────────────────────────────────────────────────────
            double jitterRange = positionJitter * (double) numSourceSamples * 0.5;
            double startPos    = position * (double) (numSourceSamples - 1)
                                 + random.nextDouble() * 2.0 * jitterRange - jitterRange;
            startPos = juce::jlimit (0.0, (double) (numSourceSamples - 1), startPos);

            // ── Pitch ─────────────────────────────────────────────────────────
            float rawSemitones = pitchJitter * 24.0f * ((float) random.nextDouble() - 0.5f);
            const float pitchJitterSemitones = ScaleQuantizer::quantize (rawSemitones, scaleIdx, rootNote);
            const double finalPitchRatio = pitchRatio
                                           * std::pow (2.0, (double) pitchJitterSemitones / 12.0);

            // ── Pan ───────────────────────────────────────────────────────────
            const float panRandom = panSpread * ((float) random.nextDouble() * 2.0f - 1.0f);
            float panL, panR;
            calcPan (panRandom, panL, panR);

            pool.activateGrain (startPos, grainLengthSamples, finalPitchRatio,
                                panL, panR, 1.0f, windowType);

            // Reset countdown with slight jitter on the interval itself (±5%)
            const double intervalJitter = intervalSamples * 0.05 * (random.nextDouble() * 2.0 - 1.0);
            samplesUntilNextGrain = intervalSamples + intervalJitter;
        }

        const double step = juce::jmin (samplesUntilNextGrain, (double) numSamples - sampleClock);
        sampleClock              += step;
        samplesUntilNextGrain    -= step;
    }
}

void GrainScheduler::calcPan (float pan, float& outL, float& outR)
{
    // Equal-power panning: pan -1=full left, 0=centre, +1=full right
    const float angle = (pan + 1.0f) * 0.5f * 1.5707963f; // 0 to pi/2
    outL = std::cos (angle);
    outR = std::sin (angle);
}
