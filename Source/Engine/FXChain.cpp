#include "FXChain.h"
#include <cmath>

//==============================================================================
// FXProcessor
//==============================================================================

void FXProcessor::prepare (double sr, int maxBlock)
{
    sampleRate   = sr;
    maxBlockSize = maxBlock;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = (juce::uint32) maxBlock;
    spec.numChannels      = 2;

    reverb.prepare (spec);
    chorus.prepare (spec);
    filter.prepare (spec);

    // Delay: max 2 seconds
    const int maxDelaySamples = (int) (sr * 2.0) + 1;
    delayBufL.assign ((size_t) maxDelaySamples, 0.f);
    delayBufR.assign ((size_t) maxDelaySamples, 0.f);
    delayWritePos = 0;

    dryBuffer.setSize (2, maxBlock);
    lastType = FXType::None;
}

void FXProcessor::reset()
{
    reverb.reset();
    chorus.reset();
    filter.reset();
    std::fill (delayBufL.begin(), delayBufL.end(), 0.f);
    std::fill (delayBufR.begin(), delayBufR.end(), 0.f);
    delayWritePos = 0;
    lastType = FXType::None;
}

void FXProcessor::reinitEffect (FXType type)
{
    // Reset the relevant DSP object when type changes to clear stale state
    if (type == FXType::Reverb)  reverb.reset();
    if (type == FXType::Chorus)  chorus.reset();
    if (type == FXType::Filter)  filter.reset();
    if (type == FXType::Delay)
    {
        std::fill (delayBufL.begin(), delayBufL.end(), 0.f);
        std::fill (delayBufR.begin(), delayBufR.end(), 0.f);
        delayWritePos = 0;
    }
    lastType = type;
}

//==============================================================================
void FXProcessor::process (juce::AudioBuffer<float>& buffer,
                            const FXSlotParams& params,
                            double bpm)
{
    if (params.bypass || params.type == FXType::None) return;
    if (params.type != lastType) reinitEffect (params.type);

    switch (params.type)
    {
        case FXType::Reverb:     processReverb     (buffer, params);       break;
        case FXType::Delay:      processDelay      (buffer, params, bpm);  break;
        case FXType::Chorus:     processChorus     (buffer, params);       break;
        case FXType::Distortion: processDistortion (buffer, params);       break;
        case FXType::Filter:     processFilter     (buffer, params);       break;
        default: break;
    }
}

//==============================================================================
// REVERB
// p1 = Room Size (0-1), p2 = Damping (0-1), p3 = Width (0-1), mix
void FXProcessor::processReverb (juce::AudioBuffer<float>& buffer,
                                  const FXSlotParams& params)
{
    juce::dsp::Reverb::Parameters rp;
    rp.roomSize   = params.p1;
    rp.damping    = params.p2;
    rp.width      = params.p3;
    rp.wetLevel   = params.mix;
    rp.dryLevel   = 1.f - params.mix;
    rp.freezeMode = 0.f;
    reverb.setParameters (rp);

    juce::dsp::AudioBlock<float>              block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx   (block);
    reverb.process (ctx);
}

//==============================================================================
// DELAY
// p1 = Time (0-1 → 0–2000ms, or BPM-synced), p2 = Feedback (0–0.95)
// p3 = Spread (0 = stereo, >0.5 = ping-pong), mix
void FXProcessor::processDelay (juce::AudioBuffer<float>& buffer,
                                 const FXSlotParams& params,
                                 double bpm)
{
    const int numSamples = buffer.getNumSamples();

    // Map p1 to delay time: snap to nearest beat division if bpm available
    double delayTimeSec;
    if (bpm > 0.0)
    {
        // 9 divisions matching beatDivision param: 1/32, 1/16T, 1/16, 1/8T, 1/8, 1/4T, 1/4, 1/2, 1 bar
        const double beatSec = 60.0 / bpm;
        const double mults[] = { 0.125, 1.0/6.0, 0.25, 1.0/3.0, 0.5, 2.0/3.0, 1.0, 2.0, 4.0 };
        const int divIdx = juce::jlimit (0, 8, (int) (params.p1 * 9.f));
        delayTimeSec = beatSec * mults[divIdx];
    }
    else
    {
        delayTimeSec = (double) params.p1 * 2.0; // 0–2 sec
    }

    const int bufSize     = (int) delayBufL.size();
    const int delaySamples = juce::jlimit (1, bufSize - 1,
                                            (int) (delayTimeSec * sampleRate));
    const float feedback  = juce::jlimit (0.f, 0.95f, params.p2);
    const bool  pingPong  = params.p3 > 0.5f;
    const float mix       = params.mix;

    const bool hasStereo  = buffer.getNumChannels() > 1;

    for (int s = 0; s < numSamples; ++s)
    {
        const int readPos = (delayWritePos - delaySamples + bufSize) % bufSize;

        const float dryL = buffer.getSample (0, s);
        const float dryR = hasStereo ? buffer.getSample (1, s) : dryL;

        const float wetL = delayBufL[readPos];
        const float wetR = delayBufR[readPos];

        if (pingPong)
        {
            delayBufL[delayWritePos] = dryR + wetR * feedback;
            delayBufR[delayWritePos] = dryL + wetL * feedback;
        }
        else
        {
            delayBufL[delayWritePos] = dryL + wetL * feedback;
            delayBufR[delayWritePos] = dryR + wetR * feedback;
        }

        buffer.setSample (0, s, dryL * (1.f - mix) + wetL * mix);
        if (hasStereo)
            buffer.setSample (1, s, dryR * (1.f - mix) + wetR * mix);

        delayWritePos = (delayWritePos + 1) % bufSize;
    }
}

