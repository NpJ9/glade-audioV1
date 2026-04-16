#pragma once
#include <JuceHeader.h>
#include <array>
#include <memory>

//==============================================================================
enum class FXType { None = 0, Reverb, Delay, Chorus, Distortion, Filter,
                    ShimmerReverb, LushChorus, Flanger, Harmonic, AutoPan };

struct FXSlotParams
{
    FXType type   = FXType::None;
    bool   bypass = false;
    float  p1     = 0.5f;   // meaning depends on type
    float  p2     = 0.5f;
    float  p3     = 0.5f;
    float  mix    = 1.0f;
};

//==============================================================================
/** Abstract interface for a single effect slot.
 *
 *  Each effect type is a separate concrete class owning only its own DSP
 *  objects.  FXChain holds an array of IEffectProcessor* and swaps the
 *  instance when the slot type changes — no switch-heavy monolithic class,
 *  no memory allocated for effects that aren't in use.
 *
 *  To add a new effect: subclass IEffectProcessor, implement the three
 *  methods, and add a case to FXChain::createEffect().  Nothing else changes. */
class IEffectProcessor
{
public:
    virtual ~IEffectProcessor() = default;

    virtual void prepare (double sampleRate, int maxBlockSize) = 0;
    virtual void process (juce::AudioBuffer<float>&, const FXSlotParams&, double bpm) = 0;
    virtual void reset() = 0;
};

//==============================================================================
// Concrete effect implementations — declared here, defined in FXChain.cpp
//==============================================================================

// REVERB  p1=RoomSize  p2=Damping  p3=Width  mix
class ReverbEffect : public IEffectProcessor
{
public:
    void prepare (double sr, int maxBlock) override;
    void process (juce::AudioBuffer<float>&, const FXSlotParams&, double bpm) override;
    void reset() override;
private:
    juce::dsp::Reverb reverb;
};

// DELAY  p1=Time(0-1→div)  p2=Feedback(0-0.95)  p3=PingPong  mix
class DelayEffect : public IEffectProcessor
{
public:
    void prepare (double sr, int maxBlock) override;
    void process (juce::AudioBuffer<float>&, const FXSlotParams&, double bpm) override;
    void reset() override;
private:
    double sampleRate = 44100.0;
    std::vector<float> delayBufL, delayBufR;
    int   delayWritePos = 0;

    static constexpr float kDCCoeff = 0.9998f;
    float dcBlockInL = 0.f, dcBlockInR = 0.f;
    float dcBlockOutL = 0.f, dcBlockOutR = 0.f;
};

// CHORUS  p1=Rate(0-1→0.01-5Hz)  p2=Depth  p3=Feedback  mix
class ChorusEffect : public IEffectProcessor
{
public:
    void prepare (double sr, int maxBlock) override;
    void process (juce::AudioBuffer<float>&, const FXSlotParams&, double bpm) override;
    void reset() override;
private:
    juce::dsp::Chorus<float> chorus;
};

// DISTORTION  p1=Drive  p2=Tone  p3=unused  mix
// 2x oversampled tanh waveshaper with pre-tone LP filter.
class DistortionEffect : public IEffectProcessor
{
public:
    void prepare (double sr, int maxBlock) override;
    void process (juce::AudioBuffer<float>&, const FXSlotParams&, double bpm) override;
    void reset() override;
private:
    double sampleRate   = 44100.0;
    int    maxBlockSize = 512;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::dsp::Oversampling<float> oversampler {
        2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR };
    juce::AudioBuffer<float> dryBuffer;
};

// FILTER  p1=Cutoff  p2=Resonance  p3=Type(LP/HP/BP)  mix
class FilterEffect : public IEffectProcessor
{
public:
    void prepare (double sr, int maxBlock) override;
    void process (juce::AudioBuffer<float>&, const FXSlotParams&, double bpm) override;
    void reset() override;
private:
    double sampleRate   = 44100.0;
    int    maxBlockSize = 512;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::AudioBuffer<float> dryBuffer;
};

// SHIMMER REVERB  p1=RoomSize  p2=Damping  p3=ShimmerAmount  mix
// Large reverb with an octave-up pitch-shift feedback loop.
class ShimmerEffect : public IEffectProcessor
{
public:
    void prepare (double sr, int maxBlock) override;
    void process (juce::AudioBuffer<float>&, const FXSlotParams&, double bpm) override;
    void reset() override;
private:
    double sampleRate = 44100.0;
    juce::dsp::Reverb shimReverb;
    std::vector<float> shimPitchBufL, shimPitchBufR;
    // Dry signal save-buffer for manual wet/dry blend (decouples mix from feedback path)
    std::vector<float> shimDryL, shimDryR;
    int   shimPitchWrite = 0;
    float shimReadPos0   = 0.f;
    float shimReadPos1   = 0.f;
};

