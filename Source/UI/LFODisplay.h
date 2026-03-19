#pragma once
#include <JuceHeader.h>
#include "GladeLookAndFeel.h"

/** Small animated LFO waveform display.
 *  Call setLfoState() from the editor's timerCallback, then repaint(). */
class LFODisplay : public juce::Component
{
public:
    LFODisplay() = default;

    void setLfoState (int shape, float phase, float depth)
    {
        lfoShape = shape;
        lfoPhase = juce::jlimit (0.f, 1.f, phase);
        lfoDepth = juce::jlimit (0.f, 1.f, depth);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Background
        g.setColour (GladeColors::panelRaised);
        g.fillRoundedRectangle (bounds, 3.f);

        g.setColour (GladeColors::border);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 3.f, 1.f);

        const float w   = bounds.getWidth()  - 6.f;
        const float h   = bounds.getHeight() - 4.f;
        const float ox  = bounds.getX() + 3.f;
        const float cy  = bounds.getCentreY();
        const float amp = h * 0.42f;

        // Centre line
        g.setColour (GladeColors::border);
        g.drawHorizontalLine ((int) cy, ox, ox + w);

        if (lfoDepth < 0.005f)
        {
            // Flat line — LFO has no effect
            g.setColour (GladeColors::textDim.withAlpha (0.35f));
            g.drawHorizontalLine ((int) cy, ox, ox + w);
            return;
        }

        // ── Waveform path ─────────────────────────────────────────────────────
        const int steps = (int) w;
        juce::Path wave;
        for (int i = 0; i < steps; ++i)
        {
            const float t  = (float) i / (float) (steps - 1);
            const float v  = evaluateLFO (lfoShape, t) * lfoDepth;
            const float px = ox + t * w;
            const float py = cy - v * amp;
            if (i == 0) wave.startNewSubPath (px, py);
            else         wave.lineTo          (px, py);
        }

        g.setColour (GladeColors::magenta.withAlpha (0.55f));
        g.strokePath (wave, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved));

        // ── Phase fill (area behind the cursor) ──────────────────────────────
        {
            juce::Path fill;
            const int phaseStep = juce::jlimit (0, steps - 1, (int) (lfoPhase * (float) steps));
            fill.startNewSubPath (ox, cy);
            for (int i = 0; i <= phaseStep; ++i)
            {
                const float t  = (float) i / (float) (steps - 1);
                const float v  = evaluateLFO (lfoShape, t) * lfoDepth;
                fill.lineTo (ox + t * w, cy - v * amp);
            }
            fill.lineTo (ox + lfoPhase * w, cy);
            fill.closeSubPath();
            g.setColour (GladeColors::magenta.withAlpha (0.08f));
            g.fillPath (fill);
        }

        // ── Phase cursor line ─────────────────────────────────────────────────
        const float cursorX = ox + lfoPhase * w;
        g.setColour (GladeColors::magenta.withAlpha (0.9f));
        g.drawLine (cursorX, bounds.getY() + 2.f,
                    cursorX, bounds.getBottom() - 2.f, 1.5f);

        // Dot on the waveform at cursor position
        const float cursorV   = evaluateLFO (lfoShape, lfoPhase) * lfoDepth;
        const float cursorDotY = cy - cursorV * amp;
        g.setColour (GladeColors::magenta);
        g.fillEllipse (cursorX - 3.f, cursorDotY - 3.f, 6.f, 6.f);
    }

private:
    int   lfoShape = 0;
    float lfoPhase = 0.f;
    float lfoDepth = 0.f;

    static float evaluateLFO (int shape, float t) noexcept
    {
        switch (shape)
        {
            case 0: return std::sin (t * juce::MathConstants<float>::twoPi);
            case 1: return 1.f - 4.f * std::abs (t - 0.5f);   // Triangle
            case 2: return t * 2.f - 1.f;                      // Saw
            case 3: return t < 0.5f ? 1.f : -1.f;             // Square
            default: return 0.f;                                // S&H — static
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LFODisplay)
};
