#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Engine/PresetManager.h"
#include "UI/GladeLookAndFeel.h"
#include "UI/GladeKnob.h"
#include "UI/WaveformDisplay.h"
#include "UI/FXSlotUI.h"
#include "UI/FractalVisualizer.h"
#include "UI/LFODisplay.h"
#include "UI/ADSRDisplay.h"
#include "UI/StepSequencerUI.h"

class GladeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   public juce::DragAndDropContainer,
                                   private juce::Timer
{
public:
    explicit GladeAudioProcessorEditor (GladeAudioProcessor&);
    ~GladeAudioProcessorEditor() override;

    void paint  (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    void paintSection (juce::Graphics&, juce::Rectangle<int> bounds,
                       const juce::String& title, juce::Colour accent);

    // Randomise helpers
    void randomiseGrain (bool wild);
    void randomisePitch (bool wild);
    void randomiseAll   (bool wild);

    static void setParamRandom (juce::AudioProcessorValueTreeState& apvts,
                                const juce::String& id, float minV, float maxV,
                                juce::Random& rng);

    GladeAudioProcessor& audioProcessor;
    PresetManager        presetManager;
    GladeLookAndFeel     laf;
    juce::Random         rng;
    int                  cachedGrainCount  = 0;
    bool                 sampleWasLoaded   = false;

    //==========================================================================
    // Header
    juce::Label      logoLabel;
    juce::TextButton presetMenuButton;          // shows current name; click → dropdown
    juce::TextButton prevPresetButton { "<" };
    juce::TextButton nextPresetButton { ">" };

    void showPresetMenu();
    void onPresetChosen (int index);
    juce::TextButton rndAllButton     { "RND ALL" };
    juce::ToggleButton wildButton     { "WILD" };

    //==========================================================================
    // Top row
    WaveformDisplay   waveformDisplay;
    FractalVisualizer fractalVisualizer;

    GladeKnob   attackKnob, decayKnob, sustainKnob, releaseKnob;
    ADSRDisplay adsrDisplay;

    //==========================================================================
    // GRAIN section
    GladeKnob          grainSizeKnob, grainDensityKnob, grainPositionKnob, posJitterKnob;
    GladeKnob          reverseKnob;
    juce::TextButton   grainRndButton { "RND" };
    juce::ToggleButton beatSyncButton { "SYNC" };
    GladeCombo         beatDivCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> beatSyncAtt;
    juce::ToggleButton seqActiveButton { "SEQ" };
    StepSequencerUI    stepSeqUI;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> seqActiveAtt;
    juce::ToggleButton freezeButton    { "FREEZE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAtt;

    // PITCH section
    GladeKnob        pitchShiftKnob, pitchJitterKnob, panSpreadKnob;
    GladeKnob        glideKnob;
    juce::TextButton pitchRndButton { "RND" };

    // OUTPUT section
    GladeKnob outputGainKnob, dryWetKnob;
    GladeKnob velocityKnob;

    //==========================================================================
    // WINDOW section + MACRO knobs
    GladeCombo windowCombo;
    GladeKnob  m1Knob, m2Knob, m3Knob, m4Knob;
    GladeCombo m1TargetCombo, m2TargetCombo, m3TargetCombo, m4TargetCombo;

    // LFO section — 3 independent LFOs, selector switches which set is visible
    GladeKnob  lfo1RateKnob, lfo1DepthKnob;
    GladeCombo lfo1ShapeCombo, lfo1TargetCombo;
    GladeKnob  lfo2RateKnob, lfo2DepthKnob;
    GladeCombo lfo2ShapeCombo, lfo2TargetCombo;
    GladeKnob  lfo3RateKnob, lfo3DepthKnob;
    GladeCombo lfo3ShapeCombo, lfo3TargetCombo;

    LFODisplay lfoDisplay;
    juce::TextButton lfoSelBtn[3];
    int activeLfo = 0;   // 0/1/2


    //==========================================================================
    // FX Chain — 4 slots (type combo is now inside each FXSlotUI)
    std::array<std::unique_ptr<FXSlotUI>, 4> fxSlots;

    //==========================================================================
    // Layout helpers
    juce::Rectangle<int> headerArea, topArea, midArea, bottomArea;
    juce::Rectangle<int> waveArea, fractalArea, envArea;
    juce::Rectangle<int> grainArea, pitchArea, outputArea;
    juce::Rectangle<int> windowArea, lfoArea, macroArea;

    void layoutKnobRow (juce::Rectangle<int> rowBounds,
                        std::initializer_list<GladeKnob*> knobs);

    // Swap all 6 APVTS params between two FX slots
    void swapFxSlots (int a, int b);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GladeAudioProcessorEditor)
};
