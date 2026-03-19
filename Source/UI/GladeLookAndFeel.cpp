#include "GladeLookAndFeel.h"

GladeLookAndFeel::GladeLookAndFeel()
{
    // Global colour overrides
    setColour (juce::ResizableWindow::backgroundColourId,  GladeColors::background);
    setColour (juce::PopupMenu::backgroundColourId,            GladeColors::panelRaised);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, GladeColors::cyan.withAlpha (0.18f));
    setColour (juce::PopupMenu::textColourId,                  GladeColors::textPrimary);
    setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);
    setColour (juce::ComboBox::backgroundColourId,          GladeColors::panelRaised);
    setColour (juce::ComboBox::outlineColourId,             GladeColors::border);
    setColour (juce::ComboBox::textColourId,                GladeColors::textPrimary);
    setColour (juce::ComboBox::arrowColourId,               GladeColors::textDim);
    setColour (juce::TextEditor::backgroundColourId,        GladeColors::panelRaised);
    setColour (juce::TextEditor::textColourId,              GladeColors::textPrimary);
    setColour (juce::TextEditor::outlineColourId,           GladeColors::border);
}

//==============================================================================
void GladeLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                          int x, int y, int width, int height,
                                          float sliderPos,
                                          float startAngle, float endAngle,
                                          juce::Slider& slider)
{
    // Accent colour stored in thumbColourId by GladeKnob
    const juce::Colour accent = slider.findColour (juce::Slider::thumbColourId);

    const float outerRadius = (float) juce::jmin (width, height) * 0.5f - 3.f;
    const float cx = (float) x + (float) width  * 0.5f;
    const float cy = (float) y + (float) height * 0.5f;

    // ── Body ─────────────────────────────────────────────────────────────────
    g.setColour (GladeColors::panelRaised);
    g.fillEllipse (cx - outerRadius, cy - outerRadius, outerRadius * 2.f, outerRadius * 2.f);

    // Edge shadow ring
    g.setColour (GladeColors::background);
    g.drawEllipse (cx - outerRadius, cy - outerRadius, outerRadius * 2.f, outerRadius * 2.f, 1.5f);

    // ── Track arc (background) ────────────────────────────────────────────────
    const float arcR = outerRadius - 4.f;
    juce::Path trackArc;
    trackArc.addCentredArc (cx, cy, arcR, arcR, 0.f, startAngle, endAngle, true);
    g.setColour (GladeColors::border);
    g.strokePath (trackArc, juce::PathStrokeType (2.f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

    // ── Value arc (filled) ────────────────────────────────────────────────────
    const float valueAngle = startAngle + sliderPos * (endAngle - startAngle);
    if (sliderPos > 0.001f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc (cx, cy, arcR, arcR, 0.f, startAngle, valueAngle, true);
        g.setColour (accent);
        g.strokePath (valueArc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
    }

    // ── Dot indicator at value position ───────────────────────────────────────
    const float dotX = cx + arcR * std::sin (valueAngle);
    const float dotY = cy - arcR * std::cos (valueAngle);
    g.setColour (accent);
    g.fillEllipse (dotX - 3.5f, dotY - 3.5f, 7.f, 7.f);

    // Bright glow behind dot
    g.setColour (accent.withAlpha (0.25f));
    g.fillEllipse (dotX - 6.f, dotY - 6.f, 12.f, 12.f);
}

//==============================================================================
void GladeLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.fillAll (label.findColour (juce::Label::backgroundColourId));

    if (!label.isBeingEdited())
    {
        const float alpha = label.isEnabled() ? 1.f : 0.5f;
        g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));
        g.setFont (label.getFont());
        g.drawFittedText (label.getText(), label.getLocalBounds(),
                          label.getJustificationType(), 1, 1.f);
    }
}

//==============================================================================
void GladeLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool /*isDown*/,
                                      int /*bx*/, int /*by*/, int /*bw*/, int /*bh*/,
                                      juce::ComboBox& box)
{
    juce::Rectangle<int> bounds (0, 0, w, h);
    g.setColour (GladeColors::panelRaised);
    g.fillRoundedRectangle (bounds.toFloat(), 3.f);
    g.setColour (GladeColors::border);
    g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 3.f, 1.f);

    // Arrow
    const float arrowX = (float) w - 16.f;
    const float arrowY = (float) h * 0.5f;
    juce::Path arrow;
    arrow.addTriangle (arrowX, arrowY - 3.f,
                       arrowX + 7.f, arrowY - 3.f,
                       arrowX + 3.5f, arrowY + 3.f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

void GladeLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (GladeColors::panelRaised);
    g.setColour (GladeColors::cyan.withAlpha (0.35f));
    g.drawRect (0, 0, w, h, 1);
}

void GladeLookAndFeel::drawPopupMenuItem (juce::Graphics& g,
                                           const juce::Rectangle<int>& area,
                                           bool isSeparator, bool isActive,
                                           bool isHighlighted, bool /*isTicked*/,
                                           bool /*hasSubMenu*/,
                                           const juce::String& text,
                                           const juce::String& /*shortcut*/,
                                           const juce::Drawable* /*icon*/,
                                           const juce::Colour* /*textColour*/)
{
    if (isSeparator)
    {
        g.setColour (GladeColors::border.brighter (0.5f));
        g.fillRect (area.reduced (5, 0).removeFromTop (1));
        return;
    }

    if (isHighlighted && isActive)
    {
        g.setColour (GladeColors::cyan.withAlpha (0.18f));
        g.fillRect (area);
        g.setColour (GladeColors::cyan.withAlpha (0.5f));
        g.drawRect (area.reduced (0, 1), 1);
    }

    const bool highlighted = isHighlighted && isActive;
    g.setColour (highlighted ? juce::Colours::white
                             : (isActive ? GladeColors::textPrimary : GladeColors::textDim));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (14.f)));
    g.drawFittedText (text, area.reduced (10, 0), juce::Justification::centredLeft, 1);
}

//==============================================================================
juce::Font GladeLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions{}.withHeight (14.f));
}

void GladeLookAndFeel::drawToggleButton (juce::Graphics& g,
                                          juce::ToggleButton& btn,
                                          bool isMouseOver, bool /*isDown*/)
{
    const bool toggled  = btn.getToggleState();
    const bool inverted = static_cast<bool> (btn.getProperties()["invertedToggle"]);
    const bool on       = inverted ? !toggled : toggled;
    const auto accent   = btn.findColour (juce::ToggleButton::tickColourId);

    juce::Rectangle<float> bounds = btn.getLocalBounds().toFloat().reduced (1.f);

    g.setColour (on ? accent.withAlpha (0.2f) : GladeColors::panelRaised);
    g.fillRoundedRectangle (bounds, 3.f);

    g.setColour (on ? accent : GladeColors::border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 3.f, 1.f);

    g.setColour (on ? accent : (isMouseOver ? GladeColors::textPrimary : GladeColors::textDim));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (13.f).withStyle ("Bold")));
    g.drawFittedText (btn.getButtonText(), btn.getLocalBounds(),
                      juce::Justification::centred, 1);
}
