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
                               int        rootNote,
                               float      reverseAmount,
                               float      velocityScale)
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

            // Amplitude normalization — the correct formula depends on whether
            // grains are coherent (same source position → same waveform) or
            // incoherent (spread across the sample → random phase).
            //
            // Incoherent (high jitter): 1/sqrt(N) — constant-power summation.
            //   N coherent grains each at 1/sqrt(N) → sum peaks at sqrt(N) → CLIPS.
            //
            // Coherent (jitter ≈ 0): 1/N — constant-amplitude summation.
            //   N coherent grains each at 1/N → sum peaks at 1.0 → safe.
            //
            // The SEQ at a fixed step with low posJitter is the coherent case.
            // We crossfade between the two normalizations using positionJitter
            // as the blend factor (fully coherent at 0, fully incoherent at 0.125+).
            const float expectedOverlap = juce::jmax (1.f,
                (float) densityClamped * grainSizeMs * 0.001f);
            const float coherentAmp     = 1.0f / expectedOverlap;
            const float incoherentAmp   = 1.0f / std::sqrt (expectedOverlap);
            const float jitterBlend     = juce::jlimit (0.f, 1.f, positionJitter * 8.f);
            const float amplitude       = (coherentAmp   * (1.f - jitterBlend)
                                        + incoherentAmp * jitterBlend)
                                        * juce::jlimit (0.f, 1.f, velocityScale);

            // Decide per-grain whether to reverse based on reverseAmount probability.
            // reverseAmount 0 = all forward, 1 = all backward, 0.5 = 50/50 random mix.
            const bool isReverse = ((float) random.nextDouble() < reverseAmount);

            pool.activateGrain (startPos, grainLengthSamples, finalPitchRatio,
                                panL, panR, amplitude, windowType, isReverse);

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
