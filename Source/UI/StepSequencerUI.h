#pragma once
#include <JuceHeader.h>
#include "GladeLookAndFeel.h"
#include "../Engine/StepSequencer.h"

/** 8-step position sequencer bar UI.
 *  Click/drag to set each step's value (0-1 = grain position).
 *  Call setCurrentStep() from timerCallback to highlight the playing step.
 *  onStepChanged(stepIdx, value) fires when the user edits a step. */
class StepSequencerUI : public juce::Component
{
public:
    StepSequencerUI() = default;

    std::function<void (int, float)> onStepChanged;

    void setStepValues (const float* vals)
    {
        for (int i = 0; i < StepSequencer::kNumSteps; ++i)
            stepValues[i] = vals[i];
        repaint();
    }

    void setCurrentStep (int step)
    {
        if (currentStep != step)
        {
            currentStep = step;
            repaint();
        }
    }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        const int n   = StepSequencer::kNumSteps;
        const float w = (float) getWidth();
        const float h = (float) getHeight();
        const float sw = w / (float) n;   // step width

        // Background
        g.setColour (GladeColors::panelRaised);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.f);
        g.setColour (GladeColors::border);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 3.f, 1.f);

        for (int i = 0; i < n; ++i)
        {
            const float x   = (float) i * sw;
            const float val = juce::jlimit (0.f, 1.f, stepValues[i]);
            const float barH = val * (h - 4.f);
            const float barY = h - 2.f - barH;

            const bool isActive = (i == currentStep);
            const juce::Colour barColour = isActive ? GladeColors::cyan
                                                     : GladeColors::cyan.withAlpha (0.45f);

            // Step background highlight when active
            if (isActive)
            {
                g.setColour (GladeColors::cyan.withAlpha (0.12f));
                g.fillRoundedRectangle (x + 1.f, 1.f, sw - 2.f, h - 2.f, 2.f);
            }

            // Bar fill
            g.setColour (barColour);
            g.fillRoundedRectangle (x + 2.f, barY, sw - 4.f, barH, 2.f);

            // Step divider
            if (i > 0)
            {
                g.setColour (GladeColors::border);
                g.drawLine (x, 2.f, x, h - 2.f, 1.f);
            }

            // Group divider (every 4 steps)
            if (i == 4)
            {
                g.setColour (GladeColors::textDim.withAlpha (0.4f));
                g.drawLine (x, 2.f, x, h - 2.f, 2.f);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override  { setStepFromMouse (e); }
    void mouseDrag (const juce::MouseEvent& e) override  { setStepFromMouse (e); }

private:
    float stepValues[StepSequencer::kNumSteps] = { 0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f };
    int   currentStep = -1;

    void setStepFromMouse (const juce::MouseEvent& e)
    {
        const int n  = StepSequencer::kNumSteps;
        const float sw = (float) getWidth() / (float) n;
        const int idx = juce::jlimit (0, n - 1, (int) ((float) e.x / sw));
        const float val = juce::jlimit (0.f, 1.f,
                                         1.f - (float) e.y / (float) getHeight());
        stepValues[idx] = val;
        repaint();
        if (onStepChanged) onStepChanged (idx, val);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepSequencerUI)
};
