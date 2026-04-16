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

void GladeKnob::setLfoOverlay (int lfoIndex, float output, float depth, juce::Colour colour)
{
    if (lfoIndex < 0 || lfoIndex > 2) return;
    lfoRings[lfoIndex] = { output, depth, colour };
}

void GladeKnob::setEnvOverlay (float value, float depth)
{
    envModValue = value;
    envModDepth = depth;
}

void GladeKnob::paintOverChildren (juce::Graphics& g)
{
    const bool anyLfo = lfoRings[0].depth > 0.001f
                     || lfoRings[1].depth > 0.001f
                     || lfoRings[2].depth > 0.001f;
    if (!anyLfo && envModDepth < 0.001f) return;

    // Slider occupies top (height - labelH) pixels
    const int    labelH = 34;
    const auto   sb     = getLocalBounds().withTrimmedBottom (labelH).toFloat();
    const float  cx     = sb.getCentreX();
    const float  cy     = sb.getCentreY();
    const float  outerR = juce::jmin (sb.getWidth(), sb.getHeight()) * 0.5f - 3.f;

    // Rotary angle range
    const auto  rp          = slider.getRotaryParameters();
    const float startA      = rp.startAngleRadians;
    const float endA        = rp.endAngleRadians;
    const float arcSpan     = endA - startA;

    // Knob's current normalised value → centre angle of the mod arc
    const float norm        = (float) ((slider.getValue() - slider.getMinimum())
                                       / (slider.getMaximum() - slider.getMinimum()));
    const float centerAngle = startA + norm * arcSpan;

    // ── LFO rings — staggered radii so all three are simultaneously visible ────
    // Ring spacing: 4 px between centres; stroke widths taper inward.
    static constexpr float kBaseOffset  = 3.5f;   // gap between knob body and innermost ring
    static constexpr float kRingStep    = 4.5f;   // radial step per LFO index
    static constexpr float kStrokeW[3] = { 2.5f, 2.0f, 2.0f };

    for (int li = 0; li < 3; ++li)
    {
        const auto& ring = lfoRings[li];
        if (ring.depth < 0.001f) continue;

        const float ringR    = outerR + kBaseOffset + (float) li * kRingStep;
        // Clamp so arc stays within [startA, endA] — prevents wrapping artefacts
        const float rawHalf  = arcSpan * ring.depth * 0.35f;
        const float halfSpan = juce::jmin (rawHalf,
                                           juce::jmin (centerAngle - startA,
                                                        endA - centerAngle));
        if (halfSpan <= 0.f) continue;

        juce::Path rangeArc;
        rangeArc.addCentredArc (cx, cy, ringR, ringR, 0.f,
                                centerAngle - halfSpan,
                                centerAngle + halfSpan, true);
        g.setColour (ring.colour.withAlpha (0.28f));
        g.strokePath (rangeArc, juce::PathStrokeType (kStrokeW[li],
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));

        const float dotAngle = centerAngle + juce::jlimit (-1.f, 1.f, ring.output) * halfSpan;
        const float dotX = cx + ringR * std::sin (dotAngle);
        const float dotY = cy - ringR * std::cos (dotAngle);
        g.setColour (ring.colour.withAlpha (0.5f));
        g.fillEllipse (dotX - 4.5f, dotY - 4.5f, 9.f, 9.f);
        g.setColour (ring.colour);
        g.fillEllipse (dotX - 2.5f, dotY - 2.5f, 5.f, 5.f);
    }

    // ── Env follower ring (cyan) — outermost ring ─────────────────────────────
    if (envModDepth > 0.001f)
    {
        const float envRingR  = outerR + kBaseOffset + 3.f * kRingStep;   // one step past LFO3
        const float rawHalf   = arcSpan * envModDepth * 0.35f;
        const float halfSpan  = juce::jmin (rawHalf,
                                            juce::jmin (centerAngle - startA,
                                                         endA - centerAngle));
        if (halfSpan > 0.f)
        {
            // env value is 0-1; map to -1..1 so 0.5 is neutral
            const float envNorm = juce::jlimit (-1.f, 1.f, envModValue * 2.f - 1.f);

            juce::Path rangeArc;
            rangeArc.addCentredArc (cx, cy, envRingR, envRingR, 0.f,
                                    centerAngle - halfSpan,
                                    centerAngle + halfSpan, true);
            g.setColour (GladeColors::cyan.withAlpha (0.22f));
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
