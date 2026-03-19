#pragma once
#include <JuceHeader.h>
#include "GladeLookAndFeel.h"

// A rotary knob with name label below and live value display.
// Attach it to an APVTS parameter on construction.
class GladeKnob : public juce::Component,
                  private juce::Slider::Listener
{
public:
    GladeKnob (const juce::String& paramId,
               const juce::String& displayName,
               juce::Colour        accentColour,
               juce::AudioProcessorValueTreeState& apvts);

    ~GladeKnob() override;

    void resized() override;
    void paint  (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;

    /** Call from timerCallback to show LFO modulation ring (magenta).
        output = raw LFO value -1..1, depth = lfoDepth 0..1. Pass depth=0 to hide. */
    void setLfoOverlay (float output, float depth);

    /** Call from timerCallback to show envelope follower ring (cyan).
        value = follower output 0..1, depth = envDepth 0..1. Pass depth=0 to hide. */
    void setEnvOverlay (float value, float depth);

    juce::Slider slider;

private:
    void sliderValueChanged (juce::Slider*) override;

    float lfoModOutput = 0.f;   // -1..1
    float lfoModDepth  = 0.f;   // 0..1  (0 = hidden)
    float envModValue  = 0.f;   // 0..1
    float envModDepth  = 0.f;   // 0..1  (0 = hidden)

    juce::String paramId;
    juce::String displayName;
    juce::Colour accent;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GladeKnob)
};

//==============================================================================
// A choice/combobox selector with section-style label
class GladeCombo : public juce::Component
{
public:
    GladeCombo (const juce::String& paramId,
                const juce::String& displayName,
                juce::Colour        accentColour,
                juce::AudioProcessorValueTreeState& apvts);

    void resized() override;
    void paint  (juce::Graphics&) override;

    juce::ComboBox combo;

private:
    juce::String displayName;
    juce::Colour accent;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GladeCombo)
};
