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

    // Dry save-buffer: pre-sized to maxBlock so the audio thread never reallocates.
    // Use resize (not assign) to retain capacity without re-zeroing on repeat calls.
    shimDryL.resize ((size_t) maxBlock, 0.f);
    shimDryR.resize ((size_t) maxBlock, 0.f);
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

    // Guard: if a non-conformant host sends more samples than maxBlock, grow the
    // dry-save buffer here (message-thread resize already happened in prepare();
    // this is a safety fallback — allocating on audio thread is last resort).
    if (numSamples > (int) shimDryL.size())
    {
        shimDryL.resize ((size_t) numSamples, 0.f);
        shimDryR.resize ((size_t) numSamples, 0.f);
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

    // NaN/Inf guard: JUCE's reverb can enter a bad state with extreme room sizes
    // or loud transients.  Detect corruption, reset everything, and restore dry.
    {
        bool bad = false;
        for (int s = 0; s < numSamples && !bad; ++s)
        {
            if (!std::isfinite (buffer.getSample (0, s))) bad = true;
            if (hasStereo && !std::isfinite (buffer.getSample (1, s))) bad = true;
        }
        if (bad)
        {
            shimReverb.reset();
            std::fill (shimPitchBufL.begin(), shimPitchBufL.end(), 0.f);
            std::fill (shimPitchBufR.begin(), shimPitchBufR.end(), 0.f);
            shimPitchWrite = 0;
            shimReadPos0   = 0.f;
            shimReadPos1   = (float) shimPitchBufL.size() * 0.5f;
            // Output dry signal — the reverb trail is lost but audio continues
            for (int s = 0; s < numSamples; ++s)
            {
                buffer.setSample (0, s, shimDryL[s]);
                if (hasStereo) buffer.setSample (1, s, shimDryR[s]);
            }
            return;
        }
    }

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
// FlangerEffect
//==============================================================================
// p1 = Rate    (0–1 → 0.05–5 Hz)
// p2 = Depth   (0–1 → delay mod depth 0–7ms around 1ms centre)
// p3 = Feedback (-0.9..0.9, mapped from 0–1 → -0.9..0.9)
// Classic single-delay comb flanger with quadrature LFOs for stereo spread.

void FlangerEffect::prepare (double sr, int maxBlock)
{
    sampleRate = sr;
    std::fill (std::begin (bufL), std::end (bufL), 0.f);
    std::fill (std::begin (bufR), std::end (bufR), 0.f);
    writePos   = 0;
    lfoPhaseL  = 0.f;
    lfoPhaseR  = 0.5f;
    juce::ignoreUnused (maxBlock);
}

void FlangerEffect::process (juce::AudioBuffer<float>& buffer,
                              const FXSlotParams& params, double)
{
    const int   numSamples = buffer.getNumSamples();
    const bool  hasStereo  = buffer.getNumChannels() > 1;

    const float rate       = params.p1 * 4.95f + 0.05f;           // 0.05–5 Hz
    const float depth      = params.p2 * 7.f;                      // 0–7 ms mod depth
    const float feedback   = (params.p3 * 2.f - 1.f) * 0.88f;    // -0.88..+0.88
    const float mix        = params.mix;
    const float lfoInc     = rate / (float) sampleRate;

    const float centreMs   = 1.f;   // centre delay in ms
    const float centreSmpl = centreMs * (float) sampleRate / 1000.f;
    const float depthSmpl  = depth   * (float) sampleRate / 1000.f;

    for (int s = 0; s < numSamples; ++s)
    {
        const float inL = buffer.getSample (0, s);
        const float inR = hasStereo ? buffer.getSample (1, s) : inL;

        // Sinusoidal LFO — quadrature pair (L/R differ by 180° for width)
        const float lfoL = std::sin (lfoPhaseL * juce::MathConstants<float>::twoPi);
        const float lfoR = std::sin (lfoPhaseR * juce::MathConstants<float>::twoPi);

        const float delayL = juce::jlimit (1.f, (float)(kMaxDelaySamples - 2),
                                           centreSmpl + lfoL * depthSmpl);
        const float delayR = juce::jlimit (1.f, (float)(kMaxDelaySamples - 2),
                                           centreSmpl + lfoR * depthSmpl);

        // Fractional delay read (linear interpolation)
        auto readBuf = [] (const float* buf, int writeIdx, float delay) -> float
        {
            const float readF = (float)writeIdx - delay;
            int   i0 = (int)readF;
            const float frac  = readF - (float)i0;
            i0 = ((i0 % kMaxDelaySamples) + kMaxDelaySamples) % kMaxDelaySamples;
            const int i1 = (i0 + 1) % kMaxDelaySamples;
            return buf[i0] * (1.f - frac) + buf[i1] * frac;
        };

        const float wetL = readBuf (bufL, writePos, delayL);
        const float wetR = readBuf (bufR, writePos, delayR);

        // Write input + feedback into delay line
        bufL[writePos] = juce::jlimit (-2.f, 2.f, inL + wetL * feedback);
        bufR[writePos] = juce::jlimit (-2.f, 2.f, inR + wetR * feedback);

        buffer.setSample (0, s, inL * (1.f - mix) + (inL + wetL) * 0.5f * mix);
        if (hasStereo)
            buffer.setSample (1, s, inR * (1.f - mix) + (inR + wetR) * 0.5f * mix);

        writePos  = (writePos + 1) % kMaxDelaySamples;
        lfoPhaseL = std::fmod (lfoPhaseL + lfoInc, 1.f);
        lfoPhaseR = std::fmod (lfoPhaseR + lfoInc, 1.f);
    }
}

void FlangerEffect::reset()
{
    std::fill (std::begin (bufL), std::end (bufL), 0.f);
    std::fill (std::begin (bufR), std::end (bufR), 0.f);
    writePos  = 0;
    lfoPhaseL = 0.f;
    lfoPhaseR = 0.5f;
}

//==============================================================================
// HarmonicEffect
//==============================================================================
// Parallel exciter: HP-filter isolates the target band, soft-clip generates
// harmonics, those harmonics are added (not replaced) back to the dry signal.
// p1 = Drive (0–1 → 1–8x)
// p2 = Freq  (0–1 → 200–5000 Hz, HP corner of the excited band)
// p3 = Character (< 0.5 = even/warm via abs-saturation, ≥ 0.5 = odd/bright via tanh)
// mix = blend of harmonic content added on top of dry

void HarmonicEffect::prepare (double sr, int maxBlock)
{
    sampleRate = sr;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = (juce::uint32) maxBlock;
    spec.numChannels      = 2;
    hpFilter.prepare (spec);
    dryBuffer.setSize (2, maxBlock);
}

void HarmonicEffect::process (juce::AudioBuffer<float>& buffer,
                               const FXSlotParams& params, double)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples > dryBuffer.getNumSamples() || numChannels > dryBuffer.getNumChannels())
        dryBuffer.setSize (numChannels, numSamples, false, true, true);

    // Save dry signal — harmonics are added additively on top
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    // HP filter isolates the frequency band to excite (avoids muddying the low end)
    const float maxCutoff = (float) (sampleRate * 0.45);
    const float freq      = 200.f * std::pow (25.f, params.p2);   // 200–5000 Hz
    hpFilter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    hpFilter.setCutoffFrequency (juce::jlimit (20.f, maxCutoff, freq));
    hpFilter.setResonance (0.7f);

    {
        juce::dsp::AudioBlock<float>              block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx   (block);
        hpFilter.process (ctx);
    }

    // Waveshaper generates harmonics from the filtered band
    const float drive      = 1.f + params.p1 * 7.f;   // 1–8×
    const bool  evenChar   = (params.p3 < 0.5f);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int s = 0; s < numSamples; ++s)
        {
            const float x = data[s];
            // Even harmonics (warm/tube): full-wave rectified soft clip minus linear
            // Odd  harmonics (bright):    symmetric tanh saturation minus linear
            data[s] = evenChar ? (std::abs (std::tanh (x * drive)) - std::abs (x))
                               : (std::tanh (x * drive) - x);
        }
    }

    // Add harmonic content back to dry (additive exciter — no dry signal loss)
    for (int ch = 0; ch < numChannels; ++ch)
    {
        buffer.applyGain (ch, 0, numSamples, params.mix);
        buffer.addFrom   (ch, 0, dryBuffer, ch, 0, numSamples, 1.f);
    }
}

