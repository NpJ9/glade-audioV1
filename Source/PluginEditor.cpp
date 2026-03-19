#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Engine/PitchDetector.h"
#include "Engine/StepSequencer.h"

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

      pitchShiftKnob ("pitchShift",  "Shift",  green, p.apvts),
      pitchJitterKnob("pitchJitter", "Jitter", green, p.apvts),
      panSpreadKnob  ("panSpread",   "Pan",    green, p.apvts),

      outputGainKnob ("outputGain", "Gain",    yellow, p.apvts),
      dryWetKnob     ("dryWet",     "Dry/Wet", yellow, p.apvts),

      windowCombo     ("windowType", "Window", cyan,    p.apvts),
      lfoRateKnob     ("lfoRate",    "Rate",   magenta, p.apvts),
      lfoDepthKnob    ("lfoDepth",   "Depth",  magenta, p.apvts),
      lfoShapeCombo   ("lfoShape",   "Shape",  magenta, p.apvts),
      lfoTargetCombo  ("lfoTarget",  "Target", magenta, p.apvts),

      beatDivCombo    ("beatDivision", "Div",    cyan,  p.apvts),

      envAttackKnob  ("envAttack",  "Atk",    cyan, p.apvts),
      envReleaseKnob ("envRelease", "Rel",    cyan, p.apvts),
      envDepthKnob   ("envDepth",   "Depth",  cyan, p.apvts),
      envTargetCombo ("envTarget",  "Target", cyan, p.apvts)
{
    setLookAndFeel (&laf);

    // ── Header ───────────────────────────────────────────────────────────────
    logoLabel.setText ("GLADE", juce::dontSendNotification);
    logoLabel.setFont (juce::Font (juce::FontOptions{}.withHeight (20.f).withStyle ("Bold")));
    logoLabel.setColour (juce::Label::textColourId, textPrimary);
    addAndMakeVisible (logoLabel);

    // Preset nav
    presetNameLabel.setText (audioProcessor.presetManager.getCurrentName(),
                             juce::dontSendNotification);
    presetNameLabel.setFont (juce::Font (juce::FontOptions{}.withHeight (16.f).withStyle ("Bold")));
    presetNameLabel.setColour (juce::Label::textColourId, textPrimary);
    presetNameLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (presetNameLabel);

    for (auto* btn : { &prevPresetButton, &nextPresetButton })
    {
        btn->setColour (juce::TextButton::buttonColourId,  panelRaised);
        btn->setColour (juce::TextButton::textColourOffId, textPrimary);
        addAndMakeVisible (btn);
    }
    prevPresetButton.onClick = [this]
    {
        audioProcessor.presetManager.prevPreset (audioProcessor.apvts);
        presetNameLabel.setText (audioProcessor.presetManager.getCurrentName(),
                                 juce::dontSendNotification);
    };
    nextPresetButton.onClick = [this]
    {
        audioProcessor.presetManager.nextPreset (audioProcessor.apvts);
        presetNameLabel.setText (audioProcessor.presetManager.getCurrentName(),
                                 juce::dontSendNotification);
    };

    rndAllButton.setColour (juce::TextButton::buttonColourId,  panelRaised);
    rndAllButton.setColour (juce::TextButton::textColourOffId, magenta);
    rndAllButton.onClick = [this] { randomiseAll (wildButton.getToggleState()); };
    addAndMakeVisible (rndAllButton);

    wildButton.setColour (juce::ToggleButton::tickColourId,         magenta);
    wildButton.setColour (juce::ToggleButton::tickDisabledColourId, textDim);
    addAndMakeVisible (wildButton);

    // ── Fractal visualizer ────────────────────────────────────────────────────
    addAndMakeVisible (fractalVisualizer);

    // ── Waveform callback ─────────────────────────────────────────────────────
    waveformDisplay.onFileLoad = [this] (const juce::File& f) -> bool
    {
        const bool ok = audioProcessor.loadSample (f);
        if (ok)
            waveformDisplay.setSampleBuffer (audioProcessor.granularEngine.getSampleBuffer());
        return ok;
    };

    // Populate waveform if a sample was already loaded (e.g. restored from state)
    if (audioProcessor.granularEngine.isReady())
        waveformDisplay.setSampleBuffer (audioProcessor.granularEngine.getSampleBuffer());

    // ── Beat sync button ─────────────────────────────────────────────────────
    beatSyncButton.setColour (juce::ToggleButton::tickColourId,         cyan);
    beatSyncButton.setColour (juce::ToggleButton::tickDisabledColourId, textDim);
    {
        auto* att = new juce::AudioProcessorValueTreeState::ButtonAttachment (
            p.apvts, "beatSync", beatSyncButton);
        beatSyncAtt.reset (att);
    }
    addAndMakeVisible (beatSyncButton);
    addAndMakeVisible (beatDivCombo);

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
    addAndMakeVisible (attackKnob);
    addAndMakeVisible (decayKnob);
    addAndMakeVisible (sustainKnob);
    addAndMakeVisible (releaseKnob);
    addAndMakeVisible (grainSizeKnob);
    addAndMakeVisible (grainDensityKnob);
    addAndMakeVisible (grainPositionKnob);
    addAndMakeVisible (posJitterKnob);
    addAndMakeVisible (pitchShiftKnob);
    addAndMakeVisible (pitchJitterKnob);
    addAndMakeVisible (panSpreadKnob);
    addAndMakeVisible (outputGainKnob);
    addAndMakeVisible (dryWetKnob);
    addAndMakeVisible (windowCombo);
    addAndMakeVisible (lfoRateKnob);
    addAndMakeVisible (lfoDepthKnob);
    addAndMakeVisible (lfoShapeCombo);
    addAndMakeVisible (lfoTargetCombo);
    addAndMakeVisible (lfoDisplay);

    // ── ENV FOLLOW ────────────────────────────────────────────────────────────
    envActiveButton.setColour (juce::ToggleButton::tickColourId,         cyan);
    envActiveButton.setColour (juce::ToggleButton::tickDisabledColourId, textDim);
    envActiveAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, "envActive", envActiveButton);
    addAndMakeVisible (envActiveButton);
    addAndMakeVisible (envAttackKnob);
    addAndMakeVisible (envReleaseKnob);
    addAndMakeVisible (envDepthKnob);
    addAndMakeVisible (envTargetCombo);


    // ── FX Slots (type combo is inside each FXSlotUI) ────────────────────────
    for (int i = 0; i < 4; ++i)
    {
        fxSlots[i] = std::make_unique<FXSlotUI> (i, p.apvts);
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
    auto getF = [&] (const char* id) -> float
    {
        return audioProcessor.apvts.getRawParameterValue (id)->load();
    };

    const int   midiNote    = audioProcessor.granularEngine.getCurrentMidiNote();
    const float rmsLevel    = audioProcessor.outputRms.load();
    const float audioFreqHz = (midiNote >= 0)
        ? 440.f * std::pow (2.f, (float) (midiNote - 69) / 12.f)
        : 0.f;

    fractalVisualizer.update (getF ("grainDensity"),
                              getF ("grainSize"),
                              midiNote >= 0,
                              rmsLevel,
                              audioFreqHz);

    // ── LFO visual feedback ───────────────────────────────────────────────────
    const float lfoDepth  = getF ("lfoDepth");
    const int   lfoTarget = (int) getF ("lfoTarget");
    const int   lfoShape  = (int) getF ("lfoShape");
    const float lfoPhase  = audioProcessor.granularEngine.getLfoPhase();
    const float lfoOutput = audioProcessor.granularEngine.getLfoOutput();

    // Update LFO waveform display
    lfoDisplay.setLfoState (lfoShape, lfoPhase, lfoDepth);
    lfoDisplay.repaint();

    // Env follower state
    const bool  envActive    = getF ("envActive") > 0.5f;
    const int   envTarget    = (int) getF ("envTarget");
    const float envDepth     = getF ("envDepth");
    const float envValue     = audioProcessor.granularEngine.getEnvFollowValue();

    // Map target index → knob pointer (shared by LFO and env)
    GladeKnob* modKnobs[] = {
        nullptr,            // 0 = None
        &grainSizeKnob,     // 1 = GrainSize
        &grainDensityKnob,  // 2 = Density
        &grainPositionKnob, // 3 = Position
        &pitchShiftKnob,    // 4 = Pitch
        &panSpreadKnob      // 5 = Pan
    };

    for (int i = 0; i < 6; ++i)
    {
        if (modKnobs[i] == nullptr) continue;

        const bool lfoActive = (i == lfoTarget && lfoDepth > 0.001f);
        const bool envOn     = (i == envTarget  && envActive && envDepth > 0.001f);

        modKnobs[i]->setLfoOverlay (lfoActive ? lfoOutput : 0.f, lfoActive ? lfoDepth : 0.f);
        modKnobs[i]->setEnvOverlay (envOn     ? envValue  : 0.f, envOn     ? envDepth : 0.f);
        modKnobs[i]->repaint();
    }

    // ── Step sequencer UI sync ────────────────────────────────────────────────
    {
        float stepVals[StepSequencer::kNumSteps];
        for (int i = 0; i < StepSequencer::kNumSteps; ++i)
            stepVals[i] = audioProcessor.apvts.getRawParameterValue (
                "seqStep" + juce::String (i))->load();
        stepSeqUI.setStepValues (stepVals);
        stepSeqUI.setCurrentStep (
            getF ("seqActive") > 0.5f
            ? audioProcessor.granularEngine.getSeqCurrentStep()
            : -1);
    }

    // Update root note label on waveform display
    if (audioProcessor.granularEngine.isReady())
        waveformDisplay.setRootNote (
            PitchDetector::midiNoteToName (audioProcessor.granularEngine.getDetectedRootNote()));

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
    // LFO depth (keep target, randomise depth)
    setParamRandom (a, "lfoDepth", 0.f,   wild ? 1.f : 0.5f,  rng);
}

