#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Engine/GladeConstants.h"

using namespace GladeColors;

//==============================================================================
GladeAudioProcessorEditor::GladeAudioProcessorEditor (GladeAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      waveformDisplay (p.apvts),

      attackKnob  ("attack",  "Attack",  purple, p.apvts),
      decayKnob   ("decay",   "Decay",   purple, p.apvts),
      sustainKnob ("sustain", "Sustain", purple, p.apvts),
      releaseKnob ("release", "Release", purple, p.apvts),

      grainSizeKnob     ("grainSize",    "Size",     cyan, p.apvts),
      grainDensityKnob  ("grainDensity", "Density",  cyan, p.apvts),
      grainPositionKnob ("grainPosition","Position", cyan, p.apvts),
      posJitterKnob     ("posJitter",    "Jitter",   cyan, p.apvts),
      reverseKnob       ("reverseAmount","Reverse",  cyan, p.apvts),

      pitchShiftKnob ("pitchShift",  "Shift",  green, p.apvts),
      pitchJitterKnob("pitchJitter", "Jitter", green, p.apvts),
      panSpreadKnob  ("panSpread",   "Pan",    green, p.apvts),
      glideKnob      ("glideTime",   "Glide",  green, p.apvts),

      outputGainKnob ("outputGain",    "Gain",    yellow, p.apvts),
      dryWetKnob     ("dryWet",        "Dry/Wet", yellow, p.apvts),
      velocityKnob   ("velocityDepth", "Velocity",yellow, p.apvts),

      windowCombo       ("windowType",  "Window", cyan,    p.apvts),
      m1Knob   ("m1", "M1", magenta, p.apvts),
      m2Knob   ("m2", "M2", magenta, p.apvts),
      m3Knob   ("m3", "M3", magenta, p.apvts),
      m4Knob   ("m4", "M4", magenta, p.apvts),
      m1TargetCombo ("m1Target", "T", magenta, p.apvts),
      m2TargetCombo ("m2Target", "T", magenta, p.apvts),
      m3TargetCombo ("m3Target", "T", magenta, p.apvts),
      m4TargetCombo ("m4Target", "T", magenta, p.apvts),
      lfo1RateKnob      ("lfoRate",     "Rate",   magenta, p.apvts),
      lfo1DepthKnob     ("lfoDepth",   "Depth",  magenta, p.apvts),
      lfo1ShapeCombo    ("lfoShape",   "Shape",  magenta, p.apvts),
      lfo1TargetCombo   ("lfoTarget",  "Target", magenta, p.apvts),
      lfo2RateKnob      ("lfoRate2",   "Rate",   green,   p.apvts),
      lfo2DepthKnob     ("lfoDepth2",  "Depth",  green,   p.apvts),
      lfo2ShapeCombo    ("lfoShape2",  "Shape",  green,   p.apvts),
      lfo2TargetCombo   ("lfoTarget2", "Target", green,   p.apvts),
      lfo3RateKnob      ("lfoRate3",   "Rate",   yellow,  p.apvts),
      lfo3DepthKnob     ("lfoDepth3",  "Depth",  yellow,  p.apvts),
      lfo3ShapeCombo    ("lfoShape3",  "Shape",  yellow,  p.apvts),
      lfo3TargetCombo   ("lfoTarget3", "Target", yellow,  p.apvts),
      beatDivCombo    ("beatDivision", "Div",    cyan,  p.apvts)
{
    setLookAndFeel (&laf);

    // ── Header ───────────────────────────────────────────────────────────────
    logoLabel.setText ("GLADE", juce::dontSendNotification);
    logoLabel.setFont (juce::Font (juce::FontOptions{}.withHeight (20.f).withStyle ("Bold")));
    logoLabel.setColour (juce::Label::textColourId, textPrimary);
    addAndMakeVisible (logoLabel);

    // ── Preset navigation ─────────────────────────────────────────────────────
    // Restore preset name from the processor (set by setStateInformation on reload)
    presetManager.setCurrentByName (audioProcessor.currentPresetName);

    presetMenuButton.setButtonText (presetManager.getCurrentName());
    presetMenuButton.setColour (juce::TextButton::buttonColourId,  panelRaised);
    presetMenuButton.setColour (juce::TextButton::textColourOffId, textPrimary);
    presetMenuButton.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible (presetMenuButton);

    for (auto* btn : { &prevPresetButton, &nextPresetButton })
    {
        btn->setColour (juce::TextButton::buttonColourId,  panelRaised);
        btn->setColour (juce::TextButton::textColourOffId, textPrimary);
        addAndMakeVisible (btn);
    }
    prevPresetButton.onClick = [this]
    {
        presetManager.prevPreset (audioProcessor.apvts);
        onPresetChosen (presetManager.getCurrentIndex());
    };
    nextPresetButton.onClick = [this]
    {
        presetManager.nextPreset (audioProcessor.apvts);
        onPresetChosen (presetManager.getCurrentIndex());
    };

    rndAllButton.setColour (juce::TextButton::buttonColourId,  panelRaised);
    rndAllButton.setColour (juce::TextButton::textColourOffId, magenta);
    rndAllButton.onClick = [this] { randomiseAll (wildButton.getToggleState()); };
    addAndMakeVisible (rndAllButton);

    // WILD — styled as a toggle button with clear on/off visual
    wildButton.setColour (juce::ToggleButton::tickColourId,         magenta);
    wildButton.setColour (juce::ToggleButton::tickDisabledColourId, textDim);
    wildButton.onStateChange = [this]
    {
        const bool on = wildButton.getToggleState();
        wildButton.setColour (juce::ToggleButton::tickColourId,
                              on ? magenta : textDim);
        repaint (headerArea);
    };
    addAndMakeVisible (wildButton);

    // ── Fractal visualizer ────────────────────────────────────────────────────
    addAndMakeVisible (fractalVisualizer);

    // ── Waveform callback ─────────────────────────────────────────────────────
    waveformDisplay.onFileLoad = [this] (const juce::File& f) -> bool
    {
        const bool ok = audioProcessor.loadSample (f);
        if (ok)
            waveformDisplay.setSampleBuffer (audioProcessor.getSampleBuffer());
        return ok;
    };

    // Populate waveform if a sample was already loaded (e.g. restored from state)
    if (audioProcessor.isSampleLoaded())
        waveformDisplay.setSampleBuffer (audioProcessor.getSampleBuffer());

    // ── Beat sync button ─────────────────────────────────────────────────────
    beatSyncButton.setColour (juce::ToggleButton::tickColourId,         cyan);
    beatSyncButton.setColour (juce::ToggleButton::tickDisabledColourId, textDim);
    beatSyncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, "beatSync", beatSyncButton);
    addAndMakeVisible (beatSyncButton);
    addAndMakeVisible (beatDivCombo);

    // ── Freeze button ─────────────────────────────────────────────────────────
    freezeButton.setColour (juce::ToggleButton::tickColourId,         cyan);
    freezeButton.setColour (juce::ToggleButton::tickDisabledColourId, textDim);
    freezeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, "freeze", freezeButton);
    addAndMakeVisible (freezeButton);

    // ── Step Sequencer ────────────────────────────────────────────────────────
    seqActiveButton.setColour (juce::ToggleButton::tickColourId,         cyan);
    seqActiveButton.setColour (juce::ToggleButton::tickDisabledColourId, textDim);
    seqActiveAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, "seqActive", seqActiveButton);
    addAndMakeVisible (seqActiveButton);

    stepSeqUI.onStepChanged = [this] (int idx, float val)
    {
        const juce::String paramId = "seqStep" + juce::String (idx);
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (
                audioProcessor.apvts.getParameter (paramId)))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (val));
            param->endChangeGesture();
        }
    };
    addAndMakeVisible (stepSeqUI);

    // ── RND buttons ───────────────────────────────────────────────────────────
    auto styleRnd = [&] (juce::TextButton& b, juce::Colour c)
    {
        b.setColour (juce::TextButton::buttonColourId,  panelRaised);
        b.setColour (juce::TextButton::textColourOffId, c);
    };
    styleRnd (grainRndButton, cyan);
    styleRnd (pitchRndButton, green);
    grainRndButton.onClick = [this] { randomiseGrain (wildButton.getToggleState()); };
    pitchRndButton.onClick = [this] { randomisePitch (wildButton.getToggleState()); };
    addAndMakeVisible (grainRndButton);
    addAndMakeVisible (pitchRndButton);

    // ── Add all knob/combo components ─────────────────────────────────────────
    addAndMakeVisible (waveformDisplay);
    addAndMakeVisible (adsrDisplay);
    addAndMakeVisible (attackKnob);
    addAndMakeVisible (decayKnob);
    addAndMakeVisible (sustainKnob);
    addAndMakeVisible (releaseKnob);
    addAndMakeVisible (grainSizeKnob);
    addAndMakeVisible (grainDensityKnob);
    addAndMakeVisible (grainPositionKnob);
    addAndMakeVisible (posJitterKnob);
    addAndMakeVisible (reverseKnob);
    addAndMakeVisible (pitchShiftKnob);
    addAndMakeVisible (pitchJitterKnob);
    addAndMakeVisible (panSpreadKnob);
    addAndMakeVisible (glideKnob);
    addAndMakeVisible (outputGainKnob);
    addAndMakeVisible (dryWetKnob);
    addAndMakeVisible (velocityKnob);
    addAndMakeVisible (windowCombo);
    addAndMakeVisible (m1Knob);  addAndMakeVisible (m2Knob);
    addAndMakeVisible (m3Knob);  addAndMakeVisible (m4Knob);
    addAndMakeVisible (m1TargetCombo); addAndMakeVisible (m2TargetCombo);
    addAndMakeVisible (m3TargetCombo); addAndMakeVisible (m4TargetCombo);
    addAndMakeVisible (lfoDisplay);
    addAndMakeVisible (lfo1RateKnob);  addAndMakeVisible (lfo1DepthKnob);
    addAndMakeVisible (lfo1ShapeCombo);addAndMakeVisible (lfo1TargetCombo);
    addAndMakeVisible (lfo2RateKnob);  addAndMakeVisible (lfo2DepthKnob);
    addAndMakeVisible (lfo2ShapeCombo);addAndMakeVisible (lfo2TargetCombo);
    addAndMakeVisible (lfo3RateKnob);  addAndMakeVisible (lfo3DepthKnob);
    addAndMakeVisible (lfo3ShapeCombo);addAndMakeVisible (lfo3TargetCombo);

    // LFO selector buttons
    for (int i = 0; i < 3; ++i)
    {
        lfoSelBtn[i].setButtonText (juce::String (i + 1));
        lfoSelBtn[i].setColour (juce::TextButton::buttonColourId,  panelRaised);
        lfoSelBtn[i].setColour (juce::TextButton::textColourOffId, textDim);
        lfoSelBtn[i].setColour (juce::TextButton::buttonOnColourId, magenta.withAlpha (0.25f));
        lfoSelBtn[i].setColour (juce::TextButton::textColourOnId,  magenta);
        lfoSelBtn[i].setClickingTogglesState (false);
        const int idx = i;
        lfoSelBtn[i].onClick = [this, idx]
        {
            activeLfo = idx;
            resized();
        };
        addAndMakeVisible (lfoSelBtn[i]);
    }



    // ── FX Slots (type combo is inside each FXSlotUI) ────────────────────────
    for (int i = 0; i < 4; ++i)
    {
        fxSlots[i] = std::make_unique<FXSlotUI> (i, p.apvts);
        fxSlots[i]->onSwap = [this] (int a, int b) { swapFxSlots (a, b); };
        addAndMakeVisible (*fxSlots[i]);
    }

    setSize (1400, 880);
    startTimerHz (30);
}