void HarmonicEffect::reset() { hpFilter.reset(); }

//==============================================================================
// AutoPanEffect
//==============================================================================
// Sinusoidal LFO drives constant-power stereo panning.
// p1 = Rate  (0–1 → 0.05–8 Hz)
// p2 = Depth (0–1, how far L/R the pan sweeps)
// p3 = unused (reserved)
// mix = blend between un-panned and panned signal

void AutoPanEffect::prepare (double sr, int maxBlock)
{
    sampleRate = sr;
    lfoPhase   = 0.f;
    juce::ignoreUnused (maxBlock);
}

void AutoPanEffect::process (juce::AudioBuffer<float>& buffer,
                              const FXSlotParams& params, double)
{
    const int   numSamples = buffer.getNumSamples();
    const bool  hasStereo  = buffer.getNumChannels() > 1;

    const float rate   = 0.05f + params.p1 * 7.95f;   // 0.05–8 Hz
    const float depth  = params.p2;                     // 0–1
    const float mix    = params.mix;
    const float lfoInc = rate / (float) sampleRate;

    for (int s = 0; s < numSamples; ++s)
    {
        const float inL = buffer.getSample (0, s);
        const float inR = hasStereo ? buffer.getSample (1, s) : inL;

        // Sine LFO: −1 (full left) to +1 (full right)
        const float lfo    = std::sin (lfoPhase * juce::MathConstants<float>::twoPi);
        const float panPos = lfo * depth;   // −depth … +depth

        // Constant-power law: map pan position to L/R gains
        // angle ∈ [0, π/2]: 0 = full left, π/4 = centre, π/2 = full right
        const float angle = (panPos + 1.f) * 0.5f * juce::MathConstants<float>::halfPi;
        const float gainL = std::cos (angle);
        const float gainR = std::sin (angle);

        buffer.setSample (0, s, inL * (1.f - mix) + inL * gainL * mix);
        if (hasStereo)
            buffer.setSample (1, s, inR * (1.f - mix) + inR * gainR * mix);

        lfoPhase = std::fmod (lfoPhase + lfoInc, 1.f);
    }
}