//==============================================================================
void GladeAudioProcessorEditor::resized()
{
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
        presetNameLabel.setBounds  (h);
    }

    // ── FX chain (233px) ─────────────────────────────────────────────────────
    bottomArea = bounds.removeFromBottom (233);

    // ── WINDOW + LFO row (128px) ──────────────────────────────────────────────
    auto windowLfoRow = bounds.removeFromBottom (128);
    windowArea = windowLfoRow.removeFromLeft (333);
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

    layoutKnobRow (envArea.reduced (16, 37),
                   { &attackKnob, &decayKnob, &sustainKnob, &releaseKnob });

    // ── GRAIN section ─────────────────────────────────────────────────────────
    {
        auto ga = grainArea.reduced (11, 11);
        grainRndButton.setBounds (ga.getRight() - 48, ga.getY() + 3, 44, 21);

        auto syncRow = ga.removeFromBottom (53);
        syncRow.removeFromTop (5);
        beatSyncButton.setBounds (syncRow.removeFromLeft (69).removeFromTop (35));
        syncRow.removeFromLeft (8);
        beatDivCombo.setBounds   (syncRow.removeFromLeft (200));
        syncRow.removeFromLeft (8);
        seqActiveButton.setBounds (syncRow.removeFromLeft (50).reduced (0, 4));
        syncRow.removeFromLeft (4);
        stepSeqUI.setBounds (syncRow.reduced (0, 2));

        layoutKnobRow (ga.withTrimmedTop (32),
                       { &grainSizeKnob, &grainDensityKnob,
                         &grainPositionKnob, &posJitterKnob });
    }

    // ── PITCH section (knobs only — scale/root combos removed) ───────────────
    {
        auto pa = pitchArea.reduced (11, 11);
        pitchRndButton.setBounds (pa.getRight() - 53, pa.getY() + 3, 48, 21);
        layoutKnobRow (pa.withTrimmedTop (32),
                       { &pitchShiftKnob, &pitchJitterKnob, &panSpreadKnob });
    }

    // ── OUTPUT section ────────────────────────────────────────────────────────
    layoutKnobRow (outputArea.reduced (11, 43),
                   { &outputGainKnob, &dryWetKnob });

    // ── WINDOW section ────────────────────────────────────────────────────────
    {
        auto wa = windowArea.reduced (16, 24);
        windowCombo.setBounds (wa.removeFromTop (69));
    }

    // ── LFO + ENV FOLLOW section ──────────────────────────────────────────────
    {
        auto la = lfoArea.reduced (13, 0);
        la.removeFromTop (22);   // section title space

        // Split: LFO on left, ENV FOLLOW on right
        const int envSectionW = 50 + 3 * la.getHeight() + 130 + 8;
        auto envSection = la.removeFromRight (juce::jmin (envSectionW, la.getWidth() / 2));

        // LFO layout: [display (70px tall narrow)] [Rate knob] [Depth knob] [Target combo] [Shape combo]
        lfoDisplay.setBounds (la.removeFromLeft (70).reduced (0, 4));
        la.removeFromLeft (6);

        auto comboCol  = la.removeFromRight (173);
        auto comboLeft = comboCol.removeFromLeft (comboCol.getWidth() / 2 - 2);
        lfoTargetCombo.setBounds (comboLeft.removeFromTop (69));
        lfoShapeCombo.setBounds  (comboCol.reduced (4, 0).removeFromTop (69));
        layoutKnobRow (la, { &lfoRateKnob, &lfoDepthKnob });

        // ENV FOLLOW layout
        envSection.removeFromLeft (8);
        envActiveButton.setBounds (envSection.removeFromLeft (50).reduced (0, (envSection.getHeight() - 30) / 2));
        envSection.removeFromLeft (4);
        auto envTargetCol = envSection.removeFromRight (130);
        envTargetCombo.setBounds (envTargetCol.reduced (0, (envTargetCol.getHeight() - 69) / 2).removeFromTop (69));
        layoutKnobRow (envSection, { &envAttackKnob, &envReleaseKnob, &envDepthKnob });
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
    paintSection (g, windowArea,  "WINDOW",      cyan);
    paintSection (g, lfoArea,     "LFO  /  ENV FOLLOW", magenta);

    // FX chain area
    g.setColour (panel);
    g.fillRect (bottomArea);
    g.setColour (border);
    g.fillRect (bottomArea.getX(), bottomArea.getY(), bottomArea.getWidth(), 1);
    g.setColour (textDim);
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.f).withStyle ("Bold")));
    g.drawText ("FX CHAIN", bottomArea.reduced (14, 6).removeFromTop (22),
                juce::Justification::centredLeft);

    // Grain count
    g.setColour (textDim);
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (12.f)));
    g.drawText (juce::String (audioProcessor.granularEngine.getActiveGrainCount()) + " grains",
                headerArea.reduced (8, 0).removeFromRight (100),
                juce::Justification::centredRight);
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
juce::AudioProcessorEditor* GladeAudioProcessor::createEditor()
{
    return new GladeAudioProcessorEditor (*this);
}
