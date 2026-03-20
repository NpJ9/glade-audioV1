#pragma once
#include <JuceHeader.h>

//==============================================================================
// Glade colour palette
namespace GladeColors
{
    inline const juce::Colour background  { 0xff090b10 };
    inline const juce::Colour panel       { 0xff111318 };
    inline const juce::Colour panelRaised { 0xff191d28 };
    inline const juce::Colour border      { 0xff1e2436 };
    inline const juce::Colour textPrimary { 0xffdde4f0 };
    inline const juce::Colour textDim     { 0xff8898b8 };

    inline const juce::Colour cyan        { 0xff00e5d4 };
    inline const juce::Colour green       { 0xff1aff8c };
    inline const juce::Colour magenta     { 0xffff2a6d };
    inline const juce::Colour purple      { 0xff9b5de5 };
    inline const juce::Colour yellow      { 0xfff5c518 };
}

//==============================================================================
class GladeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GladeLookAndFeel();

    //==========================================================================
    // Rotary knob — arc ring style
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    // Labels
    void drawLabel (juce::Graphics&, juce::Label&) override;

    // ComboBox
    void drawComboBox (juce::Graphics&, int w, int h, bool isDown,
                       int bx, int by, int bw, int bh,
                       juce::ComboBox&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>&,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu,
                            const juce::String& text, const juce::String& shortcut,
                            const juce::Drawable* icon,
                            const juce::Colour* textColour) override;

    // Toggle button
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool isMouseOverButton, bool isButtonDown) override;

    // Combo box font
    juce::Font getComboBoxFont (juce::ComboBox&) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GladeLookAndFeel)
};
