#pragma once
#include <cmath>

/** ADSR-style envelope follower.
 *  Feed it audio RMS values each block; read back a smoothed 0-1 envelope.
 *  Behaviour:
 *   – Rising (input > envelope)                   → attack
 *   – Falling above sustain floor                  → decay toward max(susLevel, input)
 *   – Below sustain floor / signal gone            → release toward input
 *  All coefficients are computed from time constants in seconds. */
struct EnvelopeFollower
{
    float envelope = 0.f;

    /** Process one RMS value per block with full ADSR shaping.
     *  susLevel: 0-1 sustain floor (fraction of the peak).
     *  Pre-compute coefficients with makeCoeff() to avoid repeated exp() calls. */
    void process (float inputRms, float attackCoeff, float decayCoeff,
                  float susLevel, float releaseCoeff) noexcept
    {
        const float floor = susLevel * inputRms;   // sustain = fraction of live signal

        if (inputRms > envelope)
            envelope += attackCoeff  * (inputRms - envelope);
        else if (envelope > floor)
            envelope += decayCoeff   * (floor - envelope);   // decay toward sustain floor
        else
            envelope += releaseCoeff * (inputRms - envelope); // release to input level

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
