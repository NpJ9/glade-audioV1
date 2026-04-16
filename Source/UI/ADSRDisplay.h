#pragma once
#include <JuceHeader.h>
#include "GladeLookAndFeel.h"

/** Small animated ADSR envelope shape display.
 *  Call setParams() from the editor's timerCallback, then repaint().
 *  The shape updates live as knobs are moved; the cursor dot animates
 *  through A→D→S→R driven by the actual engine envelope state. */
class ADSRDisplay : public juce::Component
{
public:
    ADSRDisplay() = default;

    /** Update shape params every timer tick.
     *  cursorPos: engine's adsrCursor value (-1 = idle/hidden, 0..1 = position). */
    void setParams (float attackSec, float decaySec, float sustainLevel, float releaseSec,
                    float cursorPos = -1.f)
    {
        attack    = juce::jlimit (0.001f, 10.f,  attackSec);
        decay     = juce::jlimit (0.001f, 10.f,  decaySec);
        sustain   = juce::jlimit (0.f,    1.f,   sustainLevel);
        release   = juce::jlimit (0.001f, 20.f,  releaseSec);
        cursorPosition = cursorPos;
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();

        // Background
        g.setColour (GladeColors::panelRaised);
        g.fillRoundedRectangle (bounds, 3.f);
        g.setColour (GladeColors::border);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 3.f, 1.f);

        const float padX = 6.f;
        const float padY = 5.f;
        const float w    = bounds.getWidth()  - padX * 2.f;
        const float h    = bounds.getHeight() - padY * 2.f;
        const float ox   = bounds.getX() + padX;
        const float bot  = bounds.getBottom() - padY;   // y = amplitude 0
        const float top  = bounds.getY()    + padY;     // y = amplitude 1

        // Baseline
        g.setColour (GladeColors::border);
        g.drawHorizontalLine ((int) bot, ox, ox + w);

        // ── Normalise segment widths on a log scale ────────────────────────────
        auto logNorm = [] (float t, float maxT) -> float
        {
            return std::log (1.f + t) / std::log (1.f + maxT);
        };

        const float aN = logNorm (attack,  10.f);
        const float dN = logNorm (decay,   10.f);
        const float rN = logNorm (release, 20.f);

        // 25% of width reserved for the sustain hold plateau
        const float totalADR = aN + dN + rN;
        const float holdFrac = 0.25f;
        const float scale    = (totalADR > 0.f) ? ((1.f - holdFrac) / totalADR) : 1.f;

        const float wA = aN * scale * w;
        const float wD = dN * scale * w;
        const float wS = holdFrac * w;
        const float wR = rN * scale * w;

        const float xA = ox;
        const float xD = xA + wA;
        const float xS = xD + wD;
        const float xR = xS + wS;

        const float sustainY = top + (1.f - sustain) * h;

        // ── Fill (under the curve) ────────────────────────────────────────────
        {
            juce::Path fill;
            fill.startNewSubPath (xA, bot);
            fill.lineTo           (xD, top);
            fill.lineTo           (xS, sustainY);
            fill.lineTo           (xR, sustainY);
            fill.lineTo           (xR + wR, bot);
            fill.lineTo           (xA, bot);
            fill.closeSubPath();
            g.setColour (GladeColors::purple.withAlpha (0.10f));
            g.fillPath (fill);
        }

        // ── Stroke ────────────────────────────────────────────────────────────
        {
            juce::Path line;
            line.startNewSubPath (xA, bot);
            line.lineTo           (xD, top);
            line.lineTo           (xS, sustainY);
            line.lineTo           (xR, sustainY);
            line.lineTo           (xR + wR, bot);
            g.setColour (GladeColors::purple.withAlpha (0.85f));
            g.strokePath (line, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved));
        }

        // ── Corner dots at sustain level transitions ──────────────────────────
        g.setColour (GladeColors::purple.withAlpha (0.5f));
        g.fillEllipse (xS - 2.5f, sustainY - 2.5f, 5.f, 5.f);
        g.fillEllipse (xR - 2.5f, sustainY - 2.5f, 5.f, 5.f);

        // ── Animated cursor dot ───────────────────────────────────────────────
        // cursorPosition encoding (from GranularEngine):
        //   -1      : idle — dot hidden
        //   0–0.25  : attack  (bot → top)
        //   0.25–0.5: decay   (top → sustainY)
        //   0.5     : sustain (held at xS corner)
        //   0.75–1.0: release (xR → xR+wR, sustainY → bot)
        if (cursorPosition >= 0.f)
        {
            const float c = juce::jlimit (0.f, 1.f, cursorPosition);
            float dotX, dotY;

            if (c <= 0.25f)
            {
                const float t = c / 0.25f;
                dotX = xA + t * wA;
                dotY = bot + t * (top - bot);
            }
            else if (c <= 0.5f)
            {
                const float t = (c - 0.25f) / 0.25f;
                dotX = xD + t * wD;
                dotY = top + t * (sustainY - top);
            }
            else if (c < 0.75f)
            {
                // Sustain — cursor rests at the decay→sustain corner
                dotX = xS;
                dotY = sustainY;
            }
            else
            {
                const float t = (c - 0.75f) / 0.25f;
                dotX = xR + t * wR;
                dotY = sustainY + t * (bot - sustainY);
            }

            // Outer glow
            g.setColour (GladeColors::purple.withAlpha (0.35f));
            g.fillEllipse (dotX - 5.f, dotY - 5.f, 10.f, 10.f);
            // Bright core
            g.setColour (GladeColors::textPrimary);
            g.fillEllipse (dotX - 2.5f, dotY - 2.5f, 5.f, 5.f);
        }
    }

private:
    float attack         = 0.05f;
    float decay          = 0.1f;
    float sustain        = 0.8f;
    float release        = 0.3f;
    float cursorPosition = -1.f;   // -1 = hidden

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ADSRDisplay)
};