GladeAudioProcessorEditor::~GladeAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void GladeAudioProcessorEditor::timerCallback()
{
    // Free old effect instances that the audio thread retired during type swaps.
    // Must be called from the message thread — this timer fires at 30 Hz, which
    // is more than fast enough given that type changes are user-driven.
    audioProcessor.fxChain->collectGarbage();

    // Sync preset button text after async state restore (setStateInformation defers
    // currentPresetName update to the message thread via callAsync, so the editor
    // constructor may have read a stale "INIT" name).
    {
        const juce::String procName = audioProcessor.currentPresetName;
        if (presetMenuButton.getButtonText() != procName)
        {
            presetManager.setCurrentByName (procName);
            presetMenuButton.setButtonText (procName);
        }
    }

    const auto state = audioProcessor.getState();

    auto getF = [&] (const char* id) -> float
    {
        return audioProcessor.apvts.getRawParameterValue (id)->load();
    };

    const float audioFreqHz = (state.currentMidiNote >= 0)
        ? 440.f * std::pow (2.f, (float) (state.currentMidiNote - 69) / 12.f)
        : 0.f;

    // ADSR display — update shape and animated cursor
    adsrDisplay.setParams (getF ("attack"), getF ("decay"),
                           getF ("sustain"), getF ("release"),
                           state.adsrCursor);
    adsrDisplay.repaint();


    fractalVisualizer.update (getF ("grainDensity"),
                              getF ("grainSize"),
                              state.currentMidiNote >= 0,
                              state.outputRms,
                              audioFreqHz);

    // ── LFO visual feedback (active LFO only) ────────────────────────────────
    static const char* lfoDepthIds[] = { "lfoDepth",  "lfoDepth2",  "lfoDepth3"  };
    static const char* lfoShapeIds[] = { "lfoShape",  "lfoShape2",  "lfoShape3"  };
    static const juce::Colour lfoColours[3] = {
        GladeColors::magenta, GladeColors::green, GladeColors::yellow };

    const float lfoDepth = getF (lfoDepthIds[activeLfo]);
    const int   lfoShape = (int) getF (lfoShapeIds[activeLfo]);

    lfoDisplay.setLfoState (lfoShape, state.lfoPhase[activeLfo], lfoDepth,
                            lfoColours[activeLfo]);
    lfoDisplay.repaint();

    // Collect all 3 LFO contributions for knob ring overlays
    const float lfoDep1 = getF ("lfoDepth");
    const float lfoDep2 = getF ("lfoDepth2");
    const float lfoDep3 = getF ("lfoDepth3");
    const int   lfoTgt1 = (int) getF ("lfoTarget");
    const int   lfoTgt2 = (int) getF ("lfoTarget2");
    const int   lfoTgt3 = (int) getF ("lfoTarget3");

    // Map target index → knob pointer (for LFO ring overlays)
    GladeKnob* modKnobs[] = {
        nullptr,            // 0 = None
        &grainSizeKnob,     // 1 = GrainSize
        &grainDensityKnob,  // 2 = Density
        &grainPositionKnob, // 3 = Position
        &pitchShiftKnob,    // 4 = Pitch
        &panSpreadKnob      // 5 = Pan
    };

    // Per-LFO coloured rings: each LFO drives its own ring at its own colour.
    // Rings are only shown on the knob that the LFO is targeting.
    static const juce::Colour kLfoRingColours[3] = {
        GladeColors::magenta, GladeColors::green, GladeColors::yellow };
    const int   lfoTgts[3]  = { lfoTgt1, lfoTgt2, lfoTgt3 };
    const float lfoDeps[3]  = { lfoDep1, lfoDep2, lfoDep3 };

    for (int i = 0; i < 6; ++i)
    {
        if (modKnobs[i] == nullptr) continue;

        for (int li = 0; li < 3; ++li)
        {
            const bool targeting = (i == lfoTgts[li]) && (lfoDeps[li] > 0.001f);
            modKnobs[i]->setLfoOverlay (li,
                targeting ? state.lfoOutput[li] : 0.f,
                targeting ? lfoDeps[li]         : 0.f,
                kLfoRingColours[li]);
        }

        modKnobs[i]->setEnvOverlay (0.f, 0.f);
        modKnobs[i]->repaint();
    }

    // ── Step sequencer UI sync ────────────────────────────────────────────────
    {
        static const char* stepIds[] = {
            "seqStep0","seqStep1","seqStep2","seqStep3",
            "seqStep4","seqStep5","seqStep6","seqStep7"
        };
        static_assert (std::size (stepIds) == Glade::kSeqNumSteps,
                       "stepIds length must match kSeqNumSteps");
        float stepVals[Glade::kSeqNumSteps];
        for (int i = 0; i < Glade::kSeqNumSteps; ++i)
            stepVals[i] = audioProcessor.apvts.getRawParameterValue (stepIds[i])->load();
        stepSeqUI.setStepValues (stepVals);
        stepSeqUI.setCurrentStep (getF ("seqActive") > 0.5f ? state.seqCurrentStep : -1);
    }

    // Update root note label + LFO-modulated position on waveform display.
    // Also handles the case where setStateInformation is called after the editor
    // is opened (e.g. some DAWs): detect the first moment the sample becomes
    // available and push the buffer to the display.
    if (state.sampleLoaded)
    {
        if (!sampleWasLoaded)
            waveformDisplay.setSampleBuffer (audioProcessor.getSampleBuffer());

        waveformDisplay.setRootNote (audioProcessor.getRootNoteName());
        waveformDisplay.setModulatedPosition (state.modulatedPosition);
    }
    sampleWasLoaded = state.sampleLoaded;

    cachedGrainCount = state.activeGrainCount;
    repaint (headerArea); // refresh grain count
}

