#include "GranularEngine.h"
#include <cmath>

void GranularEngine::prepare (double sr, int maxBlockSize)
{
    sampleRate = sr;
    grainPool.reset();
    scheduler.reset();
    stepSequencer.reset();   // fix: was missing — first block always skipped step 0
    wetBuffer.setSize (2, maxBlockSize, false, true, false);  // pre-alloc to max; no audio-thread realloc
    adsr.setSampleRate (sr);

    // 50ms ramp eliminates zipper noise on output gain automation
    outputGainSmoothed.reset (sr, 0.05);
    outputGainSmoothed.setCurrentAndTargetValue (1.0f);
}

void GranularEngine::releaseResources()
{
    grainPool.reset();
    scheduler.reset();
    adsr.reset();
    // Free the wet buffer heap allocation; will be reallocated in prepare()
    wetBuffer.setSize (0, 0);
}

bool GranularEngine::loadSample (const juce::File& file)
{
    // Load first so the atomic buffer flip happens before the reset flag is raised.
    // The audio thread may read the new buffer for one block before resetting the
    // grain pool, but all positions are clamped to buffer bounds so there is no crash.
    const bool ok = sampleBuffer.loadFile (file, sampleRate);

    if (ok)
    {
        // Signal the audio thread to reset pool/scheduler/sequencer safely.
        // We must NOT call grainPool.reset() here — that struct is owned by the
        // audio thread and iterating it concurrently is a data race.
        resetRequested.store (true, std::memory_order_release);

        const int detected = PitchDetector::detectMidiNote (sampleBuffer.getBuffer(), sampleRate);
        detectedRootNote.store (detected >= 0 ? detected : 60);
    }

    return ok;
}

//==============================================================================
void GranularEngine::handleMidi (juce::MidiBuffer& midi)
{
    auto removeFromStack = [this] (int idx)
    {
        for (int j = idx; j < noteStackSize - 1; ++j)
            noteStack[j] = noteStack[j + 1];
        --noteStackSize;
    };

    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();

        if (msg.isNoteOn())
        {
            const int note = msg.getNoteNumber();
            for (int i = 0; i < noteStackSize; ++i)
                if (noteStack[i] == note) { removeFromStack (i); break; }
            if (noteStackSize < kNoteStackSize)
                noteStack[noteStackSize++] = note;
            currentMidiNote.store (note);
            lastVelocity = juce::jlimit (0.f, 1.f, msg.getVelocity() / 127.f);
            adsr.noteOn();
            adsrVizNoteOn  = true;
            adsrVizNoteOff = false;
        }
        else if (msg.isNoteOff())
        {
            const int note = msg.getNoteNumber();
            for (int i = 0; i < noteStackSize; ++i)
                if (noteStack[i] == note) { removeFromStack (i); break; }
            if (noteStackSize > 0)
                currentMidiNote.store (noteStack[noteStackSize - 1]);
            else
            {
                currentMidiNote.store (-1);
                adsr.noteOff();
                if (!adsrVizNoteOn)   // don't overwrite a simultaneous noteOn flag
                    adsrVizNoteOff = true;
            }
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            noteStackSize = 0;
            currentMidiNote.store (-1);
            adsr.noteOff();
            if (!adsrVizNoteOn)
                adsrVizNoteOff = true;
        }
    }
}

double GranularEngine::midiNoteToPitchRatio (int note) const
{
    return std::pow (2.0, (double) (note - detectedRootNote.load()) / 12.0);
}

