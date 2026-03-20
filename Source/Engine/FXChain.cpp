#include "FXChain.h"
#include <cmath>

//==============================================================================
// ReverbEffect
//==============================================================================

void ReverbEffect::prepare (double sr, int maxBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = (juce::uint32) maxBlock;
    spec.numChannels      = 2;
    reverb.prepare (spec);
}

void ReverbEffect::process (juce::AudioBuffer<float>& buffer,
                             const FXSlotParams& params, double)
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

void ReverbEffect::reset() { reverb.reset(); }

//==============================================================================
// DelayEffect
//==============================================================================
// p1 = Time (0–1 → BPM-synced division, or 0–2 sec free-run)
// p2 = Feedback (0–0.95)
// p3 = Spread: <0.5 = stereo, ≥0.5 = ping-pong
// First-order DC-blocking HPF (R=kDCCoeff ≈ 1.4 Hz cutoff) in feedback path.

void DelayEffect::prepare (double sr, int maxBlock)
{
    sampleRate = sr;

    // Pre-allocate 2-second delay line
    const int maxDelaySamples = (int) (sr * 2.0) + 1;
    delayBufL.assign ((size_t) maxDelaySamples, 0.f);
    delayBufR.assign ((size_t) maxDelaySamples, 0.f);
    delayWritePos = 0;
    dcBlockInL = dcBlockOutL = dcBlockInR = dcBlockOutR = 0.f;

    juce::ignoreUnused (maxBlock);
}

void DelayEffect::process (juce::AudioBuffer<float>& buffer,
                            const FXSlotParams& params, double bpm)
{
    const int numSamples = buffer.getNumSamples();

    double delayTimeSec;
    if (bpm > 0.0)
    {
        static constexpr double beatMults[] =
            { 0.125, 1.0/6.0, 0.25, 1.0/3.0, 0.5, 2.0/3.0, 1.0, 2.0, 4.0 };
        const int divIdx    = juce::jlimit (0, 8, (int) (params.p1 * 9.f));
        delayTimeSec        = (60.0 / bpm) * beatMults[divIdx];
    }
    else
    {
        delayTimeSec = (double) params.p1 * 2.0;  // 0–2 sec
    }

    const int   bufSize      = (int) delayBufL.size();
    const int   delaySamples = juce::jlimit (1, bufSize - 1,
                                              (int) (delayTimeSec * sampleRate));
    const float feedback     = juce::jlimit (0.f, 0.95f, params.p2);
    const bool  pingPong     = params.p3 > 0.5f;
    const float mix          = params.mix;
    const bool  hasStereo    = buffer.getNumChannels() > 1;

    for (int s = 0; s < numSamples; ++s)
    {
        const int   readPos = (delayWritePos - delaySamples + bufSize) % bufSize;
        const float dryL    = buffer.getSample (0, s);
        const float dryR    = hasStereo ? buffer.getSample (1, s) : dryL;
        const float wetL    = delayBufL[readPos];
        const float wetR    = delayBufR[readPos];

        // DC-blocking IIR in feedback path: y[n] = x[n] - x[n-1] + R*y[n-1]
        const float rawL  = pingPong ? (dryR + wetR * feedback) : (dryL + wetL * feedback);
        const float rawR  = pingPong ? (dryL + wetL * feedback) : (dryR + wetR * feedback);
        const float filtL = rawL - dcBlockInL + kDCCoeff * dcBlockOutL;
        const float filtR = rawR - dcBlockInR + kDCCoeff * dcBlockOutR;
        dcBlockInL = rawL;   dcBlockOutL = filtL;
        dcBlockInR = rawR;   dcBlockOutR = filtR;

        delayBufL[delayWritePos] = filtL;
        delayBufR[delayWritePos] = filtR;

        buffer.setSample (0, s, dryL * (1.f - mix) + wetL * mix);
        if (hasStereo)
            buffer.setSample (1, s, dryR * (1.f - mix) + wetR * mix);

        delayWritePos = (delayWritePos + 1) % bufSize;
    }
}

void DelayEffect::reset()
{
    std::fill (delayBufL.begin(), delayBufL.end(), 0.f);
    std::fill (delayBufR.begin(), delayBufR.end(), 0.f);
    delayWritePos = 0;
    dcBlockInL = dcBlockOutL = dcBlockInR = dcBlockOutR = 0.f;
}

//==============================================================================
// ChorusEffect
//==============================================================================
// p1 = Rate (0–1 → 0.01–5 Hz)
// p2 = Depth (0–1)
// p3 = Feedback (0–0.5)