//==============================================================================
// Randomise helpers
void GladeAudioProcessorEditor::setParamRandom (juce::AudioProcessorValueTreeState& apvts,
                                                  const juce::String& id,
                                                  float minV, float maxV,
                                                  juce::Random& rng)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
    {
        const float val        = minV + rng.nextFloat() * (maxV - minV);
        const float normalized = p->getNormalisableRange().convertTo0to1 (val);
        p->beginChangeGesture();
        p->setValueNotifyingHost (normalized);
        p->endChangeGesture();
    }
}

void GladeAudioProcessorEditor::randomiseGrain (bool wild)
{
    fractalVisualizer.triggerGlitch();
    auto& a = audioProcessor.apvts;
    if (wild)
    {
        setParamRandom (a, "grainSize",    5.f,   500.f, rng);
        setParamRandom (a, "grainDensity", 1.f,   200.f, rng);
        setParamRandom (a, "posJitter",    0.f,   1.f,   rng);
    }
    else
    {
        setParamRandom (a, "grainSize",    20.f,  200.f, rng);
        setParamRandom (a, "grainDensity", 5.f,   100.f, rng);
        setParamRandom (a, "posJitter",    0.f,   0.5f,  rng);
    }
}

void GladeAudioProcessorEditor::randomisePitch (bool wild)
{
    fractalVisualizer.triggerGlitch();
    auto& a = audioProcessor.apvts;
    if (wild)
    {
        setParamRandom (a, "pitchShift",  -24.f, 24.f, rng);
        setParamRandom (a, "pitchJitter", 0.f,   1.f,  rng);
        setParamRandom (a, "panSpread",   0.f,   1.f,  rng);
    }
    else
    {
        setParamRandom (a, "pitchShift",  -12.f, 12.f, rng);
        setParamRandom (a, "pitchJitter", 0.f,   0.4f, rng);
        setParamRandom (a, "panSpread",   0.f,   0.8f, rng);
    }
}

