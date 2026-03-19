#include "GladeKnob.h"

//==============================================================================
GladeKnob::GladeKnob (const juce::String& pid,
                       const juce::String& name,
                       juce::Colour        accentColour,
                       juce::AudioProcessorValueTreeState& apvts)
    : paramId (pid), displayName (name), accent (accentColour)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::thumbColourId, accent);
    slider.addListener (this);
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, pid, slider);

    // Double-click resets to parameter default
    if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (pid)))
    {
        const double defaultVal = (double) param->getNormalisableRange()
                                           .convertFrom0to1 (param->getDefaultValue());
        slider.setDoubleClickReturnValue (true, defaultVal);
    }
}

GladeKnob::~GladeKnob()
{
    slider.removeListener (this);
}

void GladeKnob::resized()
{
    // Top 70% = knob, bottom 30% = name + value labels
    auto bounds = getLocalBounds();
    const int labelH = 34;
    slider.setBounds (bounds.removeFromTop (bounds.getHeight() - labelH));
}

void GladeKnob::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    const int labelH = 34;
    bounds.removeFromTop (bounds.getHeight() - labelH);

    // Value text (top of label area)
    g.setColour (accent);
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (13.f)));
    auto valueArea = bounds.removeFromTop (18);

    juce::String valueStr = juce::String (slider.getValue(), 2);
    g.drawFittedText (valueStr, valueArea, juce::Justification::centred, 1);

    // Name text (bottom of label area)
    g.setColour (GladeColors::textDim);
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.f)));
    g.drawFittedText (displayName.toUpperCase(), bounds,
                      juce::Justification::centred, 1);
}

void GladeKnob::sliderValueChanged (juce::Slider*) { repaint(); }

void GladeKnob::setLfoOverlay (float output, float depth)
{
    lfoModOutput = output;
    lfoModDepth  = depth;
}

void GladeKnob::setEnvOverlay (float value, float depth)
{
    envModValue = value;
    envModDepth = depth;
}

void GladeKnob::paintOverChildren (juce::Graphics& g)
{
    if (lfoModDepth < 0.001f && envModDepth < 0.001f) return;

    // Slider occupies top (height - labelH) pixels
    const int    labelH = 34;
    const auto   sb     = getLocalBounds().withTrimmedBottom (labelH).toFloat();
    const float  cx     = sb.getCentreX();
    const float  cy     = sb.getCentreY();
    const float  outerR = juce::jmin (sb.getWidth(), sb.getHeight()) * 0.5f - 3.f;
    const float  ringR  = outerR + 3.5f;   // draw just outside the knob body

    // Rotary angle range
    const auto  rp       = slider.getRotaryParameters();
    const float startA   = rp.startAngleRadians;
    const float endA     = rp.endAngleRadians;
    const float arcSpan  = endA - startA;

    // Knob's current normalised value → centre angle of the mod arc
    const float norm        = (float) ((slider.getValue() - slider.getMinimum())
                                       / (slider.getMaximum() - slider.getMinimum()));
    const float centerAngle = startA + norm * arcSpan;

    // ── LFO ring (magenta) ────────────────────────────────────────────────────
    if (lfoModDepth > 0.001f)
    {
        const float halfSpan = arcSpan * lfoModDepth * 0.35f;

        juce::Path rangeArc;
        rangeArc.addCentredArc (cx, cy, ringR, ringR, 0.f,
                                centerAngle - halfSpan,
                                centerAngle + halfSpan, true);
        g.setColour (GladeColors::magenta.withAlpha (0.30f));
        g.strokePath (rangeArc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));

        const float dotAngle = centerAngle + lfoModOutput * halfSpan;
        const float dotX = cx + ringR * std::sin (dotAngle);
        const float dotY = cy - ringR * std::cos (dotAngle);
        g.setColour (GladeColors::magenta.withAlpha (0.5f));
        g.fillEllipse (dotX - 5.f, dotY - 5.f, 10.f, 10.f);
        g.setColour (GladeColors::magenta);
        g.fillEllipse (dotX - 3.f, dotY - 3.f, 6.f, 6.f);
    }

    // ── Env follower ring (cyan) — slightly larger radius to avoid overlap ────
    if (envModDepth > 0.001f)
    {
        const float envRingR  = ringR + 5.f;
        const float halfSpan  = arcSpan * envModDepth * 0.35f;
        // env value is 0-1; map to -1..1 so 0.5 is neutral
        const float envNorm   = envModValue * 2.f - 1.f;

        juce::Path rangeArc;
        rangeArc.addCentredArc (cx, cy, envRingR, envRingR, 0.f,
                                centerAngle - halfSpan,
                                centerAngle + halfSpan, true);
        g.setColour (GladeColors::cyan.withAlpha (0.25f));
        g.strokePath (rangeArc, juce::PathStrokeType (2.f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));

        const float dotAngle = centerAngle + envNorm * halfSpan;
        const float dotX = cx + envRingR * std::sin (dotAngle);
        const float dotY = cy - envRingR * std::cos (dotAngle);
        g.setColour (GladeColors::cyan.withAlpha (0.5f));
        g.fillEllipse (dotX - 4.f, dotY - 4.f, 8.f, 8.f);
        g.setColour (GladeColors::cyan);
        g.fillEllipse (dotX - 2.5f, dotY - 2.5f, 5.f, 5.f);
    }
}

//==============================================================================
GladeCombo::GladeCombo (const juce::String& pid,
                         const juce::String& name,
                         juce::Colour        accentColour,
                         juce::AudioProcessorValueTreeState& apvts)
    : displayName (name), accent (accentColour)
{
    addAndMakeVisible (combo);

    // Populate from parameter choices
    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (pid)))
    {
        const auto& choices = choiceParam->choices;
        for (int i = 0; i < choices.size(); ++i)
            combo.addItem (choices[i], i + 1);
    }

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, pid, combo);
}

void GladeCombo::resized()
{
    auto bounds = getLocalBounds();
    const int labelH = 14;
    bounds.removeFromTop (labelH);  // room for name label above
    combo.setBounds (bounds);
}

void GladeCombo::paint (juce::Graphics& g)
{
    g.setColour (GladeColors::textDim);
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.f)));
    g.drawFittedText (displayName.toUpperCase(),
                      getLocalBounds().removeFromTop (16),
                      juce::Justification::centredLeft, 1);
}
