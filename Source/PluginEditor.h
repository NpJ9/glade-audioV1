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
    int                  cachedGrainCount = 0;

    //==========================================================================
    // Header
    juce::Label      logoLabel;
    juce::Label      presetNameLabel;
    juce::TextButton prevPresetButton { "<" };
    juce::TextButton nextPresetButton { ">" };
    juce::TextButton rndAllButton     { "RND ALL" };
    juce::ToggleButton wildButton     { "WILD" };

    //==========================================================================
    // Top row
    WaveformDisplay   waveformDisplay;
    FractalVisualizer fractalVisualizer;

    GladeKnob attackKnob, decayKnob, sustainKnob, releaseKnob;

    //==========================================================================
    // GRAIN section
    GladeKnob          grainSizeKnob, grainDensityKnob, grainPositionKnob, posJitterKnob;
    juce::TextButton   grainRndButton { "RND" };
    juce::ToggleButton beatSyncButton { "SYNC" };
    GladeCombo         beatDivCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> beatSyncAtt;
    juce::ToggleButton seqActiveButton { "SEQ" };
    StepSequencerUI    stepSeqUI;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> seqActiveAtt;

    // PITCH section
    GladeKnob        pitchShiftKnob, pitchJitterKnob, panSpreadKnob;
    juce::TextButton pitchRndButton { "RND" };

    // OUTPUT section
    GladeKnob outputGainKnob, dryWetKnob;

    //==========================================================================
    // WINDOW section
    GladeCombo windowCombo;

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

    // ENV FOLLOW section
    juce::ToggleButton envActiveButton { "ENV" };
    GladeKnob          envAttackKnob, envReleaseKnob, envDepthKnob;
    GladeCombo         envTargetCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> envActiveAtt;

    //==========================================================================
    // FX Chain — 4 slots (type combo is now inside each FXSlotUI)
    std::array<std::unique_ptr<FXSlotUI>, 4> fxSlots;

    //==========================================================================
    // Layout helpers
    juce::Rectangle<int> headerArea, topArea, midArea, bottomArea;
    juce::Rectangle<int> waveArea, fractalArea, envArea;
    juce::Rectangle<int> grainArea, pitchArea, outputArea;
    juce::Rectangle<int> windowArea, lfoArea;

    void layoutKnobRow (juce::Rectangle<int> rowBounds,
                        std::initializer_list<GladeKnob*> knobs);

    // Swap all 6 APVTS params between two FX slots
    void swapFxSlots (int a, int b);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GladeAudioProcessorEditor)
};
