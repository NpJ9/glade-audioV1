#include "GranularEngine.h"
#include <cmath>

void GranularEngine::prepare (double sr, int maxBlockSize)
{
    sampleRate = sr;
    grainPool.reset();
    scheduler.reset();
    wetBuffer.setSize (2, maxBlockSize);
    adsr.setSampleRate (sr);
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
    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            currentMidiNote.store (msg.getNoteNumber());
            adsr.noteOn();
        }
        else if (msg.isNoteOff() && msg.getNoteNumber() == currentMidiNote.load())
        {
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
void GranularEngine::process (juce::AudioBuffer<float>&           output,
                               juce::MidiBuffer&                   midi,
                               juce::AudioProcessorValueTreeState& apvts,
                               double bpm)
{
    handleMidi (midi);

    const int numSamples = output.getNumSamples();

    // ── Update ADSR parameters from APVTS ────────────────────────────────────
    {
        juce::ADSR::Parameters p;
        p.attack  = apvts.getRawParameterValue ("attack") ->load();
        p.decay   = apvts.getRawParameterValue ("decay")  ->load();
        p.sustain = apvts.getRawParameterValue ("sustain")->load();
        p.release = apvts.getRawParameterValue ("release")->load();
        adsr.setParameters (p);
    }

    // ── Determine if we should produce sound ─────────────────────────────────
    const int midiNote = currentMidiNote.load();
    const bool isActive = sampleBuffer.hasContent()
                          && (midiNote >= 0 || adsr.isActive());

    if (!isActive)
    {
        output.clear();
        return;
    }

    // ── Read parameters from APVTS ───────────────────────────────────────────
    auto getF = [&] (const char* id) -> float
    {
        return apvts.getRawParameterValue (id)->load();
    };

    // ── LFO 1/2/3 — compute and sum modulation onto shared targets ───────────
    const float lfoRaw1 = calcLFO (getF ("lfoRate"),  (int) getF ("lfoShape"),  numSamples, lfoPhase,  lfoLastS_H);
    const float lfoRaw2 = calcLFO (getF ("lfoRate2"), (int) getF ("lfoShape2"), numSamples, lfoPhase2, lfoLastS_H2);
    const float lfoRaw3 = calcLFO (getF ("lfoRate3"), (int) getF ("lfoShape3"), numSamples, lfoPhase3, lfoLastS_H3);

    lfoPhaseAtomic.store   ((float) lfoPhase);   lfoOutputAtomic.store  (lfoRaw1);
    lfoPhase2Atomic.store  ((float) lfoPhase2);  lfoOutput2Atomic.store (lfoRaw2);
    lfoPhase3Atomic.store  ((float) lfoPhase3);  lfoOutput3Atomic.store (lfoRaw3);

    const float lfoVal1 = lfoRaw1 * getF ("lfoDepth");
    const float lfoVal2 = lfoRaw2 * getF ("lfoDepth2");
    const float lfoVal3 = lfoRaw3 * getF ("lfoDepth3");

    const int lfoTarget1 = (int) getF ("lfoTarget");
    const int lfoTarget2 = (int) getF ("lfoTarget2");
    const int lfoTarget3 = (int) getF ("lfoTarget3");

    // ── Grain parameters — apply LFO offsets to targets ──────────────────────
    float grainSizeMs  = getF ("grainSize");
    float density      = getF ("grainDensity");
    float position     = getF ("grainPosition");
    float pitchShift   = getF ("pitchShift");
    float panSpread    = getF ("panSpread");

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
    applyLFO (lfoTarget1, lfoVal1);
    applyLFO (lfoTarget2, lfoVal2);
    applyLFO (lfoTarget3, lfoVal3);

    // ── Env follower modulation ───────────────────────────────────────────────
    {
        const bool  envActive = getF ("envActive") > 0.5f;
        const int   envTarget = (int) getF ("envTarget");
        const float envDepth  = getF ("envDepth");
        if (envActive && envTarget > 0 && envDepth > 0.001f)
        {
            const float envVal = (envFollower.getValue() * 2.f - 1.f) * envDepth;
            switch (envTarget)
            {
                case 1: grainSizeMs = juce::jlimit (5.f,   500.f, grainSizeMs + envVal * 200.f); break;
                case 2: density     = juce::jlimit (1.f,   200.f, density     + envVal * 80.f);  break;
                case 3: position    = juce::jlimit (0.f,   1.f,   position    + envVal * 0.5f);  break;
                case 4: pitchShift  = juce::jlimit (-24.f, 24.f,  pitchShift  + envVal * 12.f);  break;
                case 5: panSpread   = juce::jlimit (0.f,   1.f,   panSpread   + envVal * 0.5f);  break;
                default: break;
            }
        }
    }

    // ── Step sequencer: overrides position when active ────────────────────────
    {
        const bool seqActive = getF ("seqActive") > 0.5f;
        if (seqActive)
        {
            float stepVals[StepSequencer::kNumSteps];
            for (int i = 0; i < StepSequencer::kNumSteps; ++i)
                stepVals[i] = apvts.getRawParameterValue ("seqStep" + juce::String (i))->load();
            const int divIdx = juce::jlimit (0, 8, (int) getF ("beatDivision"));
            const float seqPos = stepSequencer.process (stepVals, numSamples,
                                                         sampleRate, bpm, divIdx, true);
            if (seqPos >= 0.f)
                position = seqPos;
        }
    }

    // ── Publish final modulated position for waveform display ─────────────────
    modulatedPositionAtomic.store (position);

    // ── Beat sync ─────────────────────────────────────────────────────────────
    const bool beatSync = getF ("beatSync") > 0.5f;
    if (beatSync && bpm > 0.0)
    {
        const int divIdx = juce::jlimit (0, 8, (int) getF ("beatDivision"));
        const double mults[] = { 0.125, 1.0/6.0, 0.25, 1.0/3.0, 0.5, 2.0/3.0, 1.0, 2.0, 4.0 };
        const double divSec  = (60.0 / bpm) * mults[divIdx];
        density = (float) (1.0 / divSec);
    }

    const float posJitter    = getF ("posJitter");
    const float pitchJitter  = getF ("pitchJitter");
    const float outputGainDb = getF ("outputGain");

    const int windowIdx = (int) getF ("windowType");
    const WindowType windowType = static_cast<WindowType> (juce::jlimit (0, 3, windowIdx));

    const int scaleIdx  = (int) getF ("pitchScale");
    const int rootNote  = (int) getF ("pitchRoot");

    // ── Pitch ratio ───────────────────────────────────────────────────────────
    if (midiNote >= 0) lastActiveMidiNote = midiNote;

    const double pitchRatio = midiNoteToPitchRatio (lastActiveMidiNote)
                            * std::pow (2.0, (double) pitchShift / 12.0);

    // ── Scheduler ────────────────────────────────────────────────────────────
    const auto& source = sampleBuffer.getBuffer();

    scheduler.process (numSamples, sampleRate, grainPool,
                       source.getNumSamples(),
                       grainSizeMs, density, position, posJitter,
                       pitchRatio, pitchJitter, panSpread,
                       windowType, true,
                       scaleIdx, rootNote);

    // ── Process pool into wet buffer ──────────────────────────────────────────
    wetBuffer.setSize (2, numSamples, false, false, true);
    wetBuffer.clear();
    grainPool.process (wetBuffer, source);

    // ── Envelope follower update ──────────────────────────────────────────────
    {
        float rmsSum = 0.f;
        for (int ch = 0; ch < wetBuffer.getNumChannels(); ++ch)
            rmsSum += wetBuffer.getRMSLevel (ch, 0, numSamples);
        const float wetRms = rmsSum / (float) juce::jmax (1, wetBuffer.getNumChannels());

        const float attCoeff = EnvelopeFollower::makeCoeff (getF ("envAttack"),  sampleRate, numSamples);
        const float relCoeff = EnvelopeFollower::makeCoeff (getF ("envRelease"), sampleRate, numSamples);
        envFollower.process (wetRms, attCoeff, relCoeff);
        envFollowValueAtomic.store (envFollower.getValue());
    }

    // ── ADSR envelope ─────────────────────────────────────────────────────────
    adsr.applyEnvelopeToBuffer (wetBuffer, 0, numSamples);

    // ── Output gain ───────────────────────────────────────────────────────────
    wetBuffer.applyGain (juce::Decibels::decibelsToGain (outputGainDb));

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

    switch (shape)
    {
        case 0: return std::sin (p * 6.283185307f);
        case 1: return 1.f - 4.f * std::abs (p - 0.5f);
        case 2: return p * 2.f - 1.f;
        case 3: return p < 0.5f ? 1.f : -1.f;
        case 4:
        {
            if (p < (float) advance)
                lastSH = lfoRandom.nextFloat() * 2.f - 1.f;
            return lastSH;
        }
        default: return 0.f;
    }
}
