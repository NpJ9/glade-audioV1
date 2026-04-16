#pragma once
#include "GrainPool.h"
#include <JuceHeader.h>

// Tracks elapsed time and fires new grains into the pool based on
// density, position, jitter, pitch, and pan spread parameters.
class GrainScheduler
{
public:
    GrainScheduler() = default;

    // Call from processBlock. numSamples = block size.
    // isActive: false = scheduler is silent (no note playing in SYNTH mode)
    void process (int             numSamples,
                  double          sampleRate,
                  GrainPool&      pool,
                  int             numSourceSamples,

                  // Grain parameters (read each block from APVTS)
                  float           grainSizeMs,
                  float           density,            // grains/sec
                  float           position,           // 0-1 in source buffer
                  float           positionJitter,     // 0-1
                  double          pitchRatio,
                  float           pitchJitter,        // 0-1 (semitones = ±12 * jitter)
                  float           panSpread,          // 0-1
                  WindowType      windowType,
                  bool            isActive,
                  int             scaleIdx       = 0,   // 0=Chromatic … 6=Dorian
                  int             rootNote       = 0,   // 0=C … 11=B
                  float           reverseAmount  = 0.f, // 0=all fwd, 1=all rev, 0.5=mix
                  float           velocityScale  = 1.f);// 0–1 MIDI velocity scale

    void reset() { samplesUntilNextGrain = 0.0; }

private:
    double         samplesUntilNextGrain = 0.0;
    juce::Random   random;

    // Equal-power pan from -1 (left) to +1 (right)
    static void calcPan (float normalised, float& outL, float& outR);
};