void GladeAudioProcessorEditor::randomiseAll (bool wild)
{
    randomiseGrain (wild);
    randomisePitch (wild);

    auto& a = audioProcessor.apvts;
    // Envelope
    setParamRandom (a, "attack",  0.001f, wild ? 10.f : 2.f,  rng);
    setParamRandom (a, "decay",   0.001f, wild ? 10.f : 2.f,  rng);
    setParamRandom (a, "sustain", 0.2f,   1.f,                 rng);
    setParamRandom (a, "release", 0.05f,  wild ? 10.f : 3.f,  rng);
    // LFO depths (keep targets, randomise depths for all 3)
    setParamRandom (a, "lfoDepth",  0.f, wild ? 1.f : 0.5f, rng);
    setParamRandom (a, "lfoDepth2", 0.f, wild ? 1.f : 0.4f, rng);
    setParamRandom (a, "lfoDepth3", 0.f, wild ? 1.f : 0.4f, rng);
}

//==============================================================================
void GladeAudioProcessorEditor::resized()
{
    // Guard: ButtonAttachment fires setToggleState() in its constructor (to sync
    // with the current parameter value), which triggers onStateChange → resized().
    // This can happen before fxSlots are created, causing a null-ptr crash.
    // Return early until construction is complete — setSize() at the end of the
    // constructor will call resized() again once everything is initialised.
    if (fxSlots[0] == nullptr) return;

    auto bounds = getLocalBounds();

    // ── Header (61px) ────────────────────────────────────────────────────────
    headerArea = bounds.removeFromTop (61);
    {
        auto h = headerArea.reduced (19, 11);
        logoLabel.setBounds        (h.removeFromLeft (85));
        wildButton.setBounds       (h.removeFromRight (75));
        rndAllButton.setBounds     (h.removeFromRight (101));
        h.removeFromRight (11);
        nextPresetButton.setBounds (h.removeFromRight (37));
        prevPresetButton.setBounds (h.removeFromRight (37));
        h.removeFromRight (5);
        presetMenuButton.setBounds (h);
    }

    // ── FX chain (233px) ─────────────────────────────────────────────────────
    bottomArea = bounds.removeFromBottom (233);

    // ── WINDOW + LFO + MACRO row (128px) ─────────────────────────────────────
    auto windowLfoRow = bounds.removeFromBottom (128);
    windowArea = windowLfoRow.removeFromLeft (333);
    macroArea  = windowLfoRow.removeFromRight (660);
    lfoArea    = windowLfoRow;

    // ── Middle knob row (253px) ───────────────────────────────────────────────
    midArea    = bounds.removeFromBottom (253);
    grainArea  = midArea.removeFromLeft (573);
    pitchArea  = midArea.removeFromLeft (453);
    outputArea = midArea;

    // ── Top display row (remaining) ───────────────────────────────────────────
    topArea    = bounds;
    waveArea   = topArea.removeFromLeft (440);
    fractalArea= topArea.removeFromLeft (360);
    envArea    = topArea;

    // ── Top row components ────────────────────────────────────────────────────
    {
        auto wa = waveArea.reduced (11);
        wa.removeFromTop (22);  // section title
        waveformDisplay.setBounds (wa);
    }
    fractalVisualizer.setBounds (fractalArea.reduced (11).withTrimmedTop (22));

    {
        auto ea = envArea.reduced (14, 0);
        ea.removeFromTop (22);                   // section title
        adsrDisplay.setBounds (ea.removeFromTop (48).reduced (2, 4));
        ea.removeFromTop (2);                    // small gap
        layoutKnobRow (ea.reduced (0, 6), { &attackKnob, &decayKnob, &sustainKnob, &releaseKnob });
    }

    // ── GRAIN section ─────────────────────────────────────────────────────────
    {
        auto ga = grainArea.reduced (11, 11);
        grainRndButton.setBounds (ga.getRight() - 48, ga.getY() + 3, 44, 21);

        auto syncRow = ga.removeFromBottom (53);
        syncRow.removeFromTop (5);
        freezeButton.setBounds  (syncRow.removeFromLeft (65).removeFromTop (35));
        syncRow.removeFromLeft (6);
        beatSyncButton.setBounds (syncRow.removeFromLeft (60).removeFromTop (35));
        syncRow.removeFromLeft (6);
        beatDivCombo.setBounds   (syncRow.removeFromLeft (175));
        syncRow.removeFromLeft (6);
        seqActiveButton.setBounds (syncRow.removeFromLeft (50).reduced (0, 4));
        syncRow.removeFromLeft (4);
        stepSeqUI.setBounds (syncRow.reduced (0, 2));

        layoutKnobRow (ga.withTrimmedTop (32),
                       { &grainSizeKnob, &grainDensityKnob,
                         &grainPositionKnob, &posJitterKnob, &reverseKnob });
    }

    // ── PITCH section ────────────────────────────────────────────────────────
    {
        auto pa = pitchArea.reduced (11, 11);
        pitchRndButton.setBounds (pa.getRight() - 53, pa.getY() + 3, 48, 21);
        layoutKnobRow (pa.withTrimmedTop (32),
                       { &pitchShiftKnob, &pitchJitterKnob, &panSpreadKnob, &glideKnob });
    }

    // ── OUTPUT section ────────────────────────────────────────────────────────
    layoutKnobRow (outputArea.reduced (11, 43),
                   { &outputGainKnob, &dryWetKnob, &velocityKnob });

    // ── WINDOW section ────────────────────────────────────────────────────────
    {
        auto wa = windowArea.reduced (16, 24);
        windowCombo.setBounds (wa.removeFromTop (69));
    }

    // ── MACRO section ─────────────────────────────────────────────────────────
    {
        auto ma = macroArea.reduced (10, 0);
        ma.removeFromTop (22);  // section title

        // 4 columns, each with a knob (top) and a target combo (bottom 34px)
        const int   cols   = 4;
        const int   colW   = ma.getWidth() / cols;
        const int   comboH = 34;
        const int   knobH  = ma.getHeight() - comboH - 4;

        GladeKnob*  macroKnobs[]  = { &m1Knob,        &m2Knob,        &m3Knob,        &m4Knob        };
        GladeCombo* macroCombos[] = { &m1TargetCombo, &m2TargetCombo, &m3TargetCombo, &m4TargetCombo };

        for (int i = 0; i < cols; ++i)
        {
            auto col = ma.removeFromLeft (colW).reduced (4, 0);
            macroKnobs[i]->setBounds  (col.removeFromTop (knobH));
            col.removeFromTop (4);
            macroCombos[i]->setBounds (col.removeFromTop (comboH));
        }
    }

    // ── LFO section ───────────────────────────────────────────────────────────
    {
        auto la = lfoArea.reduced (13, 0);
        la.removeFromTop (22);   // section title space

        // LFO selector buttons [1][2][3] stacked vertically, then display, then knobs
        static const juce::Colour lfoColours[3] = { magenta, green, yellow };

        auto selCol = la.removeFromLeft (22);
        const int btnH = selCol.getHeight() / 3;
        for (int i = 0; i < 3; ++i)
        {
            lfoSelBtn[i].setBounds (selCol.removeFromTop (btnH).reduced (0, 2));
            // Each LFO button uses its own colour
            const bool active = (i == activeLfo);
            lfoSelBtn[i].setColour (juce::TextButton::buttonColourId,
                active ? lfoColours[i].withAlpha (0.22f) : panelRaised);
            lfoSelBtn[i].setColour (juce::TextButton::textColourOffId,
                active ? lfoColours[i] : textDim);
        }
        la.removeFromLeft (4);

        // LFO display
        lfoDisplay.setBounds (la.removeFromLeft (70).reduced (0, 4));
        la.removeFromLeft (6);

        // Active LFO controls
        GladeKnob*  rateK   = nullptr;  GladeKnob*  depthK  = nullptr;
        GladeCombo* shapeC  = nullptr;  GladeCombo* targetC = nullptr;

        if (activeLfo == 0) { rateK=&lfo1RateKnob; depthK=&lfo1DepthKnob; shapeC=&lfo1ShapeCombo; targetC=&lfo1TargetCombo; }
        if (activeLfo == 1) { rateK=&lfo2RateKnob; depthK=&lfo2DepthKnob; shapeC=&lfo2ShapeCombo; targetC=&lfo2TargetCombo; }
        if (activeLfo == 2) { rateK=&lfo3RateKnob; depthK=&lfo3DepthKnob; shapeC=&lfo3ShapeCombo; targetC=&lfo3TargetCombo; }

        // Hide all controls for inactive LFOs, then show the active set
        for (auto* c : { &lfo1RateKnob, &lfo1DepthKnob, &lfo2RateKnob, &lfo2DepthKnob,
                          &lfo3RateKnob, &lfo3DepthKnob }) c->setVisible (false);
        for (auto* c : { &lfo1ShapeCombo, &lfo1TargetCombo, &lfo2ShapeCombo, &lfo2TargetCombo,
                          &lfo3ShapeCombo, &lfo3TargetCombo }) c->setVisible (false);

        // Knob/combo layout — compact combo column so rate/depth knobs get more room
        auto comboCol  = la.removeFromRight (130);
        auto comboLeft = comboCol.removeFromLeft (comboCol.getWidth() / 2 - 2);
        if (targetC) { targetC->setBounds (comboLeft.removeFromTop (69)); targetC->setVisible (true); }
        if (shapeC)  { shapeC->setBounds  (comboCol.reduced (2, 0).removeFromTop (69)); shapeC->setVisible (true); }
        if (rateK && depthK)
        {
            layoutKnobRow (la, { rateK, depthK });
            rateK->setVisible (true);  depthK->setVisible (true);
        }

    }

    // ── FX chain strip ────────────────────────────────────────────────────────
    {
        auto strip = bottomArea.reduced (11, 8);
        strip.removeFromTop (27);
        const int slotW = strip.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
            fxSlots[i]->setBounds (strip.removeFromLeft (slotW).reduced (5, 3));
    }
}

