#include "GranularEngine.h"
#include <cmath>

void GranularEngine::prepare (double sr, int maxBlockSize)
{
    sampleRate = sr;
    grainPool.reset();
    scheduler.reset();
    wetBuffer.setSize (2, maxBlockSize);
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
    grainPool.reset();
    scheduler.reset();
    const bool ok = sampleBuffer.loadFile (file, sampleRate);

    if (ok)
    {
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
            adsr.noteOn();
        }
        else if (msg.isNoteOff())
        {
            const int note = msg.getNoteNumber();
            for (int i = 0; i < noteStackSize; ++i)
                if (noteStack[i] == note) { removeFromStack (i); break; }
            if (noteStackSize > 0)
                currentMidiNote.store (noteStack[noteStackSize - 1]);
            else
            { currentMidiNote.store (-1); adsr.noteOff(); }
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            noteStackSize = 0;
            currentMidiNote.store (-1);
            adsr.noteOff();
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
    handleMidi (midi);

    const int numSamples = output.getNumSamples();

    // ── Update ADSR parameters ────────────────────────────────────────────────
    {
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

    // ── LFO 1/2/3 — compute and sum modulation onto shared targets ───────────
    // LFO 1: apply BPM sync when enabled.
    // syncMults maps lfoSyncDiv index to note duration in beats
    // {"1/32","1/16","1/8","1/4","1/2","1 bar","2 bars","4 bars"}
    // LFO rate (Hz) = bpm/60 / beats_per_cycle
    float lfoRate1 = p.lfoRate;
    if (p.lfoSync && p.bpm > 0.0)
    {
        static constexpr double syncMults[] = { 0.125, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 };
        const int syncDiv = juce::jlimit (0, 7, p.lfoSyncDiv);
        lfoRate1 = (float) (p.bpm / 60.0 / syncMults[syncDiv]);
    }

    const float lfoRaw1 = calcLFO (lfoRate1,   p.lfoShape,  numSamples, lfoPhase,  lfoSH1);
    const float lfoRaw2 = calcLFO (p.lfoRate2, p.lfoShape2, numSamples, lfoPhase2, lfoSH2);
    const float lfoRaw3 = calcLFO (p.lfoRate3, p.lfoShape3, numSamples, lfoPhase3, lfoSH3);

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

    // ── Envelope follower modulation ──────────────────────────────────────────
    if (p.envActive && p.envTarget > 0 && p.envDepth > 0.001f)
    {
        const float envVal = (envFollower.getValue() * 2.f - 1.f) * p.envDepth;
        switch (p.envTarget)
        {
            case 1: grainSizeMs = juce::jlimit (5.f,   500.f, grainSizeMs + envVal * 200.f); break;
            case 2: density     = juce::jlimit (1.f,   200.f, density     + envVal * 80.f);  break;
            case 3: position    = juce::jlimit (0.f,   1.f,   position    + envVal * 0.5f);  break;
            case 4: pitchShift  = juce::jlimit (-24.f, 24.f,  pitchShift  + envVal * 12.f);  break;
            case 5: panSpread   = juce::jlimit (0.f,   1.f,   panSpread   + envVal * 0.5f);  break;
            default: break;
        }
    }

    // ── Step sequencer: overrides position when active ────────────────────────
    if (p.seqActive)
    {
        const int   divIdx = juce::jlimit (0, 8, p.beatDivision);
        const float seqPos = stepSequencer.process (p.seqSteps, numSamples,
                                                     sampleRate, p.bpm, divIdx, true);
        if (seqPos >= 0.f)
            position = seqPos;
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

    // ── Pitch ratio ───────────────────────────────────────────────────────────
    if (midiNote >= 0) lastActiveMidiNote = midiNote;

    const double pitchRatio = midiNoteToPitchRatio (lastActiveMidiNote)
                            * std::pow (2.0, (double) pitchShift / 12.0);

    // ── Scheduler ────────────────────────────────────────────────────────────
    const auto& source = sampleBuffer.getBuffer();

    scheduler.process (numSamples, sampleRate, grainPool,
                       source.getNumSamples(),
                       grainSizeMs, density, position, p.posJitter,
                       pitchRatio, p.pitchJitter, panSpread,
                       windowType, true,
                       p.pitchScale, p.pitchRoot);

    // ── Process pool into wet buffer ──────────────────────────────────────────
    if (numSamples > wetBuffer.getNumSamples())
        wetBuffer.setSize (2, numSamples, false, true, true);
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

    // ── Envelope follower update ──────────────────────────────────────────────
    {
        float rmsSum = 0.f;
        for (int ch = 0; ch < wetBuffer.getNumChannels(); ++ch)
            rmsSum += wetBuffer.getRMSLevel (ch, 0, numSamples);
        const float wetRms = rmsSum / (float) juce::jmax (1, wetBuffer.getNumChannels());

        const float attCoeff = EnvelopeFollower::makeCoeff (p.envAttack,  sampleRate, numSamples);
        const float relCoeff = EnvelopeFollower::makeCoeff (p.envRelease, sampleRate, numSamples);
        envFollower.process (wetRms, attCoeff, relCoeff);
        envFollowValueAtomic.store (envFollower.getValue());
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
