#pragma once
#include <JuceHeader.h>

//==============================================================================
enum class FXType { None = 0, Reverb, Delay, Chorus, Distortion, Filter,
                    ShimmerReverb, LushChorus };

struct FXSlotParams
{
    FXType type   = FXType::None;
    bool   bypass = false;
    float  p1     = 0.5f;  // meaning depends on type
    float  p2     = 0.5f;
    float  p3     = 0.5f;
    float  mix    = 1.0f;
};

//==============================================================================
// Processes one effect slot. Owns all DSP objects so re-init on type change
// is contained here.
class FXProcessor
{
public:
    FXProcessor()  = default;

    void prepare (double sampleRate, int maxBlockSize);
    void process (juce::AudioBuffer<float>& buffer,
                  const FXSlotParams& params,
                  double bpm);
    void reset();

private:
    double sampleRate   = 44100.0;
    int    maxBlockSize = 512;
    FXType lastType     = FXType::None;

    // ── DSP objects ──────────────────────────────────────────────────────────
    juce::dsp::Reverb                        reverb;
    juce::dsp::Chorus<float>                 chorus;
    juce::dsp::StateVariableTPTFilter<float> filter;

    // Manual stereo delay (circular buffer)
    std::vector<float> delayBufL, delayBufR;
    int                delayWritePos = 0;

    // Scratch buffer for dry signal / dry-wet mixing
    juce::AudioBuffer<float> dryBuffer;

    // ── Shimmer Reverb DSP ────────────────────────────────────────────────────
    juce::dsp::Reverb         shimReverb;
    std::vector<float>        shimPitchBufL, shimPitchBufR;
    int                       shimPitchWrite = 0;
    float                     shimReadPos0   = 0.f;
    float                     shimReadPos1   = 0.f;   // second crossfade head

    // ── Lush Chorus DSP ───────────────────────────────────────────────────────
    static constexpr int kLushVoices = 6;
    std::vector<float>   lushBufL[kLushVoices], lushBufR[kLushVoices];
    int                  lushWritePos[kLushVoices] = {};
    float                lushLfoPhase[kLushVoices] = {};

    // ── Per-type process helpers ─────────────────────────────────────────────
    void reinitEffect (FXType type);
    void processReverb        (juce::AudioBuffer<float>&, const FXSlotParams&);
    void processDelay         (juce::AudioBuffer<float>&, const FXSlotParams&, double bpm);
    void processChorus        (juce::AudioBuffer<float>&, const FXSlotParams&);
    void processDistortion    (juce::AudioBuffer<float>&, const FXSlotParams&);
    void processFilter        (juce::AudioBuffer<float>&, const FXSlotParams&);
    void processShimmerReverb (juce::AudioBuffer<float>&, const FXSlotParams&);
    void processLushChorus    (juce::AudioBuffer<float>&, const FXSlotParams&);

    FXProcessor (const FXProcessor&) = delete;
    FXProcessor& operator= (const FXProcessor&) = delete;
};

//==============================================================================
// The full 4-slot chain. Reads all parameters from APVTS and processes
// buffer in order.
class FXChain
{
public:
    static constexpr int numSlots = 4;

    FXChain();

    void prepare (double sampleRate, int maxBlockSize);
    void process (juce::AudioBuffer<float>&,
                  juce::AudioProcessorValueTreeState&,
                  double bpm);
    void reset();

private:
    std::vector<std::unique_ptr<FXProcessor>> slots;

    FXChain (const FXChain&) = delete;
    FXChain& operator= (const FXChain&) = delete;
};