// FLANGER  p1=Rate(0-1→0.05-5Hz)  p2=Depth(0-1)  p3=Feedback(-1..1)  mix
// Classic all-pass style flanger: one LFO-modulated short delay (1-8ms)
// summed back with the dry signal, producing a comb-filter sweep.
class FlangerEffect : public IEffectProcessor
{
public:
    void prepare (double sr, int maxBlock) override;
    void process (juce::AudioBuffer<float>&, const FXSlotParams&, double bpm) override;
    void reset() override;
private:
    double sampleRate = 44100.0;
    static constexpr int kMaxDelaySamples = 4096;   // ~93ms at 44100
    float bufL[kMaxDelaySamples] = {};
    float bufR[kMaxDelaySamples] = {};
    int   writePos = 0;
    float lfoPhaseL = 0.f;
    float lfoPhaseR = 0.5f;  // 180° offset for stereo width
};

// HARMONIC EXCITER  p1=Drive  p2=Freq(target band 200-5000Hz)  p3=Character(0=even/warm, 1=odd/bright)  mix
// Parallel saturation exciter: HP-filtered band → soft-clip → additive blend.
class HarmonicEffect : public IEffectProcessor
{
public:
    void prepare (double sr, int maxBlock) override;
    void process (juce::AudioBuffer<float>&, const FXSlotParams&, double bpm) override;
    void reset() override;
private:
    double sampleRate = 44100.0;
    juce::dsp::StateVariableTPTFilter<float> hpFilter;
    juce::AudioBuffer<float> dryBuffer;
};

// AUTO-PAN  p1=Rate(0-1→0.05-8Hz)  p2=Depth  p3=unused  mix
// Sinusoidal LFO-driven constant-power stereo panner.
class AutoPanEffect : public IEffectProcessor
{
public:
    void prepare (double sr, int maxBlock) override;
    void process (juce::AudioBuffer<float>&, const FXSlotParams&, double bpm) override;
    void reset() override;
private:
    double sampleRate = 44100.0;
    float  lfoPhase   = 0.f;
};

// LUSH CHORUS  p1=Rate  p2=Depth  p3=Feedback  mix
// 6-voice BBD-style chorus with staggered LFO rates.
class LushChorusEffect : public IEffectProcessor
{
public:
    void prepare (double sr, int maxBlock) override;
    void process (juce::AudioBuffer<float>&, const FXSlotParams&, double bpm) override;
    void reset() override;
private:
    double sampleRate = 44100.0;

    static constexpr int kLushVoices = 6;
    std::vector<float> lushBufL[kLushVoices], lushBufR[kLushVoices];
    int   lushWritePos[kLushVoices] = {};
    float lushLfoPhase[kLushVoices] = {};
};

//==============================================================================
/** 4-slot FX chain.  Each slot holds a unique_ptr<IEffectProcessor>.
 *
 *  Thread-safety model
 *  -------------------
 *  Effect CREATION is expensive (heap allocation + DSP setup).  It must not
 *  happen on the audio thread.  The protocol is:
 *
 *    Message thread  →  requestTypeChange(slot, type)
 *      Creates the new effect, writes it into incomingFifo (SPSC, M→A).
 *
 *    Audio thread    →  drainIncoming()  (called at the top of process())
 *      Swaps in the new effect; the old effect pointer goes into garbageFifo (A→M).
 *
 *    Message thread  →  collectGarbage()  (called from the editor's 30-Hz timer)
 *      Deletes the old effects from garbageFifo.
 *
 *  Neither queue ever blocks.  The audio thread never allocates or deletes. */
class FXChain
{
public:
    static constexpr int numSlots = 4;

    FXChain();
    ~FXChain();

    void prepare (double sampleRate, int maxBlockSize);
    void process (juce::AudioBuffer<float>&,
                  const std::array<FXSlotParams, numSlots>& params,
                  double bpm);
    void reset();

    /** Call from the message thread when the user changes an effect type.
     *  Creates the new effect immediately (safe to allocate here) and enqueues
     *  it for the audio thread to install on its next process() call. */
    void requestTypeChange (int slot, FXType newType);

    /** Call from the message thread (e.g. editor timer) to delete old effects
     *  that were retired by the audio thread. */
    void collectGarbage();

private:
    std::array<std::unique_ptr<IEffectProcessor>, numSlots> slots;
    std::array<FXType, numSlots>                            slotTypes;

    // Stored so newly created effect instances can be prepared immediately.
    double storedSR       = 44100.0;
    int    storedMaxBlock = 512;

    static std::unique_ptr<IEffectProcessor> createEffect (FXType type,
                                                            double sr,
                                                            int    maxBlock);

    // ── Lock-free SPSC queues ────────────────────────────────────────────────
    struct EffectUpdate
    {
        int               slot   = -1;
        FXType            type   = FXType::None;
        IEffectProcessor* ptr    = nullptr;   // nullptr means "clear this slot"
    };

    static constexpr int kQueueSize = 16;   // 4 slots × up to 4 rapid changes each

    // Message thread → Audio thread: new effects to install
    juce::AbstractFifo incomingFifo { kQueueSize };
    EffectUpdate       incomingBuf  [kQueueSize];

    // Audio thread → Message thread: old effects to delete
    juce::AbstractFifo garbageFifo  { kQueueSize };
    IEffectProcessor*  garbageBuf   [kQueueSize] {};

    /** Drain incomingFifo and swap in any pending effects.  Audio thread only. */
    void drainIncoming() noexcept;

    FXChain (const FXChain&) = delete;
    FXChain& operator= (const FXChain&) = delete;
};