//==============================================================================
// CHORUS
// p1 = Rate (0–1 → 0.01–5 Hz), p2 = Depth (0–1), p3 = Feedback (0-0.5), mix
void FXProcessor::processChorus (juce::AudioBuffer<float>& buffer,
                                  const FXSlotParams& params)
{
    chorus.setRate        (params.p1 * 4.99f + 0.01f);
    chorus.setDepth       (params.p2);
    chorus.setCentreDelay (7.f);
    chorus.setFeedback    (params.p3 * 0.5f);
    chorus.setMix         (params.mix);

    juce::dsp::AudioBlock<float>              block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx   (block);
    chorus.process (ctx);
}

//==============================================================================
// DISTORTION
// p1 = Drive (0–1), p2 = Tone (0–1 → LP cutoff), p3 = Symmetry (unused for now), mix
void FXProcessor::processDistortion (juce::AudioBuffer<float>& buffer,
                                      const FXSlotParams& params)
{
    const float drive = params.p1 * 9.f + 1.f;  // 1–10
    const float mix   = params.mix;

    // Pre-gain tone shaping — simple 1-pole LP using tone param as cutoff
    const float toneCutoff = 500.f + params.p2 * 19500.f; // 500Hz–20kHz
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    filter.setCutoffFrequency (toneCutoff);
    filter.setResonance (0.5f);

    juce::dsp::AudioBlock<float>              block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx   (block);
    filter.process (ctx);

    // Waveshaping: soft clip via tanh
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int s = 0; s < buffer.getNumSamples(); ++s)
        {
            const float dry = data[s];
            const float wet = std::tanh (dry * drive);
            data[s] = dry * (1.f - mix) + wet * mix;
        }
    }
}

//==============================================================================
// FILTER
// p1 = Cutoff (0–1 → 20Hz–20kHz log), p2 = Resonance (0–1), p3 = Type (LP/HP/BP)
void FXProcessor::processFilter (juce::AudioBuffer<float>& buffer,
                                  const FXSlotParams& params)
{
    const float cutoff = 20.f * std::pow (1000.f, params.p1); // 20Hz–20kHz
    filter.setCutoffFrequency (juce::jlimit (20.f, 20000.f, cutoff));
    filter.setResonance       (0.5f + params.p2 * 9.5f);      // 0.5–10

    if (params.p3 < 0.33f)
        filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    else if (params.p3 < 0.67f)
        filter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    else
        filter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);

    // Store dry, process, then mix
    dryBuffer.makeCopyOf (buffer);

    juce::dsp::AudioBlock<float>              block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx   (block);
    filter.process (ctx);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.addFrom (ch, 0, dryBuffer, ch, 0, buffer.getNumSamples(),
                        -(1.f - params.mix)); // scale wet by mix, re-add dry portion
}

//==============================================================================
// FXChain
//==============================================================================

FXChain::FXChain()
{
    for (int i = 0; i < numSlots; ++i)
        slots.push_back (std::make_unique<FXProcessor>());
}

void FXChain::prepare (double sampleRate, int maxBlockSize)
{
    for (auto& slot : slots)
        slot->prepare (sampleRate, maxBlockSize);
}

void FXChain::reset()
{
    for (auto& slot : slots)
        slot->reset();
}

void FXChain::process (juce::AudioBuffer<float>&           buffer,
                        juce::AudioProcessorValueTreeState& apvts,
                        double                              bpm)
{
    auto getF = [&] (const juce::String& id) -> float
    {
        return apvts.getRawParameterValue (id)->load();
    };

    for (int i = 0; i < numSlots; ++i)
    {
        const juce::String s (i);

        FXSlotParams p;
        p.type   = static_cast<FXType> ((int) getF ("fxType"   + s));
        p.bypass = getF ("fxBypass" + s) > 0.5f;
        p.p1     = getF ("fxP1"     + s);
        p.p2     = getF ("fxP2"     + s);
        p.p3     = getF ("fxP3"     + s);
        p.mix    = getF ("fxMix"    + s);

        slots[(size_t) i]->process (buffer, p, bpm);
    }
}
