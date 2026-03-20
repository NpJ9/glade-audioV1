#pragma once
#include <cmath>

enum class WindowType { Gaussian, Hanning, Trapezoid, Rectangular };

namespace WindowFunction
{
    // phase: 0.0 -> 1.0 over the grain lifetime
    // returns amplitude: 0.0 -> 1.0
    inline float get (WindowType type, float phase)
    {
        switch (type)
        {
            case WindowType::Hanning:
                return 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * phase));

            case WindowType::Gaussian:
            {
                // Raised-cosine Gaussian, centred at 0.5 (sigma = 0.18).
                //
                // A plain Gaussian never truly reaches 0; at the grain edges
                // (phase = 0 or 1) it evaluates to:
                //   exp(-0.25 / (2 * 0.18^2)) ≈ 0.0214
                //
                // That ~2.1% pedestal accumulates as DC when hundreds of grains
                // overlap.  Fix: subtract the pedestal value and renormalise so
                // the window is exactly 0 at the edges and 1.0 at the centre.
                static constexpr float kSigma    = 0.18f;
                static constexpr float kVar2     = 2.0f * kSigma * kSigma;          // 0.0648
                static constexpr float kPedestal = 0.02140f;  // exp(-0.25 / kVar2)

                const float x = phase - 0.5f;
                const float g = std::exp (-x * x / kVar2);
                return juce::jmax (0.f, (g - kPedestal) / (1.0f - kPedestal));
            }

            case WindowType::Trapezoid:
            {
                // 10% ramp up, 80% flat, 10% ramp down
                if (phase < 0.1f)  return phase / 0.1f;
                if (phase > 0.9f)  return (1.0f - phase) / 0.1f;
                return 1.0f;
            }

            case WindowType::Rectangular:
            default:
                return 1.0f;
        }
    }
}
