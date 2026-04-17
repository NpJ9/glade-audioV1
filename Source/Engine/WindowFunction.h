#pragma once
#include <cmath>
#include <array>

enum class WindowType { Gaussian, Hanning, Trapezoid, Rectangular };

namespace WindowFunction
{
    //==========================================================================
    // Pre-computed 1024-point LUT per window type.
    //
    // Calling std::sin or std::exp per sample per active grain is expensive at
    // high grain counts.  The LUT reduces that to two array lookups + one
    // multiply per sample.  Accuracy error vs the analytic formula is < 0.001
    // amplitude (< 0.01 dB), which is inaudible.
    //
    // kLutSize+1 entries so the linear interpolation at phase=1.0 can safely
    // read index [kLutSize] without a bounds check (value is always 0 there
    // for all envelope shapes).
    //==========================================================================
    static constexpr int kLutSize = 1024;

    namespace detail
    {
        // Analytic evaluation used only at LUT build time (not on the audio thread).
        inline float analytic (WindowType type, float phase)
        {
            switch (type)
            {
                case WindowType::Hanning:
                    return 0.5f * (1.0f - std::cos (6.28318530f * phase));

                case WindowType::Gaussian:
                {
                    static constexpr float kSigma    = 0.18f;
                    static constexpr float kVar2     = 2.0f * kSigma * kSigma;
                    static constexpr float kPedestal = 0.02140f;
                    const float x = phase - 0.5f;
                    const float g = std::exp (-x * x / kVar2);
                    return std::max (0.f, (g - kPedestal) / (1.0f - kPedestal));
                }

                case WindowType::Trapezoid:
                    if (phase < 0.1f)  return phase / 0.1f;
                    if (phase > 0.9f)  return (1.0f - phase) / 0.1f;
                    return 1.0f;

                default: return 1.0f;
            }
        }

        using Lut = std::array<float, kLutSize + 1>;

        inline Lut buildLut (WindowType type)
        {
            Lut t;
            for (int i = 0; i <= kLutSize; ++i)
                t[i] = analytic (type, (float) i / (float) kLutSize);
            return t;
        }
    }

    // Lookup-table evaluation with linear interpolation.
    // phase must be in [0, 1]; this is not bounds-checked for performance.
    // Rectangular is handled as a special case (constant 1.0, no table needed).
    inline float get (WindowType type, float phase) noexcept
    {
        if (type == WindowType::Rectangular)
            return 1.0f;

        // Static locals: constructed exactly once (C++11 thread-safe init).
        // The first call to get() initialises all three tables; subsequent calls
        // are just a pointer dereference + two array reads + one multiply.
        static const detail::Lut hannLut  = detail::buildLut (WindowType::Hanning);
        static const detail::Lut gaussLut = detail::buildLut (WindowType::Gaussian);
        static const detail::Lut trapLut  = detail::buildLut (WindowType::Trapezoid);

        const detail::Lut* lut = nullptr;
        switch (type)
        {
            case WindowType::Hanning:    lut = &hannLut;  break;
            case WindowType::Gaussian:   lut = &gaussLut; break;
            case WindowType::Trapezoid:  lut = &trapLut;  break;
            default: return 1.0f;
        }

        const float fi = phase * (float) kLutSize;
        const int   i0 = (int) fi;                        // guaranteed [0, kLutSize-1]
        const float fr = fi - (float) i0;
        return (*lut)[i0] + fr * ((*lut)[i0 + 1] - (*lut)[i0]);
    }
}
