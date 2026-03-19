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
                return 0.5f * (1.0f - std::cos (6.283185307f * phase));

            case WindowType::Gaussian:
            {
                // Centre the gaussian at 0.5, sigma ~0.18 so it touches 0 at edges
                const float x = phase - 0.5f;
                return std::exp (-x * x / (2.0f * 0.18f * 0.18f));
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
