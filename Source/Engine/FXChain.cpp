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
    shimReverb.prepare (spec);

    // Delay: max 2 seconds
    const int maxDelaySamples = (int) (sr * 2.0) + 1;
    delayBufL.assign ((size_t) maxDelaySamples, 0.f);
    delayBufR.assign ((size_t) maxDelaySamples, 0.f);
    delayWritePos = 0;

    // Shimmer pitch-shift buffer: ~80ms ring buffer, two crossfading read heads
    const int pitchBufSize = (int) (sr * 0.08) + 4;
    shimPitchBufL.assign ((size_t) pitchBufSize, 0.f);
    shimPitchBufR.assign ((size_t) pitchBufSize, 0.f);
    shimPitchWrite = 0;
    shimReadPos0   = 0.f;
    shimReadPos1   = (float) pitchBufSize * 0.5f;

    // Lush chorus: 6 voices, max ~25ms delay each
    const int maxLushDelay = (int) (sr * 0.025) + 4;
    for (int v = 0; v < kLushVoices; ++v)
    {
        lushBufL[v].assign ((size_t) maxLushDelay, 0.f);
        lushBufR[v].assign ((size_t) maxLushDelay, 0.f);
        lushWritePos[v]  = 0;
        lushLfoPhase[v]  = (float) v / (float) kLushVoices; // staggered start phases
    }

    dryBuffer.setSize (2, maxBlock);
    lastType = FXType::None;
}

void FXProcessor::reset()
{
    reverb.reset();
    chorus.reset();
    filter.reset();
    shimReverb.reset();
    std::fill (delayBufL.begin(),    delayBufL.end(),    0.f);
    std::fill (delayBufR.begin(),    delayBufR.end(),    0.f);
    std::fill (shimPitchBufL.begin(),shimPitchBufL.end(),0.f);
    std::fill (shimPitchBufR.begin(),shimPitchBufR.end(),0.f);
    for (int v = 0; v < kLushVoices; ++v)
    {
        std::fill (lushBufL[v].begin(), lushBufL[v].end(), 0.f);
        std::fill (lushBufR[v].begin(), lushBufR[v].end(), 0.f);
    }
    delayWritePos = 0;
    shimPitchWrite = 0;
    lastType = FXType::None;
}