//==============================================================================
void GranularEngine::process (juce::AudioBuffer<float>& output,
                               juce::MidiBuffer&         midi,
                               const GrainParams&        p)
{
    // Drain any pending pool reset requested by loadSample() on the message thread.
    // Doing the reset here (audio thread) eliminates the data race on grainPool.
    if (resetRequested.load (std::memory_order_acquire))
    {
        grainPool.reset();
        scheduler.reset();
        stepSequencer.reset();
        resetRequested.store (false, std::memory_order_release);
    }

    handleMidi (midi);

    const int numSamples = output.getNumSamples();

    // ── Update ADSR parameters ────────────────────────────────────────────────
    {
        // Standard ADSR: decay is the rate knob (time to decay from peak to 0).
        // Audible decay time = decay * (1 - sustain) — lower sustain gives more
        // noticeable decay.  This matches hardware synth (Moog, etc.) behaviour
        // and keeps the visualisation and audio in sync.
        juce::ADSR::Parameters ap;
        ap.attack  = p.attack;
        ap.decay   = p.decay;
        ap.sustain = p.sustain;
        ap.release = p.release;
        adsr.setParameters (ap);
    }

    // ── Determine if we should produce sound ─────────────────────────────────
    const int  midiNote = currentMidiNote.load();
    const bool isActive = sampleBuffer.hasContent()
                          && (midiNote >= 0 || adsr.isActive());

    if (!isActive)
    {
        output.clear();
        return;
    }

    // ── ADSR visualisation cursor ─────────────────────────────────────────────
    // Consume flags set by handleMidi and advance the state machine.
    {
        if (adsrVizNoteOn)
        {
            adsrVizStage   = AdsrVizStage::Attack;
            adsrVizTimeSec = 0.0;
            adsrVizNoteOn  = false;
            adsrVizNoteOff = false;
        }
        else if (adsrVizNoteOff)
        {
            adsrVizStage   = AdsrVizStage::Release;
            adsrVizTimeSec = 0.0;
            adsrVizNoteOff = false;
        }

        const double blockSec = (double) numSamples / sampleRate;
        float cursor = -1.f;

        switch (adsrVizStage)
        {
            case AdsrVizStage::Idle:
                cursor = -1.f;
                break;
            case AdsrVizStage::Attack:
                adsrVizTimeSec += blockSec;
                if (adsrVizTimeSec >= (double) p.attack)
                    { adsrVizStage = AdsrVizStage::Decay; adsrVizTimeSec = 0.0; cursor = 0.25f; }
                else
                    cursor = 0.25f * (float) (adsrVizTimeSec / juce::jmax (0.001, (double) p.attack));
                break;
            case AdsrVizStage::Decay:
                adsrVizTimeSec += blockSec;
                if (adsrVizTimeSec >= (double) p.decay)
                    { adsrVizStage = AdsrVizStage::Sustain; adsrVizTimeSec = 0.0; cursor = 0.5f; }
                else
                    cursor = 0.25f + 0.25f * (float) (adsrVizTimeSec / juce::jmax (0.001, (double) p.decay));
                break;
            case AdsrVizStage::Sustain:
                cursor = 0.5f;
                break;
            case AdsrVizStage::Release:
                adsrVizTimeSec += blockSec;
                if (adsrVizTimeSec >= (double) p.release)
                    { adsrVizStage = AdsrVizStage::Idle; adsrVizTimeSec = 0.0; cursor = -1.f; }
                else
                    cursor = 0.75f + 0.25f * (float) (adsrVizTimeSec / juce::jmax (0.001, (double) p.release));
                break;
        }
        adsrCursorAtomic.store (cursor, std::memory_order_release);
    }

    // ── LFO 1/2/3 — compute and sum modulation onto shared targets ───────────
    const float lfoRate1 = p.lfoRate;
    const float lfoRate2 = p.lfoRate2;
    const float lfoRate3 = p.lfoRate3;

    const float lfoRaw1 = calcLFO (lfoRate1, p.lfoShape,  numSamples, lfoPhase,  lfoSH1);
    const float lfoRaw2 = calcLFO (lfoRate2, p.lfoShape2, numSamples, lfoPhase2, lfoSH2);
    const float lfoRaw3 = calcLFO (lfoRate3, p.lfoShape3, numSamples, lfoPhase3, lfoSH3);

    lfoPhaseAtomic.store  ((float) lfoPhase);   lfoOutputAtomic.store  (lfoRaw1);
    lfoPhase2Atomic.store ((float) lfoPhase2);  lfoOutput2Atomic.store (lfoRaw2);
    lfoPhase3Atomic.store ((float) lfoPhase3);  lfoOutput3Atomic.store (lfoRaw3);

    const float lfoVal1 = lfoRaw1 * p.lfoDepth;
    const float lfoVal2 = lfoRaw2 * p.lfoDepth2;
    const float lfoVal3 = lfoRaw3 * p.lfoDepth3;

    // ── Grain parameters — apply LFO offsets to targets ──────────────────────
    float grainSizeMs = p.grainSizeMs;
    float density     = p.density;
    float position    = p.position;
    float pitchShift  = p.pitchShift;
    float panSpread   = p.panSpread;

    auto applyLFO = [&] (int target, float val)
    {
        switch (target)
        {
            case 1: grainSizeMs = juce::jlimit (5.f,   500.f, grainSizeMs + val * 200.f); break;
            case 2: density     = juce::jlimit (1.f,   200.f, density     + val * 80.f);  break;
            case 3: position    = juce::jlimit (0.f,   1.f,   position    + val * 0.5f);  break;
            case 4: pitchShift  = juce::jlimit (-24.f, 24.f,  pitchShift  + val * 12.f);  break;
            case 5: panSpread   = juce::jlimit (0.f,   1.f,   panSpread   + val * 0.5f);  break;
            default: break;
        }
    };
    applyLFO (p.lfoTarget,  lfoVal1);
    applyLFO (p.lfoTarget2, lfoVal2);
    applyLFO (p.lfoTarget3, lfoVal3);

    // ── Macro knobs M1–M4: additive modulation on their assigned targets ───────
    // Each macro value 0–1 maps to output −1..+1, identical scale to LFO output.
    // Default knob value 0.5 → output 0 (no effect).
    applyLFO (p.m1Target, (p.m1 - 0.5f) * 2.f);
    applyLFO (p.m2Target, (p.m2 - 0.5f) * 2.f);
    applyLFO (p.m3Target, (p.m3 - 0.5f) * 2.f);
    applyLFO (p.m4Target, (p.m4 - 0.5f) * 2.f);

    // ── Grain freeze: lock position when active ───────────────────────────────
    // On the rising edge (off → on), snapshot the current modulated position.
    // While frozen, skip sequencer and LFO position changes (already applied
    // above, so override here with the snapshot).
    if (p.freeze && !prevFreeze)
        frozenPosition = position;
    if (p.freeze)
        position = frozenPosition;
    prevFreeze = p.freeze;

    // ── Step sequencer: overrides position when active (skip if frozen) ───────
    if (p.seqActive && !p.freeze)
    {
        const int   divIdx = juce::jlimit (0, 8, p.beatDivision);
        const float seqPos = stepSequencer.process (p.seqSteps, numSamples,
                                                     sampleRate, p.bpm, divIdx, true);
        if (seqPos >= 0.f)
            position = seqPos;
    }
    else if (!p.seqActive)
    {
        // Still need to tick the sequencer so it stays in sync even when inactive
        stepSequencer.process (p.seqSteps, numSamples, sampleRate, p.bpm,
                               juce::jlimit (0, 8, p.beatDivision), false);
    }

    // ── Publish final modulated position for waveform display ─────────────────
    modulatedPositionAtomic.store (position);

    // ── Beat sync ─────────────────────────────────────────────────────────────
    if (p.beatSync && p.bpm > 0.0)
    {
        const int    divIdx  = juce::jlimit (0, 8, p.beatDivision);
        static constexpr double mults[] = { 0.125, 1.0/6.0, 0.25, 1.0/3.0, 0.5,
                                            2.0/3.0, 1.0, 2.0, 4.0 };
        const double divSec  = (60.0 / p.bpm) * mults[divIdx];
        density = (float) (1.0 / divSec);
    }

    const WindowType windowType = static_cast<WindowType> (juce::jlimit (0, 3, p.windowType));

    // ── Pitch ratio with portamento / glide ───────────────────────────────────
    if (midiNote >= 0) lastActiveMidiNote = midiNote;

    const double targetMidiRatio = midiNoteToPitchRatio (lastActiveMidiNote);

    // Exponential smoothing toward target MIDI pitch ratio.
    // coeff → 0 when glideTime → 0 (instant snap), → 1 for very long glide.
    if (p.glideTime < 0.001f)
    {
        glidePitchRatio = targetMidiRatio;
    }
    else
    {
        const double blocksPerSec = sampleRate / (double) numSamples;
        const double coeff = std::exp (-1.0 / (p.glideTime * blocksPerSec));
        glidePitchRatio = targetMidiRatio + coeff * (glidePitchRatio - targetMidiRatio);
    }

    const double pitchRatio = glidePitchRatio
                            * std::pow (2.0, (double) pitchShift / 12.0);

    // ── Velocity scale: blend between full-amplitude and velocity-proportional ─
    // velocityDepth 0 = always full amplitude; 1 = amplitude equals MIDI velocity.
    const float velocityScale = 1.f - p.velocityDepth + p.velocityDepth * lastVelocity;

    // ── Scheduler ────────────────────────────────────────────────────────────
    const auto& source = sampleBuffer.getBuffer();

    scheduler.process (numSamples, sampleRate, grainPool,
                       source.getNumSamples(),
                       grainSizeMs, density, position, p.posJitter,
                       pitchRatio, p.pitchJitter, panSpread,
                       windowType, true,
                       p.pitchScale, p.pitchRoot,
                       p.reverseAmount, velocityScale);

    // ── Process pool into wet buffer ──────────────────────────────────────────
    // wetBuffer is pre-allocated to maxBlockSize in prepare(); no allocation here.
    jassert (numSamples <= wetBuffer.getNumSamples());
    wetBuffer.clear();
    grainPool.process (wetBuffer, source);

    // Safety clamp: coherent grains (low jitter / fixed SEQ position) can
    // temporarily exceed 0 dBFS.  ±4 FS headroom still passes legitimate
    // loud transients but prevents NaN / Inf from escaping to the host.
    for (int ch = 0; ch < wetBuffer.getNumChannels(); ++ch)
    {
        auto* data = wetBuffer.getWritePointer (ch);
        for (int s = 0; s < numSamples; ++s)
            data[s] = juce::jlimit (-4.f, 4.f, data[s]);
    }

    // ── ADSR envelope ─────────────────────────────────────────────────────────
    adsr.applyEnvelopeToBuffer (wetBuffer, 0, numSamples);

    // ── Output gain (smoothed to prevent zipper noise on automation) ──────────
    outputGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (p.outputGainDb));
    outputGainSmoothed.applyGain (wetBuffer, numSamples);

    // ── Copy wet buffer to output ─────────────────────────────────────────────
    output.clear();
    for (int ch = 0; ch < output.getNumChannels(); ++ch)
    {
        const int wetCh = juce::jmin (ch, wetBuffer.getNumChannels() - 1);
        output.addFrom (ch, 0, wetBuffer, wetCh, 0, numSamples, 1.0f);
    }
}

//==============================================================================
float GranularEngine::calcLFO (float rate, int shape, int numSamples,
                                double& phase, float& lastSH) noexcept
{
    const double advance = (double) rate * (double) numSamples / sampleRate;
    phase = std::fmod (phase + advance, 1.0);
    const float p = (float) phase;

    // S&H (shape 4): update held value on each new cycle
    if (shape == 4)
    {
        if (p < (float) advance)
            lastSH = lfoRandom.nextFloat() * 2.f - 1.f;
        return lastSH;
    }

    // All other shapes delegate to the shared evaluator
    return LFOFunction::evaluate (shape, p);
}