void GladeAudioProcessorEditor::layoutKnobRow (juce::Rectangle<int> row,
                                                std::initializer_list<GladeKnob*> knobs)
{
    const int n = (int) knobs.size();
    if (n == 0) return;
    const int w = row.getWidth() / n;
    int x = row.getX();
    for (auto* k : knobs)
    {
        k->setBounds (x, row.getY(), w, row.getHeight());
        x += w;
    }
}

//==============================================================================
void GladeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);

    // Header bar
    g.setColour (panel);
    g.fillRect (headerArea);
    g.setColour (border);
    g.fillRect (headerArea.getX(), headerArea.getBottom() - 1, headerArea.getWidth(), 1);

    // Section panels
    paintSection (g, waveArea,    "SAMPLE",      cyan);
    paintSection (g, fractalArea, "GRAIN CLOUD", cyan);
    paintSection (g, envArea,     "ENVELOPE",    purple);
    paintSection (g, grainArea,   "GRAIN",       cyan);
    paintSection (g, pitchArea,   "PITCH",       green);
    paintSection (g, outputArea,  "OUTPUT",      yellow);
    paintSection (g, windowArea,  "WINDOW",  cyan);
    paintSection (g, macroArea,   "MACRO",   magenta);
    paintSection (g, lfoArea,     "LFO", magenta);

    // FX chain area
    g.setColour (panel);
    g.fillRect (bottomArea);
    g.setColour (border);
    g.fillRect (bottomArea.getX(), bottomArea.getY(), bottomArea.getWidth(), 1);
    g.setColour (textDim);
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.f).withStyle ("Bold")));
    g.drawText ("FX CHAIN", bottomArea.reduced (14, 6).removeFromTop (22),
                juce::Justification::centredLeft);

    // Grain count — shown in the GRAIN CLOUD section title bar (not the header, where it
    // would overlap the WILD button and RND ALL button on the right side).
    g.setColour (textDim);
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.f)));
    g.drawText (juce::String (cachedGrainCount) + " grains",
                fractalArea.reduced (12, 0).removeFromTop (26),
                juce::Justification::centredRight);

    // WILD active indicator — magenta glow behind the word
    if (wildButton.getToggleState())
    {
        const auto wb = wildButton.getBoundsInParent().toFloat().expanded (3.f);
        g.setColour (magenta.withAlpha (0.12f));
        g.fillRoundedRectangle (wb, 4.f);
        g.setColour (magenta.withAlpha (0.5f));
        g.drawRoundedRectangle (wb.reduced (0.5f), 4.f, 1.f);
    }
}

