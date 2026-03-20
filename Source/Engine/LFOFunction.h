#pragma once
#include <cmath>

/** Stateless LFO waveform evaluator shared by the audio engine and LFODisplay.
 *
 *  All LFO shape logic lives here exactly once.  GranularEngine::calcLFO
 *  and LFODisplay::evaluateLFO both delegate here — the audio and visual
 *  representations can never silently diverge when a new shape is added.
 *
 *  shape: matches the lfoShape APVTS choice index
 *    0 = Sine, 1 = Triangle, 2 = Saw, 3 = Square, 4 = S&H
 *  phase: 0..1 normalised cycle position
 *  Returns: -1..1  (S&H returns 0 — the caller maintains the held value) */
namespace LFOFunction
{
    [[nodiscard]] inline float evaluate (int shape, float phase) noexcept
    {
        switch (shape)
        {
            case 0: return std::sin (phase * 6.28318530f);          // Sine
            case 1: return 1.f - 4.f * std::abs (phase - 0.5f);    // Triangle
            case 2: return phase * 2.f - 1.f;                       // Saw
            case 3: return phase < 0.5f ? 1.f : -1.f;              // Square
            default: return 0.f;   // S&H — static; phase position meaningless
        }
    }
}