void FXProcessor::reinitEffect (FXType type)
{
    // Reset the relevant DSP object when type changes to clear stale state
    if (type == FXType::Reverb)       reverb.reset();
    if (type == FXType::Chorus)       chorus.reset();
    if (type == FXType::Filter)       filter.reset();
    if (type == FXType::ShimmerReverb)
    {
        shimReverb.reset();
        std::fill (shimPitchBufL.begin(), shimPitchBufL.end(), 0.f);
        std::fill (shimPitchBufR.begin(), shimPitchBufR.end(), 0.f);
        shimPitchWrite = 0;
        shimReadPos0   = 0.f;
        shimReadPos1   = (float) shimPitchBufL.size() * 0.5f;
    }
    if (type == FXType::LushChorus)
    {
        for (int v = 0; v < kLushVoices; ++v)
        {
            std::fill (lushBufL[v].begin(), lushBufL[v].end(), 0.f);
            std::fill (lushBufR[v].begin(), lushBufR[v].end(), 0.f);
            lushWritePos[v] = 0;
        }
    }
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
        case FXType::Reverb:        processReverb        (buffer, params);       break;
        case FXType::Delay:         processDelay         (buffer, params, bpm);  break;
        case FXType::Chorus:        processChorus        (buffer, params);       break;
        case FXType::Distortion:    processDistortion    (buffer, params);       break;
        case FXType::Filter:        processFilter        (buffer, params);       break;
        case FXType::ShimmerReverb: processShimmerReverb (buffer, params);       break;
        case FXType::LushChorus:    processLushChorus    (buffer, params);       break;
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
// SHIMMER REVERB
// A large JUCE reverb with a pitch-shifted (octave up) feedback loop.
// p1 = Size (0-1 → room size 0.65-1.0), p2 = Damping, p3 = Shimmer amount
//
// Algorithm: read pitch-shifted signal from a ring buffer (previous block's
// reverb output), add to input, process through reverb, write reverb output
// back to the ring buffer. Two crossfading read heads at 2x speed = +1 octave.
void FXProcessor::processShimmerReverb (juce::AudioBuffer<float>& buffer,
                                         const FXSlotParams& params)
{
    const int  numSamples   = buffer.getNumSamples();
    const bool hasStereo    = buffer.getNumChannels() > 1;
    const float shimmerAmt  = params.p3 * 0.55f;   // cap to avoid feedback runaway
    const int   pitchBufSz  = (int) shimPitchBufL.size();
    if (pitchBufSz < 4) return;

    // ── Step 1: add pitch-shifted shimmer from previous block into input ──────
    for (int s = 0; s < numSamples; ++s)
    {
        // Linear-interpolated read for both crossfade heads
        auto readBuf = [&] (const std::vector<float>& buf, float pos) -> float
        {
            const int i = (int) pos % pitchBufSz;
            const int j = (i + 1) % pitchBufSz;
            const float f = pos - std::floor (pos);
            return buf[i] * (1.f - f) + buf[j] * f;
        };

        // Hann window crossfade (position-based)
        const float t0 = std::fmod (shimReadPos0, (float) pitchBufSz) / (float) pitchBufSz;
        const float t1 = std::fmod (shimReadPos1, (float) pitchBufSz) / (float) pitchBufSz;
        const float w0 = 0.5f * (1.f - std::cos (t0 * 6.283185f));
        const float w1 = 0.5f * (1.f - std::cos (t1 * 6.283185f));

        const float shimL = readBuf (shimPitchBufL, shimReadPos0) * w0
                          + readBuf (shimPitchBufL, shimReadPos1) * w1;
        const float shimR = readBuf (shimPitchBufR, shimReadPos0) * w0
                          + readBuf (shimPitchBufR, shimReadPos1) * w1;

        buffer.addSample (0, s, shimL * shimmerAmt);
        if (hasStereo) buffer.addSample (1, s, shimR * shimmerAmt);

        // Advance both read heads at 2x write speed → +1 octave pitch shift
        shimReadPos0 = std::fmod (shimReadPos0 + 2.f, (float) pitchBufSz);
        shimReadPos1 = std::fmod (shimReadPos1 + 2.f, (float) pitchBufSz);
    }

    // ── Step 2: process through reverb ────────────────────────────────────────
    juce::dsp::Reverb::Parameters rp;
    rp.roomSize   = 0.65f + params.p1 * 0.35f;
    rp.damping    = params.p2;
    rp.width      = 1.0f;
    rp.wetLevel   = params.mix;
    rp.dryLevel   = 1.f - params.mix;
    rp.freezeMode = 0.f;
    shimReverb.setParameters (rp);

    juce::dsp::AudioBlock<float>              block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx   (block);
    shimReverb.process (ctx);

    // ── Step 3: write reverb output to pitch buffer for next block ────────────
    for (int s = 0; s < numSamples; ++s)
    {
        shimPitchBufL[shimPitchWrite] = buffer.getSample (0, s);
        shimPitchBufR[shimPitchWrite] = hasStereo ? buffer.getSample (1, s)
                                                  : buffer.getSample (0, s);
        shimPitchWrite = (shimPitchWrite + 1) % pitchBufSz;
    }
}

//==============================================================================
// LUSH CHORUS
// 6-voice BBD-style chorus. Each voice uses a slightly different LFO rate for
// a living, wide texture. Voices alternate left/right for stereo spread.
// p1 = Rate (0-1 → 0.1-4 Hz base), p2 = Depth (0-1 → 1-9ms depth range),
// p3 = Feedback (0-1 → 0-40%), mix
void FXProcessor::processLushChorus (juce::AudioBuffer<float>& buffer,
                                      const FXSlotParams& params)
{
    const int  numSamples = buffer.getNumSamples();
    const bool hasStereo  = buffer.getNumChannels() > 1;
    const float baseRate  = params.p1 * 3.9f + 0.1f;      // 0.1–4.0 Hz
    const float depthMs   = params.p2 * 8.f + 1.f;        // 1–9 ms
    const float feedback  = params.p3 * 0.4f;             // 0–40%
    const float mix       = params.mix;

    // Per-voice LFO frequency multipliers and stereo pan (+1=L bias, -1=R bias)
    static constexpr float rateMults[kLushVoices] = { 1.0f, 1.13f, 0.87f, 1.07f, 0.73f, 1.19f };
    static constexpr float panL     [kLushVoices] = { 1.0f, 0.7f,  1.0f,  0.7f,  1.0f,  0.7f  };
    static constexpr float panR     [kLushVoices] = { 0.7f, 1.0f,  0.7f,  1.0f,  0.7f,  1.0f  };
    // Base delay offsets (ms) spread across voices for extra thickness
    static constexpr float baseDelMs[kLushVoices] = { 5.f, 6.5f, 8.f, 7.f, 10.f, 9.f };

    for (int s = 0; s < numSamples; ++s)
    {
        const float inL = buffer.getSample (0, s);
        const float inR = hasStereo ? buffer.getSample (1, s) : inL;
        float outL = 0.f, outR = 0.f;

        for (int v = 0; v < kLushVoices; ++v)
        {
            // Advance LFO
            lushLfoPhase[v] = std::fmod (lushLfoPhase[v] + rateMults[v] * baseRate / (float) sampleRate, 1.f);
            const float lfoSine = std::sin (lushLfoPhase[v] * 6.283185f);

            // Fractional delay in samples
            const float delaySamples = juce::jlimit (1.f, (float) lushBufL[v].size() - 2.f,
                (baseDelMs[v] + lfoSine * depthMs) * (float) sampleRate / 1000.f);
            const int bufSz = (int) lushBufL[v].size();

            // Write (with feedback from read)
            const int readIdx = (int) std::fmod ((float) lushWritePos[v] - delaySamples + (float) bufSz, (float) bufSz);
            const int readIdx1 = (readIdx + 1) % bufSz;
            const float frac = std::fmod ((float) lushWritePos[v] - delaySamples + (float) bufSz, 1.f);
            const float delayedL = lushBufL[v][readIdx] * (1.f - frac) + lushBufL[v][readIdx1] * frac;
            const float delayedR = lushBufR[v][readIdx] * (1.f - frac) + lushBufR[v][readIdx1] * frac;

            lushBufL[v][lushWritePos[v]] = inL + delayedL * feedback;
            lushBufR[v][lushWritePos[v]] = inR + delayedR * feedback;

            // Sum with pan spread
            outL += delayedL * panL[v];
            outR += delayedR * panR[v];

            lushWritePos[v] = (lushWritePos[v] + 1) % bufSz;
        }

        // Normalize
        constexpr float norm = 1.f / (float) kLushVoices;
        outL *= norm;
        outR *= norm;

        buffer.setSample (0, s, inL * (1.f - mix) + outL * mix);
        if (hasStereo) buffer.setSample (1, s, inR * (1.f - mix) + outR * mix);
    }
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