void GladeAudioProcessorEditor::paintSection (juce::Graphics& g,
                                               juce::Rectangle<int> bounds,
                                               const juce::String& title,
                                               juce::Colour accent)
{
    g.setColour (panel);
    g.fillRoundedRectangle (bounds.toFloat().reduced (4.f), 5.f);

    g.setColour (border);
    g.drawRoundedRectangle (bounds.toFloat().reduced (4.5f), 5.f, 1.f);

    g.setColour (accent);
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.f).withStyle ("Bold")));
    g.drawText (title, bounds.reduced (12, 0).removeFromTop (26),
                juce::Justification::centredLeft);

    g.setColour (accent.withAlpha (0.4f));
    g.fillRect (juce::Rectangle<float> ((float) bounds.getX() + 12.f,
                                         (float) bounds.getY() + 19.f,
                                         28.f, 1.5f));
}

//==============================================================================
void GladeAudioProcessorEditor::onPresetChosen (int /*index*/)
{
    const juce::String name = presetManager.getCurrentName();
    presetMenuButton.setButtonText (name);
    audioProcessor.currentPresetName = name;
    // Notify the DAW so it can refresh its preset/state display
    audioProcessor.updateHostDisplay (
        juce::AudioProcessorListener::ChangeDetails{}.withNonParameterStateChanged (true));
}