void AutoPanEffect::reset() { lfoPhase = 0.f; }

//==============================================================================
// FXChain
//==============================================================================

FXChain::FXChain()
{
    slotTypes.fill (FXType::None);
}

FXChain::~FXChain()
{
    // Drain both queues so no effects are leaked when the plugin is destroyed.
    // At teardown the audio thread is stopped, so single-thread access is safe.
    {
        int s1, bs1, s2, bs2;
        const int n = incomingFifo.getNumReady();
        incomingFifo.prepareToRead (n, s1, bs1, s2, bs2);
        for (int k = 0; k < bs1; ++k) delete incomingBuf[s1 + k].ptr;
        for (int k = 0; k < bs2; ++k) delete incomingBuf[s2 + k].ptr;
        incomingFifo.finishedRead (bs1 + bs2);
    }
    {
        int s1, bs1, s2, bs2;
        const int n = garbageFifo.getNumReady();
        garbageFifo.prepareToRead (n, s1, bs1, s2, bs2);
        for (int k = 0; k < bs1; ++k) delete garbageBuf[s1 + k];
        for (int k = 0; k < bs2; ++k) delete garbageBuf[s2 + k];
        garbageFifo.finishedRead (bs1 + bs2);
    }
    // slots[] unique_ptrs auto-delete here.
}