void ChorusEffect::prepare (double sr, int maxBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = (juce::uint32) maxBlock;
    spec.numChannels      = 2;
    chorus.prepare (spec);
}

void ChorusEffect::process (juce::AudioBuffer<float>& buffer,
                             const FXSlotParams& params, double)
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

void ChorusEffect::reset() { chorus.reset(); }

//==============================================================================
// DistortionEffect
//==============================================================================
// p1 = Drive (0–1 → 1–10x)
// p2 = Tone (0–1 → LP cutoff 500Hz–20kHz)
// p3 = unused
// 2× oversampled tanh waveshaper; pre-gain LP for tone shaping.
// Cutoff clamped to 45% Nyquist to keep SVF stable at high resonance.

void DistortionEffect::prepare (double sr, int maxBlock)
{
    sampleRate   = sr;
    maxBlockSize = maxBlock;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = (juce::uint32) maxBlock;
    spec.numChannels      = 2;

    filter.prepare (spec);
    oversampler.initProcessing ((size_t) maxBlock);
    dryBuffer.setSize (2, maxBlock);
}

void DistortionEffect::process (juce::AudioBuffer<float>& buffer,
                                 const FXSlotParams& params, double)
{
    const float drive       = params.p1 * 9.f + 1.f;  // 1–10
    const float mix         = params.mix;
    const int   numSamples  = buffer.getNumSamples();
    const int   numChannels = buffer.getNumChannels();

    // Guard: non-conformant host may exceed declared maxBlockSize
    if (numSamples > dryBuffer.getNumSamples() || numChannels > dryBuffer.getNumChannels())
        dryBuffer.setSize (numChannels, numSamples, false, true, true);

    // Pre-gain tone shaping — 1-pole LP; cutoff capped at 45% Nyquist
    const float maxCutoff  = (float) (sampleRate * 0.45);
    const float toneCutoff = juce::jlimit (500.f, maxCutoff, 500.f + params.p2 * 19500.f);
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    filter.setCutoffFrequency (toneCutoff);
    filter.setResonance (0.5f);

    juce::dsp::AudioBlock<float> block (buffer);
    {
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        filter.process (ctx);
    }

    // Save pre-waveshaper signal for dry/wet blend
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    // 2× oversample → tanh waveshaper → downsample
    auto oversampledBlock = oversampler.processSamplesUp (block);
    for (size_t ch = 0; ch < oversampledBlock.getNumChannels(); ++ch)
    {
        auto*        data = oversampledBlock.getChannelPointer (ch);
        const size_t n    = oversampledBlock.getNumSamples();
        for (size_t s = 0; s < n; ++s)
            data[s] = std::tanh (data[s] * drive);
    }
    oversampler.processSamplesDown (block);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        buffer.applyGain (ch, 0, numSamples, mix);
        buffer.addFrom   (ch, 0, dryBuffer, ch, 0, numSamples, 1.f - mix);
    }
}

void DistortionEffect::reset()
{
    filter.reset();
    oversampler.reset();
}

//==============================================================================
// FilterEffect
//==============================================================================
// p1 = Cutoff (0–1 → 20Hz–20kHz log)
// p2 = Resonance (0–1 → Q 0.5–10)
// p3 = Type (0–0.33=LP, 0.33–0.67=HP, 0.67–1=BP)
// Cutoff clamped to 45% Nyquist to prevent SVF instability at high Q.

void FilterEffect::prepare (double sr, int maxBlock)
{
    sampleRate   = sr;
    maxBlockSize = maxBlock;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = (juce::uint32) maxBlock;
    spec.numChannels      = 2;
    filter.prepare (spec);
    dryBuffer.setSize (2, maxBlock);
}

void FilterEffect::process (juce::AudioBuffer<float>& buffer,
                             const FXSlotParams& params, double)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples > dryBuffer.getNumSamples() || numChannels > dryBuffer.getNumChannels())
        dryBuffer.setSize (numChannels, numSamples, false, true, true);

    const float maxCutoff = (float) (sampleRate * 0.45);
    const float cutoff    = 20.f * std::pow (1000.f, params.p1);  // 20Hz–20kHz log
    filter.setCutoffFrequency (juce::jlimit (20.f, maxCutoff, cutoff));
    filter.setResonance       (0.5f + params.p2 * 9.5f);          // Q 0.5–10

    if      (params.p3 < 0.33f) filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    else if (params.p3 < 0.67f) filter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    else                        filter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);

    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    juce::dsp::AudioBlock<float>              block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx   (block);
    filter.process (ctx);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        buffer.applyGain (ch, 0, numSamples, params.mix);
        buffer.addFrom   (ch, 0, dryBuffer, ch, 0, numSamples, 1.f - params.mix);
    }
}

