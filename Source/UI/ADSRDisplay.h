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
        const float a = juce::jlimit (0.001f, 10.f,  attackSec);
        const float d = juce::jlimit (0.001f, 10.f,  decaySec);
        const float s = juce::jlimit (0.f,    1.f,   sustainLevel);
        const float r = juce::jlimit (0.001f, 20.f,  releaseSec);

        // Rebuild the cached paths only when the shape parameters change.
        // cursorPosition changes every block so we never rebuild for it alone.
        if (a != attack || d != decay || s != sustain || r != release
            || pathsDirty)
        {
            attack  = a;  decay = d;  sustain = s;  release = r;
            pathsDirty = false;
            rebuildPaths();
        }

        cursorPosition = cursorPos;
    }

    void resized() override
    {
        // Force path rebuild when the component is resized.
        pathsDirty = true;
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
        const float bot  = bounds.getBottom() - 5.f;
        const float ox   = bounds.getX() + padX;
        const float w    = bounds.getWidth() - padX * 2.f;

        // Baseline
        g.setColour (GladeColors::border);
        g.drawHorizontalLine ((int) bot, ox, ox + w);

        // ── Fill, stroke, and corner dots (all pre-built in rebuildPaths) ──────
        g.setColour (GladeColors::purple.withAlpha (0.10f));
        g.fillPath (cachedFill);

        g.setColour (GladeColors::purple.withAlpha (0.85f));
        g.strokePath (cachedLine, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved));

        g.setColour (GladeColors::purple.withAlpha (0.5f));
        g.fillEllipse (dot1X - 2.5f, dot1Y - 2.5f, 5.f, 5.f);
        g.fillEllipse (dot2X - 2.5f, dot2Y - 2.5f, 5.f, 5.f);

        // ── Animated cursor dot (uses cached segment coords) ─────────────────
        // cursorPosition encoding (from GranularEngine):
        //   -1      : idle — dot hidden
        //   0–0.25  : attack  (segBot → segTop)
        //   0.25–0.5: decay   (segTop → segSustainY)
        //   0.5     : sustain (held at segXS corner)
        //   0.75–1.0: release (segXR → segXR+segWR, segSustainY → segBot)
        if (cursorPosition >= 0.f)
        {
            const float c  = juce::jlimit (0.f, 1.f, cursorPosition);
            const float wA = segXD - segXA;
            const float wD = segXS - segXD;
            float dotX, dotY;

            if (c <= 0.25f)
            {
                const float t = c / 0.25f;
                dotX = segXA + t * wA;
                dotY = segBot + t * (segTop - segBot);
            }
            else if (c <= 0.5f)
            {
                const float t = (c - 0.25f) / 0.25f;
                dotX = segXD + t * wD;
                dotY = segTop + t * (segSustainY - segTop);
            }
            else if (c < 0.75f)
            {
                dotX = segXS;
                dotY = segSustainY;
            }
            else
            {
                const float t = (c - 0.75f) / 0.25f;
                dotX = segXR + t * segWR;
                dotY = segSustainY + t * (segBot - segSustainY);
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

    // Cached geometry — rebuilt only when ADSR params or component size changes.
    juce::Path cachedFill, cachedLine;
    float      dot1X = 0.f, dot1Y = 0.f;   // sustain start corner dot
    float      dot2X = 0.f, dot2Y = 0.f;   // sustain end corner dot
    // Cursor segment endpoints, stored for the animated dot calculation in paint().
    float      segXA = 0.f, segXD = 0.f, segXS = 0.f, segXR = 0.f, segWR = 0.f;
    float      segBot = 0.f, segTop = 0.f, segSustainY = 0.f;
    bool       pathsDirty = true;

    void rebuildPaths()
    {
        const auto bounds = getLocalBounds().toFloat();
        if (bounds.isEmpty()) return;

        const float padX = 6.f;
        const float padY = 5.f;
        const float w    = bounds.getWidth()  - padX * 2.f;
        const float h    = bounds.getHeight() - padY * 2.f;
        const float ox   = bounds.getX() + padX;
        const float bot  = bounds.getBottom() - padY;
        const float top  = bounds.getY()      + padY;

        auto logNorm = [] (float t, float maxT) -> float
        {
            return std::log (1.f + t) / std::log (1.f + maxT);
        };

        const float aN = logNorm (attack,  10.f);
        const float dN = logNorm (decay,   10.f);
        const float rN = logNorm (release, 20.f);

        const float totalADR = aN + dN + rN;
        const float holdFrac = 0.25f;
        const float scale    = (totalADR > 0.f) ? ((1.f - holdFrac) / totalADR) : 1.f;

        const float wA = aN * scale * w;
        const float wD = dN * scale * w;
        const float wS = holdFrac * w;
        const float wR = rN * scale * w;

        const float xA       = ox;
        const float xD       = xA + wA;
        const float xS       = xD + wD;
        const float xR       = xS + wS;
        const float sustainY = top + (1.f - sustain) * h;

        // Store for animated cursor use in paint()
        segXA = xA;  segXD = xD;  segXS = xS;  segXR = xR;  segWR = wR;
        segBot = bot; segTop = top; segSustainY = sustainY;

        // Fill path
        cachedFill.clear();
        cachedFill.startNewSubPath (xA, bot);
        cachedFill.lineTo           (xD, top);
        cachedFill.lineTo           (xS, sustainY);
        cachedFill.lineTo           (xR, sustainY);
        cachedFill.lineTo           (xR + wR, bot);
        cachedFill.lineTo           (xA, bot);
        cachedFill.closeSubPath();

        // Stroke path
        cachedLine.clear();
        cachedLine.startNewSubPath (xA, bot);
        cachedLine.lineTo           (xD, top);
        cachedLine.lineTo           (xS, sustainY);
        cachedLine.lineTo           (xR, sustainY);
        cachedLine.lineTo           (xR + wR, bot);

        // Corner dot positions
        dot1X = xS;  dot1Y = sustainY;
        dot2X = xR;  dot2Y = sustainY;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ADSRDisplay)
};
