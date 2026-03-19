#pragma once
#include <JuceHeader.h>
#include <cmath>

/** Double-buffered single-cycle wavetable generator.
 *  Call prepare() once, then generate() from the message thread whenever shape changes.
 *  The audio thread reads from getBuffer() — wait-free, no locks. */
class WavetableBuffer
{
public:
    static constexpr int kTableSize = 2048;

    // Shapes (match APVTS wavetableShape choice indices)
    enum Shape { Sine=0, Saw, Square, Triangle, Noise };

    void prepare (double sr)
    {
        sampleRate = sr;
        for (auto& b : buffers)
            b.setSize (2, kTableSize, false, true, false);
        generate (Sine);
    }

    /** Generate a new waveform. Call from the message thread only. */
    void generate (int shape)
    {
        const int writeIdx = 1 - readIdx.load (std::memory_order_acquire);
        auto& buf = buffers[writeIdx];
        auto* L   = buf.getWritePointer (0);
        auto* R   = buf.getWritePointer (1);

        juce::Random rng;

        for (int i = 0; i < kTableSize; ++i)
        {
            const float t = (float) i / (float) kTableSize;  // 0..1 per cycle
            float v = 0.f;
            switch (shape)
            {
                case Sine:     v = std::sin (t * juce::MathConstants<float>::twoPi);  break;
                case Saw:      v = t * 2.f - 1.f;                                     break;
                case Square:   v = t < 0.5f ? 1.f : -1.f;                             break;
                case Triangle: v = 1.f - 4.f * std::abs (t - 0.5f);                  break;
                case Noise:    v = rng.nextFloat() * 2.f - 1.f;                       break;
                default:       v = 0.f; break;
            }
            L[i] = R[i] = v * 0.85f;  // slight headroom
        }

        readIdx.store (writeIdx, std::memory_order_release);
    }

    /** Audio-thread safe. Returns the currently active wavetable buffer. */
    const juce::AudioBuffer<float>& getBuffer() const
    {
        return buffers[readIdx.load (std::memory_order_acquire)];
    }

    /** Fundamental frequency of the table at the current sample rate. */
    double tableFreqHz() const { return sampleRate / (double) kTableSize; }

    bool isPrepared() const { return buffers[0].getNumSamples() == kTableSize; }

private:
    juce::AudioBuffer<float> buffers[2];
    std::atomic<int>         readIdx { 0 };
    double                   sampleRate = 44100.0;
};