void FilterEffect::reset() { filter.reset(); }

//==============================================================================
// ShimmerEffect
//==============================================================================
// p1 = RoomSize (0–1 → 0.65–1.0)
// p2 = Damping
// p3 = Shimmer amount (capped at 0.55 to prevent feedback runaway)
// Algorithm: read pitch-shifted (octave up, 2× read speed) signal from a ring
// buffer fed by the previous block's reverb output, add to input, process
// through reverb, write back.  Two Hann-crossfading read heads.

void ShimmerEffect::prepare (double sr, int maxBlock)
{
    sampleRate = sr;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = (juce::uint32) maxBlock;
    spec.numChannels      = 2;
    shimReverb.prepare (spec);

    // ~80ms pitch-shift ring buffer, two crossfading read heads
    const int pitchBufSize = (int) (sr * 0.08) + 4;
    shimPitchBufL.assign ((size_t) pitchBufSize, 0.f);
    shimPitchBufR.assign ((size_t) pitchBufSize, 0.f);
    shimPitchWrite = 0;
    shimReadPos0   = 0.f;
    shimReadPos1   = (float) pitchBufSize * 0.5f;

    // Dry save-buffer: pre-sized to avoid per-block allocation
    shimDryL.assign ((size_t) maxBlock, 0.f);
    shimDryR.assign ((size_t) maxBlock, 0.f);
}

void ShimmerEffect::process (juce::AudioBuffer<float>& buffer,
                              const FXSlotParams& params, double)
{
    const int   numSamples  = buffer.getNumSamples();
    const bool  hasStereo   = buffer.getNumChannels() > 1;
    // Reduced cap (0.55 → 0.40): steady-state loop gain = shimmerAmt / (1 - shimmerAmt).
    // At 0.40 this is 0.67×; at 0.55 it was 1.22× which amplified above unity.
    const float shimmerAmt  = params.p3 * 0.40f;
    const float mix         = params.mix;
    const int   pitchBufSz  = (int) shimPitchBufL.size();
    if (pitchBufSz < 4) return;

    // Guard: non-conformant host may send a block larger than maxBlock
    if (numSamples > (int) shimDryL.size())
    {
        shimDryL.assign ((size_t) numSamples, 0.f);
        shimDryR.assign ((size_t) numSamples, 0.f);
    }

    // Step 0: save dry signal — the mix blend is done manually after reverb so
    // that the mix knob is outside the feedback path and can't affect loop gain.
    for (int s = 0; s < numSamples; ++s)
    {
        shimDryL[s] = buffer.getSample (0, s);
        shimDryR[s] = hasStereo ? buffer.getSample (1, s) : shimDryL[s];
    }

    // Step 1: add octave-up pitch-shifted shimmer from previous block
    for (int s = 0; s < numSamples; ++s)
    {
        auto readBuf = [&] (const std::vector<float>& buf, float pos) -> float
        {
            const int   i = (int) pos % pitchBufSz;
            const int   j = (i + 1) % pitchBufSz;
            const float f = pos - std::floor (pos);
            return buf[i] * (1.f - f) + buf[j] * f;
        };

        const float t0 = std::fmod (shimReadPos0, (float) pitchBufSz) / (float) pitchBufSz;
        const float t1 = std::fmod (shimReadPos1, (float) pitchBufSz) / (float) pitchBufSz;
        const float w0 = 0.5f * (1.f - std::cos (t0 * juce::MathConstants<float>::twoPi));
        const float w1 = 0.5f * (1.f - std::cos (t1 * juce::MathConstants<float>::twoPi));

        const float shimL = readBuf (shimPitchBufL, shimReadPos0) * w0
                          + readBuf (shimPitchBufL, shimReadPos1) * w1;
        const float shimR = readBuf (shimPitchBufR, shimReadPos0) * w0
                          + readBuf (shimPitchBufR, shimReadPos1) * w1;

        buffer.addSample (0, s, shimL * shimmerAmt);
        if (hasStereo) buffer.addSample (1, s, shimR * shimmerAmt);

        shimReadPos0 = std::fmod (shimReadPos0 + 2.f, (float) pitchBufSz);
        shimReadPos1 = std::fmod (shimReadPos1 + 2.f, (float) pitchBufSz);
    }

    // Step 2: reverb always at 100% wet internally — keeps the feedback loop
    // independent of the mix knob (mix is applied manually in step 4).
    juce::dsp::Reverb::Parameters rp;
    rp.roomSize   = 0.65f + params.p1 * 0.35f;
    rp.damping    = params.p2;
    rp.width      = 1.0f;
    rp.wetLevel   = 1.0f;
    rp.dryLevel   = 0.0f;
    rp.freezeMode = 0.f;
    shimReverb.setParameters (rp);

    juce::dsp::AudioBlock<float>              block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx   (block);
    shimReverb.process (ctx);

    // Step 3: write reverb output to pitch buffer — hard-clamped to ±2 to
    // prevent NaN/Inf from escaping into the feedback loop on very loud input.
    for (int s = 0; s < numSamples; ++s)
    {
        shimPitchBufL[shimPitchWrite] = juce::jlimit (-2.f, 2.f, buffer.getSample (0, s));
        shimPitchBufR[shimPitchWrite] = juce::jlimit (-2.f, 2.f,
            hasStereo ? buffer.getSample (1, s) : buffer.getSample (0, s));
        shimPitchWrite = (shimPitchWrite + 1) % pitchBufSz;
    }

    // Step 4: manual dry/wet blend — mix knob is now outside the feedback path
    for (int s = 0; s < numSamples; ++s)
    {
        buffer.setSample (0, s, shimDryL[s] * (1.f - mix) + buffer.getSample (0, s) * mix);
        if (hasStereo)
            buffer.setSample (1, s, shimDryR[s] * (1.f - mix) + buffer.getSample (1, s) * mix);
    }
}

