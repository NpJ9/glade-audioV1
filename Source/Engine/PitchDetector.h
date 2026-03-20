#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <vector>

//==============================================================================
// Header-only autocorrelation pitch detector.
// Analyses a window from the middle of the sample (steady-state region)
// and returns the detected MIDI note (0-127), or -1 if no clear pitch found.
//
// Uses normalised autocorrelation with parabolic interpolation for
// sub-sample accuracy. Confidence threshold: 0.4 (empirically tuned).
//==============================================================================
namespace PitchDetector
{
    inline int detectMidiNote (const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        const int totalSamples = buffer.getNumSamples();
        if (totalSamples < 512 || sampleRate <= 0.0)
            return -1;

        // ── Analysis window: middle 50% of sample, capped at 4096 samples ─────
        const int windowSize  = juce::jmin (totalSamples, 4096);
        const int startSample = (totalSamples - windowSize) / 2;

        // ── Downmix to mono ───────────────────────────────────────────────────
        const int numCh = buffer.getNumChannels();
        std::vector<float> mono (windowSize, 0.f);
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* data = buffer.getReadPointer (ch, startSample);
            for (int i = 0; i < windowSize; ++i)
                mono[i] += data[i] / (float) numCh;
        }

        // ── Signal power check (skip silent / near-silent samples) ────────────
        float power = 0.f;
        for (int i = 0; i < windowSize; ++i)
            power += mono[i] * mono[i];
        if (power < 1e-6f)
            return -1;

        // ── Autocorrelation search: 50 Hz – 2000 Hz ───────────────────────────
        const int minLag = juce::jmax (1,              (int) (sampleRate / 2000.0));
        const int maxLag = juce::jmin (windowSize / 2, (int) (sampleRate / 50.0));

        // Pearson correlation: normalises by the power of the overlapping segment
        // only, not the full window.  The biased form (c / power) undervalues long
        // lags because it divides a shorter sum by the full-window denominator,
        // creating a systematic preference for higher-octave candidates.
        auto normCorr = [&] (int lag) -> float
        {
            float c = 0.f, p0 = 0.f, p1 = 0.f;
            const int n = windowSize - lag;
            for (int i = 0; i < n; ++i)
            {
                c  += mono[i] * mono[i + lag];
                p0 += mono[i] * mono[i];
                p1 += mono[i + lag] * mono[i + lag];
            }
            const float denom = std::sqrt (p0 * p1);
            return (denom > 1e-9f) ? c / denom : 0.f;
        };

        float bestCorr = 0.f;
        int   bestLag  = 0;
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            const float c = normCorr (lag);
            if (c > bestCorr) { bestCorr = c; bestLag = lag; }
        }

        // Reject if confidence is too low (unpitched material / noise)
        if (bestCorr < 0.4f || bestLag == 0)
            return -1;

        // ── Parabolic interpolation for sub-sample lag accuracy ───────────────
        float refinedLag = (float) bestLag;
        if (bestLag > minLag && bestLag < maxLag)
        {
            const float y0 = normCorr (bestLag - 1);
            const float y1 = bestCorr;
            const float y2 = normCorr (bestLag + 1);
            const float denom = 2.f * y1 - y0 - y2;
            if (std::abs (denom) > 1e-6f)
                refinedLag = (float) bestLag + 0.5f * (y2 - y0) / denom;
        }

        // ── Convert lag → Hz → MIDI note ──────────────────────────────────────
        const float freq = (float) (sampleRate / (double) refinedLag);
        const float midi = 69.f + 12.f * std::log2f (freq / 440.f);
        return juce::jlimit (0, 127, (int) std::round (midi));
    }

    // Utility: MIDI note number → human-readable string, e.g. 60 → "C4"
    inline juce::String midiNoteToName (int note)
    {
        if (note < 0)
            return "?";
        static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        const int octave = note / 12 - 1;
        return juce::String (names[note % 12]) + juce::String (octave);
    }
}
