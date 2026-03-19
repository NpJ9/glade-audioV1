#pragma once
#include <JuceHeader.h>
#include "GladeLookAndFeel.h"
#include "../Engine/FXChain.h"

//==============================================================================
// One FX slot panel: bypass toggle, type selector, 3 parameter knobs + mix knob.
// Labels update automatically when the effect type changes.
class FXSlotUI : public juce::Component,
                 public juce::AudioProcessorValueTreeState::Listener,
                 private juce::AsyncUpdater
{
public:
    FXSlotUI (int slotIndex, juce::AudioProcessorValueTreeState& apvts);
    ~FXSlotUI() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

    // AudioProcessorValueTreeState::Listener
    void parameterChanged (const juce::String& paramId, float newValue) override;

private:
    // AsyncUpdater — dispatches type-change to message thread
    void handleAsyncUpdate() override;

    void updateForType      (int type);
    void updateBypassVisual ();
    void layoutSliders      ();

    int  slotIndex;
    juce::AudioProcessorValueTreeState& apvts;

    // ── Controls ─────────────────────────────────────────────────────────────
    juce::ComboBox     typeCombo;
    juce::ToggleButton bypassButton;
    juce::Slider       p1Slider, p2Slider, p3Slider, mixSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   bypassAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   p1Att, p2Att,
                                                                             p3Att, mixAtt;

    // ── Display state (updated on type change) ────────────────────────────────
    int          currentType = 0;
    juce::Colour accentColour { GladeColors::border };

    juce::String typeName { "NONE" };
    juce::String p1Label  { "-" };
    juce::String p2Label  { "-" };
    juce::String p3Label  { "-" };

    // Static lookup: info per FX type
    struct EffectInfo
    {
        const char* name;
        const char* p1; const char* p2; const char* p3;
        juce::Colour colour;
    };
    static const EffectInfo& infoForType (int type);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FXSlotUI)
};