void ShimmerEffect::reset()
{
    shimReverb.reset();
    std::fill (shimPitchBufL.begin(), shimPitchBufL.end(), 0.f);
    std::fill (shimPitchBufR.begin(), shimPitchBufR.end(), 0.f);
    std::fill (shimDryL.begin(), shimDryL.end(), 0.f);
    std::fill (shimDryR.begin(), shimDryR.end(), 0.f);
    shimPitchWrite = 0;
    shimReadPos0   = 0.f;
    shimReadPos1   = (float) shimPitchBufL.size() * 0.5f;
}

//==============================================================================
// LushChorusEffect
//==============================================================================
// p1 = Rate (0–1 → 0.1–4.0 Hz base)
// p2 = Depth (0–1 → 1–9 ms)
// p3 = Feedback (0–1 → 0–40%)
// 6 voices with slightly different LFO rates and alternating L/R pan for width.

void LushChorusEffect::prepare (double sr, int maxBlock)
{
    sampleRate = sr;

    // Max ~25ms delay per voice
    const int maxLushDelay = (int) (sr * 0.025) + 4;
    for (int v = 0; v < kLushVoices; ++v)
    {
        lushBufL[v].assign ((size_t) maxLushDelay, 0.f);
        lushBufR[v].assign ((size_t) maxLushDelay, 0.f);
        lushWritePos[v] = 0;
        lushLfoPhase[v] = (float) v / (float) kLushVoices;  // staggered phases
    }

    juce::ignoreUnused (maxBlock);
}