void GladeAudioProcessorEditor::showPresetMenu()
{
    const auto& presets = presetManager.getPresets();

    // Collect unique category order (preserving first-seen order)
    juce::StringArray categories;
    for (const auto& p : presets)
        if (!categories.contains (p.category))
            categories.add (p.category);

    juce::PopupMenu menu;
    int itemId = 1;

    for (const auto& cat : categories)
    {
        juce::PopupMenu sub;
        for (int i = 0; i < (int) presets.size(); ++i)
        {
            if (presets[i].category == cat)
            {
                const bool isCurrent = (i == presetManager.getCurrentIndex());
                sub.addItem (itemId + i, presets[i].name, true, isCurrent);
            }
        }
        menu.addSubMenu (cat, sub);
    }

    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetComponent (presetMenuButton)
                            .withMinimumWidth (presetMenuButton.getWidth()),
        [this] (int result)
        {
            if (result <= 0) return;
            const int index = result - 1;   // itemId = index + 1
            presetManager.loadPreset (index, audioProcessor.apvts);
            onPresetChosen (index);
        });
}

//==============================================================================
void GladeAudioProcessorEditor::swapFxSlots (int a, int b)
{
    if (a == b || a < 0 || b < 0 || a >= 4 || b >= 4) return;

    auto swapParam = [&] (const char* prefix)
    {
        const juce::String idA = juce::String (prefix) + juce::String (a);
        const juce::String idB = juce::String (prefix) + juce::String (b);
        auto* pa = audioProcessor.apvts.getParameter (idA);
        auto* pb = audioProcessor.apvts.getParameter (idB);
        if (!pa || !pb) return;
        const float va = pa->getValue();
        const float vb = pb->getValue();
        pa->beginChangeGesture(); pa->setValueNotifyingHost (vb); pa->endChangeGesture();
        pb->beginChangeGesture(); pb->setValueNotifyingHost (va); pb->endChangeGesture();
    };

    for (const char* name : { "fxType", "fxBypass", "fxP1", "fxP2", "fxP3", "fxMix" })
        swapParam (name);
}

//==============================================================================
juce::AudioProcessorEditor* GladeAudioProcessor::createEditor()
{
    return new GladeAudioProcessorEditor (*this);
}
