#pragma once
#include <atomic>
#include <cmath>

/** 8-step position sequencer.
 *  Call process() each audio block; it advances by beat division and returns
 *  the current step's position value (0-1), or -1.f if inactive.
 *  Reads step values as raw floats (passed in from APVTS each block). */
class StepSequencer
{
public:
    static constexpr int kNumSteps = 8;

    /** Process one audio block.
     *  stepValues: array of kNumSteps floats (0-1), read from APVTS.
     *  divIdx: beat division index (0-8, same table as GranularEngine beat sync).
     *  Returns the current step position (0-1) if active, or -1.f if not active / bpm==0. */
    float process (const float* stepValues, int numSamples,
                   double sampleRate, double bpm, int divIdx, bool active) noexcept
    {
        if (!active || bpm <= 0.0) return -1.f;

        // Same multiplier table as GranularEngine beat sync
        static constexpr double mults[] =
            { 0.125, 1.0/6.0, 0.25, 1.0/3.0, 0.5, 2.0/3.0, 1.0, 2.0, 4.0 };
        const int idx = (divIdx < 0) ? 0 : (divIdx > 8 ? 8 : divIdx);
        const double stepDurationSec = (60.0 / bpm) * mults[idx];
        const double stepDurationSamples = stepDurationSec * sampleRate;

        samplesUntilAdvance -= (double) numSamples;
        if (samplesUntilAdvance <= 0.0)
        {
            currentStep = (currentStep + 1) % kNumSteps;
            samplesUntilAdvance += stepDurationSamples;
            // Guard against drift if timer slipped multiple steps
            if (samplesUntilAdvance <= 0.0)
                samplesUntilAdvance = stepDurationSamples;
        }

        currentStepAtomic.store (currentStep, std::memory_order_relaxed);
        return stepValues[currentStep];
    }

    void reset() noexcept
    {
        currentStep = 0;
        samplesUntilAdvance = 0.0;
        currentStepAtomic.store (0, std::memory_order_relaxed);
    }

    int getCurrentStep() const noexcept
    {
        return currentStepAtomic.load (std::memory_order_relaxed);
    }

private:
    int    currentStep          = 0;
    double samplesUntilAdvance  = 0.0;
    std::atomic<int> currentStepAtomic { 0 };
};