void LushChorusEffect::process (juce::AudioBuffer<float>& buffer,
                                 const FXSlotParams& params, double)
{
    const int  numSamples = buffer.getNumSamples();
    const bool hasStereo  = buffer.getNumChannels() > 1;
    const float baseRate  = params.p1 * 3.9f + 0.1f;  // 0.1–4.0 Hz
    const float depthMs   = params.p2 * 8.f + 1.f;    // 1–9 ms
    const float feedback  = params.p3 * 0.4f;          // 0–40%
    const float mix       = params.mix;

    static constexpr float rateMults[kLushVoices] = { 1.0f, 1.13f, 0.87f, 1.07f, 0.73f, 1.19f };
    static constexpr float panL     [kLushVoices] = { 1.0f, 0.7f,  1.0f,  0.7f,  1.0f,  0.7f  };
    static constexpr float panR     [kLushVoices] = { 0.7f, 1.0f,  0.7f,  1.0f,  0.7f,  1.0f  };
    static constexpr float baseDelMs[kLushVoices] = { 5.f, 6.5f, 8.f, 7.f, 10.f, 9.f };

    for (int s = 0; s < numSamples; ++s)
    {
        const float inL = buffer.getSample (0, s);
        const float inR = hasStereo ? buffer.getSample (1, s) : inL;
        float outL = 0.f, outR = 0.f;

        for (int v = 0; v < kLushVoices; ++v)
        {
            lushLfoPhase[v] = std::fmod (lushLfoPhase[v]
                                         + rateMults[v] * baseRate / (float) sampleRate, 1.f);
            const float lfoSine = std::sin (lushLfoPhase[v] * juce::MathConstants<float>::twoPi);

            const float delaySamples = juce::jlimit (
                1.f, (float) lushBufL[v].size() - 2.f,
                (baseDelMs[v] + lfoSine * depthMs) * (float) sampleRate / 1000.f);
            const int bufSz = (int) lushBufL[v].size();

            const int   readIdx  = (int) std::fmod (
                (float) lushWritePos[v] - delaySamples + (float) bufSz, (float) bufSz);
            const int   readIdx1 = (readIdx + 1) % bufSz;
            const float frac     = std::fmod (
                (float) lushWritePos[v] - delaySamples + (float) bufSz, 1.f);

            const float delayedL = lushBufL[v][readIdx] * (1.f - frac) + lushBufL[v][readIdx1] * frac;
            const float delayedR = lushBufR[v][readIdx] * (1.f - frac) + lushBufR[v][readIdx1] * frac;

            lushBufL[v][lushWritePos[v]] = inL + delayedL * feedback;
            lushBufR[v][lushWritePos[v]] = inR + delayedR * feedback;

            outL += delayedL * panL[v];
            outR += delayedR * panR[v];

            lushWritePos[v] = (lushWritePos[v] + 1) % bufSz;
        }

        constexpr float norm = 1.f / (float) kLushVoices;
        outL *= norm;
        outR *= norm;

        buffer.setSample (0, s, inL * (1.f - mix) + outL * mix);
        if (hasStereo) buffer.setSample (1, s, inR * (1.f - mix) + outR * mix);
    }
}

void LushChorusEffect::reset()
{
    for (int v = 0; v < kLushVoices; ++v)
    {
        std::fill (lushBufL[v].begin(), lushBufL[v].end(), 0.f);
        std::fill (lushBufR[v].begin(), lushBufR[v].end(), 0.f);
        lushWritePos[v] = 0;
    }
}

//==============================================================================
// FXChain
//==============================================================================

FXChain::FXChain()
{
    slotTypes.fill (FXType::None);
}

std::unique_ptr<IEffectProcessor> FXChain::createEffect (FXType type, double sr, int maxBlock)
{
    std::unique_ptr<IEffectProcessor> e;
    switch (type)
    {
        case FXType::Reverb:        e = std::make_unique<ReverbEffect>();      break;
        case FXType::Delay:         e = std::make_unique<DelayEffect>();       break;
        case FXType::Chorus:        e = std::make_unique<ChorusEffect>();      break;
        case FXType::Distortion:    e = std::make_unique<DistortionEffect>();  break;
        case FXType::Filter:        e = std::make_unique<FilterEffect>();      break;
        case FXType::ShimmerReverb: e = std::make_unique<ShimmerEffect>();     break;
        case FXType::LushChorus:    e = std::make_unique<LushChorusEffect>();  break;
        default:                    return nullptr;   // FXType::None
    }
    e->prepare (sr, maxBlock);
    return e;
}

void FXChain::prepare (double sr, int maxBlock)
{
    storedSR       = sr;
    storedMaxBlock = maxBlock;

    for (int i = 0; i < numSlots; ++i)
    {
        if (slots[i])
            slots[i]->prepare (sr, maxBlock);
    }
}

void FXChain::reset()
{
    for (int i = 0; i < numSlots; ++i)
    {
        if (slots[i]) slots[i]->reset();
    }
}

void FXChain::process (juce::AudioBuffer<float>&            buffer,
                        const std::array<FXSlotParams, numSlots>& params,
                        double bpm)
{
    for (int i = 0; i < numSlots; ++i)
    {
        const auto& p = params[i];

        if (p.bypass || p.type == FXType::None)
        {
            // Clear slot instance if type changed to None (frees its memory)
            if (slotTypes[i] != FXType::None)
            {
                slots[i].reset();
                slotTypes[i] = FXType::None;
            }
            continue;
        }

        // Swap instance when the effect type changes
        if (p.type != slotTypes[i])
        {
            slots[i]    = createEffect (p.type, storedSR, storedMaxBlock);
            slotTypes[i] = p.type;
        }

        if (slots[i])
            slots[i]->process (buffer, p, bpm);
    }
}
