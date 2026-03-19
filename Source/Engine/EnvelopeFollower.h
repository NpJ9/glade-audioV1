#pragma once
#include <cmath>

/** Simple one-pole attack/release envelope follower.
 *  Feed it audio RMS values each block; read back a smoothed 0-1 envelope.
 *  All coefficients are computed from time constants in seconds. */
struct EnvelopeFollower
{
    float envelope = 0.f;

    /** Process one RMS value per block.
     *  attackCoeff / releaseCoeff = 1 - exp(-1 / (timeSec * sampleRate / blockSize))
     *  Pre-compute these once per block to avoid repeated exp() calls. */
    void process (float inputRms, float attackCoeff, float releaseCoeff) noexcept
    {
        if (inputRms > envelope)
            envelope += attackCoeff  * (inputRms - envelope);
        else
            envelope += releaseCoeff * (inputRms - envelope);

        // Clamp to avoid denormals
        if (envelope < 1e-7f) envelope = 0.f;
    }

    float getValue() const noexcept { return envelope; }

    void reset() noexcept { envelope = 0.f; }

    /** Compute a one-pole smoothing coefficient from a time constant.
     *  timeSec: time in seconds, sampleRate, blockSize: samples per block. */
    static float makeCoeff (float timeSec, double sampleRate, int blockSize) noexcept
    {
        if (timeSec < 0.0001f) return 1.f;
        return 1.f - std::exp (-1.f / (timeSec * (float)(sampleRate / (double)blockSize)));
    }
};
