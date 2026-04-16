#pragma once
#include <cmath>

/** Stateless LFO waveform evaluator shared by the audio engine and LFODisplay.
 *
 *  All LFO shape logic lives here exactly once.  GranularEngine::calcLFO
 *  and LFODisplay::evaluateLFO both delegate here — the audio and visual
 *  representations can never silently diverge when a new shape is added.
 *
 *  shape: matches the lfoShape APVTS choice index
 *    0 = Sine, 1 = Triangle, 2 = Saw (-1→+1), 3 = Square, 4 = S&H
 *    5 = Ramp Up (0→1 unipolar), 6 = Ramp Down (1→0 unipolar)
 *  phase: 0..1 normalised cycle position
 *  Returns: -1..1 for bipolar shapes; 0..1 for unipolar shapes (5, 6).
 *  S&H (4) returns 0 here — the caller (calcLFO) maintains the held value. */
namespace LFOFunction
{
    [[nodiscard]] inline float evaluate (int shape, float phase) noexcept
    {
        switch (shape)
        {
            case 0: return std::sin (phase * 6.28318530f);          // Sine
            case 1: return 1.f - 4.f * std::abs (phase - 0.5f);    // Triangle
            case 2: return phase * 2.f - 1.f;                       // Saw  (-1→+1)
            case 3: return phase < 0.5f ? 1.f : -1.f;              // Square
            case 5: return phase;                                    // Ramp Up   (0→1)
            case 6: return 1.f - phase;                             // Ramp Down (1→0)
            default: return 0.f;   // S&H (4) — static; phase meaningless
        }
    }
}
