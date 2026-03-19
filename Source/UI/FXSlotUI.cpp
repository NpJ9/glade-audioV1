#include "FXSlotUI.h"

using namespace GladeColors;

//==============================================================================
// Static effect info table
const FXSlotUI::EffectInfo& FXSlotUI::infoForType (int type)
{
    static const EffectInfo table[] =
    {
        { "NONE",     "-",      "-",       "-",       GladeColors::border  },
        { "REVERB",   "Size",   "Damp",    "Width",   GladeColors::purple  },
        { "DELAY",    "Time",   "Feedbk",  "Spread",  GladeColors::cyan    },
        { "CHORUS",   "Rate",   "Depth",   "Feedbk",  GladeColors::green   },
        { "DISTORT",  "Drive",  "Tone",    "---",     GladeColors::magenta },
        { "FILTER",   "Cutoff", "Res",     "Type",    GladeColors::yellow  },
        { "SHIMMER",  "Size",   "Damp",    "Shimmer", GladeColors::purple  },
        { "L.CHORUS", "Rate",   "Depth",   "Feedbk",  GladeColors::green   },
    };
    return table[juce::jlimit (0, 7, type)];
}

//==============================================================================
FXSlotUI::FXSlotUI (int idx, juce::AudioProcessorValueTreeState& apvts_)
    : slotIndex (idx), apvts (apvts_)
{
    const juce::String s (idx);

    // ── Drag grip ────────────────────────────────────────────────────────────
    dragGrip.ownerSlot = idx;
    addAndMakeVisible (dragGrip);

    // ── Type combo ────────────────────────────────────────────────────────────
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("fxType" + s)))
        for (int i = 0; i < param->choices.size(); ++i)
            typeCombo.addItem (param->choices[i], i + 1);
    addAndMakeVisible (typeCombo);
    typeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, "fxType" + s, typeCombo);

    // ── Bypass toggle ─────────────────────────────────────────────────────────
    // fxBypass=true means BYPASSED (effect OFF) — so we invert the lit/dim visual
    bypassButton.getProperties().set ("invertedToggle", true);
    bypassButton.setColour (juce::ToggleButton::tickColourId,         cyan);
    bypassButton.setColour (juce::ToggleButton::tickDisabledColourId, textDim);
    bypassButton.onStateChange = [this] { updateBypassVisual(); };
    addAndMakeVisible (bypassButton);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, "fxBypass" + s, bypassButton);

    // ── Parameter sliders ────────────────────────────────────────────────────
    for (auto* sl : { &p1Slider, &p2Slider, &p3Slider, &mixSlider })
    {
        sl->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        sl->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        sl->setColour (juce::Slider::thumbColourId, textDim); // overridden in updateForType
        sl->onValueChange = [this] { repaint(); };
        addAndMakeVisible (sl);
    }
    mixSlider.setColour (juce::Slider::thumbColourId, textDim);

    p1Att  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "fxP1"  + s, p1Slider);
    p2Att  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "fxP2"  + s, p2Slider);
    p3Att  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "fxP3"  + s, p3Slider);
    mixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "fxMix" + s, mixSlider);

    // ── Listen for type changes ───────────────────────────────────────────────
    apvts.addParameterListener ("fxType" + s, this);

    // Set initial state
    const int initType = (int) apvts.getRawParameterValue ("fxType" + s)->load();
    updateForType (initType);
    updateBypassVisual();
}

FXSlotUI::~FXSlotUI()
{
    apvts.removeParameterListener ("fxType" + juce::String (slotIndex), this);
    cancelPendingUpdate();
}

//==============================================================================
void FXSlotUI::parameterChanged (const juce::String&, float)
{
    triggerAsyncUpdate(); // bounce to message thread
}

void FXSlotUI::handleAsyncUpdate()
{
    const int type = (int) apvts.getRawParameterValue (
        "fxType" + juce::String (slotIndex))->load();
    updateForType (type);
    repaint();
}

void FXSlotUI::updateForType (int type)
{
    currentType = type;
    const auto& info = infoForType (type);

    typeName     = info.name;
    p1Label      = info.p1;
    p2Label      = info.p2;
    p3Label      = info.p3;
    accentColour = info.colour;

    const bool hasEffect = (type != 0);
    for (auto* sl : { &p1Slider, &p2Slider, &p3Slider, &mixSlider })
    {
        sl->setEnabled (hasEffect);
        sl->setColour (juce::Slider::thumbColourId, accentColour);
    }

    bypassButton.setColour (juce::ToggleButton::tickColourId, accentColour);
    updateBypassVisual();
}

void FXSlotUI::updateBypassVisual()
{
    // fxBypass=true → effect is OFF; fxBypass=false → effect is ON
    const bool bypassed = bypassButton.getToggleState();
    bypassButton.setButtonText (bypassed ? "OFF" : "ON");
    repaint();
}

