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

    /** Call from timerCallback to show one LFO's modulation ring in its own colour.
        lfoIndex = 0/1/2, output = raw LFO value -1..1, depth = lfoDepth 0..1.
        Pass depth=0 to hide that ring.  Rings are drawn at staggered radii so all
        three are visible simultaneously when multiple LFOs target the same knob. */
    void setLfoOverlay (int lfoIndex, float output, float depth, juce::Colour colour);

    /** Call from timerCallback to show envelope follower ring (cyan).
        value = follower output 0..1, depth = envDepth 0..1. Pass depth=0 to hide. */
    void setEnvOverlay (float value, float depth);

    juce::Slider slider;

private:
    void sliderValueChanged (juce::Slider*) override;

    struct LfoRing
    {
        float        output = 0.f;
        float        depth  = 0.f;
        juce::Colour colour { GladeColors::magenta };
    };
    LfoRing lfoRings[3];

    float envModValue  = 0.f;   // 0..1
    float envModDepth  = 0.f;   // 0..1  (0 = hidden)

    juce::String paramId;
    juce::String displayName;
    juce::String cachedValueStr { "0.00" };   // updated in sliderValueChanged, not in paint()
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