//==============================================================================
void FXChain::requestTypeChange (int slot, FXType newType)
{
    if (slot < 0 || slot >= numSlots)
        return;

    // Create the new effect on the message thread — allocation is fine here.
    IEffectProcessor* newEffect = nullptr;
    if (newType != FXType::None)
    {
        auto up  = createEffect (newType, storedSR, storedMaxBlock);
        newEffect = up.release();
    }

    // Enqueue for the audio thread.
    int s1, bs1, s2, bs2;
    incomingFifo.prepareToWrite (1, s1, bs1, s2, bs2);
    const int idx = (bs1 > 0) ? s1 : (bs2 > 0 ? s2 : -1);
    if (idx >= 0)
    {
        incomingBuf[idx] = { slot, newType, newEffect };
        incomingFifo.finishedWrite (1);
    }
    else
    {
        // Queue full — should never happen with kQueueSize=16 and 4 slots.
        jassertfalse;
        delete newEffect;
    }
}

void FXChain::collectGarbage()
{
    const int n = garbageFifo.getNumReady();
    if (n == 0) return;

    int s1, bs1, s2, bs2;
    garbageFifo.prepareToRead (n, s1, bs1, s2, bs2);
    for (int k = 0; k < bs1; ++k) delete garbageBuf[s1 + k];
    for (int k = 0; k < bs2; ++k) delete garbageBuf[s2 + k];
    garbageFifo.finishedRead (bs1 + bs2);
}

void FXChain::drainIncoming() noexcept
{
    const int n = incomingFifo.getNumReady();
    if (n == 0) return;

    int s1, bs1, s2, bs2;
    incomingFifo.prepareToRead (n, s1, bs1, s2, bs2);

    auto installRange = [&] (int start, int count)
    {
        for (int k = 0; k < count; ++k)
        {
            auto& cmd = incomingBuf[start + k];

            // Retire the current occupant to the garbage queue (audio thread cannot delete).
            IEffectProcessor* old = slots[cmd.slot].release();
            if (old != nullptr)
            {
                int gs1, gbs1, gs2, gbs2;
                garbageFifo.prepareToWrite (1, gs1, gbs1, gs2, gbs2);
                if (gbs1 > 0)      { garbageBuf[gs1] = old; garbageFifo.finishedWrite (1); }
                else if (gbs2 > 0) { garbageBuf[gs2] = old; garbageFifo.finishedWrite (1); }
                else               { jassertfalse; delete old; } // garbage full — should never happen
            }

            // Install the new effect (may be nullptr when type changed to None).
            slots[cmd.slot].reset (cmd.ptr);
            slotTypes[cmd.slot] = cmd.type;
        }
    };

    installRange (s1, bs1);
    installRange (s2, bs2);
    incomingFifo.finishedRead (bs1 + bs2);
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
        case FXType::ShimmerReverb: e = std::make_unique<ShimmerEffect>();      break;
        case FXType::LushChorus:    e = std::make_unique<LushChorusEffect>(); break;
        case FXType::Flanger:       e = std::make_unique<FlangerEffect>();    break;
        case FXType::Harmonic:      e = std::make_unique<HarmonicEffect>();   break;
        case FXType::AutoPan:       e = std::make_unique<AutoPanEffect>();    break;
        default:                    return nullptr;   // FXType::None
    }
    e->prepare (sr, maxBlock);
    return e;
}

void FXChain::prepare (double sr, int maxBlock)
{
    storedSR       = sr;
    storedMaxBlock = maxBlock;

    // Drain any pending type changes first so they are installed (and prepared
    // at the correct sample rate) before audio processing begins.
    drainIncoming();

    for (int i = 0; i < numSlots; ++i)
        if (slots[i]) slots[i]->prepare (sr, maxBlock);
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
    // Pick up any pending type changes enqueued by requestTypeChange() on the
    // message thread.  No allocation happens here — effects are already created.
    drainIncoming();

    for (int i = 0; i < numSlots; ++i)
    {
        const auto& p = params[i];

        // Empty slot: nothing to do (drainIncoming() already cleared it).
        if (p.type == FXType::None)
            continue;

        // Bypassed: keep the effect instance alive (preserves reverb tail, delay
        // echo, chorus LFO state, etc.) but skip processing.
        if (p.bypass)
            continue;

        if (slots[i])
            slots[i]->process (buffer, p, bpm);
    }
}