//==============================================================================
void FXSlotUI::resized()
{
    auto bounds = getLocalBounds().reduced (8, 5);

    // ── Top row: drag grip + type combo + bypass button ───────────────────────
    auto topRow = bounds.removeFromTop (34);
    bypassButton.setBounds (topRow.removeFromRight (50));
    topRow.removeFromRight (5);
    dragGrip.setBounds (topRow.removeFromLeft (18));
    topRow.removeFromLeft (4);
    typeCombo.setBounds (topRow);

    bounds.removeFromTop (5); // gap

    // ── Knob row ──────────────────────────────────────────────────────────────
    const int labelH = 40;
    const int kh     = bounds.getHeight() - labelH;
    const int kw     = bounds.getWidth() / 4;
    int x = bounds.getX();
    const int ky = bounds.getY();

    p1Slider .setBounds (x,      ky, kw, kh); x += kw;
    p2Slider .setBounds (x,      ky, kw, kh); x += kw;
    p3Slider .setBounds (x,      ky, kw, kh); x += kw;
    mixSlider.setBounds (x,      ky, kw, kh);
}

//==============================================================================
void FXSlotUI::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // ── Panel background ─────────────────────────────────────────────────────
    g.setColour (panel);
    g.fillRoundedRectangle (bounds.reduced (2.f), 4.f);

    // ── Accent border (colour = effect type) ──────────────────────────────────
    g.setColour (currentType == 0 ? border : accentColour.withAlpha (0.5f));
    g.drawRoundedRectangle (bounds.reduced (2.5f), 4.f, 1.f);

    // ── Drop-target highlight ─────────────────────────────────────────────────
    if (isDragTarget)
    {
        g.setColour (GladeColors::cyan.withAlpha (0.18f));
        g.fillRoundedRectangle (bounds.reduced (2.f), 4.f);
        g.setColour (GladeColors::cyan.withAlpha (0.8f));
        g.drawRoundedRectangle (bounds.reduced (2.5f), 4.f, 2.f);
    }

    // ── Top accent line ───────────────────────────────────────────────────────
    if (currentType != 0)
    {
        g.setColour (accentColour);
        g.fillRoundedRectangle (bounds.reduced (2.f).removeFromTop (2.f), 2.f);
    }

    // ── Knob labels + value readouts ─────────────────────────────────────────
    if (currentType != 0)
    {
        const int n      = 4;
        const int kw     = (getWidth() - 16) / n;
        const int labelY = getHeight() - 22;
        const int valY   = getHeight() - 38;

        const juce::String labels[] = { p1Label, p2Label, p3Label, "Mix" };

        // Delay division names for p1
        static const char* divNames[] = {
            "1/32","1/16T","1/16","1/8T","1/8","1/4T","1/4","1/2","1 bar"
        };

        const float p1v  = (float) p1Slider .getValue();
        const float p2v  = (float) p2Slider .getValue();
        const float p3v  = (float) p3Slider .getValue();
        const float mixv = (float) mixSlider.getValue();

        auto formatP1 = [&]() -> juce::String {
            if (currentType == 2) // Delay
            {
                const int divIdx = juce::jlimit (0, 8, (int) (p1v * 9.f));
                return divNames[divIdx];
            }
            if (currentType == 4) return juce::String (juce::roundToInt (p1v * 9.f + 1.f)) + "x"; // Distort drive
            if (currentType == 5) // Filter cutoff in Hz
            {
                const float hz = 20.f * std::pow (1000.f, p1v);
                return hz < 1000.f ? juce::String ((int) hz) + "Hz"
                                   : juce::String (hz / 1000.f, 1) + "k";
            }
            return juce::String (juce::roundToInt (p1v * 100.f)) + "%";
        };

        const juce::String values[] = {
            formatP1(),
            juce::String (juce::roundToInt (p2v * 100.f)) + "%",
            currentType == 5   ? (p3v < 0.33f ? "LP" : p3v < 0.67f ? "HP" : "BP")
                               : juce::String (juce::roundToInt (p3v * 100.f)) + "%",
            juce::String (juce::roundToInt (mixv * 100.f)) + "%"
        };

        g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.f)));
        for (int i = 0; i < n; ++i)
        {
            g.setColour (accentColour.withAlpha (0.85f));
            g.drawText (values[i], 8 + i * kw, valY, kw, 16, juce::Justification::centred);

            g.setColour (textDim);
            g.drawText (labels[i].toUpperCase(),
                        8 + i * kw, labelY, kw, 20,
                        juce::Justification::centred);
        }
    }
}

//==============================================================================
// DragAndDropTarget
bool FXSlotUI::isInterestedInDragSource (const SourceDetails& details)
{
    return details.description.toString().startsWith ("fx-slot-");
}

void FXSlotUI::itemDragEnter (const SourceDetails& details)
{
    // Don't highlight when hovering over self
    const int src = details.description.toString().substring (8).getIntValue();
    isDragTarget = (src != slotIndex);
    repaint();
}

void FXSlotUI::itemDragExit (const SourceDetails&)
{
    isDragTarget = false;
    repaint();
}

void FXSlotUI::itemDropped (const SourceDetails& details)
{
    isDragTarget = false;
    repaint();
    const int src = details.description.toString().substring (8).getIntValue();
    if (src != slotIndex && onSwap)
        onSwap (src, slotIndex);
}
