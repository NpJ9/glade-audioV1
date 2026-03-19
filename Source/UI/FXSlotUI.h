#pragma once
#include <JuceHeader.h>
#include "GladeLookAndFeel.h"
#include "../Engine/FXChain.h"

//==============================================================================
// One FX slot panel: bypass toggle, type selector, 3 parameter knobs + mix knob.
// Labels update automatically when the effect type changes.
// Supports drag-and-drop reordering — drag from the grip handle on the left.
class FXSlotUI : public juce::Component,
                 public juce::DragAndDropTarget,
                 public juce::AudioProcessorValueTreeState::Listener,
                 private juce::AsyncUpdater
{
public:
    // Called when user drops another slot onto this one. Args: (srcSlot, dstSlot).
    std::function<void(int, int)> onSwap;

    FXSlotUI (int slotIndex, juce::AudioProcessorValueTreeState& apvts);
    ~FXSlotUI() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

    // AudioProcessorValueTreeState::Listener
    void parameterChanged (const juce::String& paramId, float newValue) override;

    // DragAndDropTarget
    bool isInterestedInDragSource (const SourceDetails& details) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragExit  (const SourceDetails&) override;
    void itemDropped   (const SourceDetails& details) override;

private:
    // AsyncUpdater — dispatches type-change to message thread
    void handleAsyncUpdate() override;

    void updateForType      (int type);
    void updateBypassVisual ();
    void layoutSliders      ();

    int  slotIndex;
    juce::AudioProcessorValueTreeState& apvts;

    // ── Drag grip ────────────────────────────────────────────────────────────
    struct DragGrip : public juce::Component
    {
        int ownerSlot = 0;
        void paint (juce::Graphics& g) override
        {
            g.setColour (GladeColors::textDim.withAlpha (0.55f));
            const float cx = getWidth() * 0.5f;
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 2; ++col)
                    g.fillEllipse (cx - 4.f + col * 5.f,
                                   getHeight() * 0.5f - 5.f + row * 5.f,
                                   2.5f, 2.5f);
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (e.getDistanceFromDragStart() < 6) return;
            if (auto* dc = juce::DragAndDropContainer::findParentDragContainerFor (this))
                dc->startDragging ("fx-slot-" + juce::String (ownerSlot), this,
                                   juce::ScaledImage(), true);
        }
        juce::MouseCursor getMouseCursor() override
        {
            return juce::MouseCursor::DraggingHandCursor;
        }
    };
    DragGrip dragGrip;

    bool isDragTarget = false;   // visual highlight when another slot is dragged over

    // ── Controls ─────────────────────────────────────────────────────────────
    juce::ComboBox     typeCombo;
    juce::ToggleButton bypassButton;
    juce::Slider       p1Slider, p2Slider, p3Slider, mixSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   bypassAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   p1Att, p2Att,
                                                                             p3Att, mixAtt;

    // ── Display state (updated on type change) ────────────────────────────────
    int          currentType = 0;
    juce::Colour accentColour { GladeColors::border };

    juce::String typeName { "NONE" };
    juce::String p1Label  { "-" };
    juce::String p2Label  { "-" };
    juce::String p3Label  { "-" };

    // Static lookup: info per FX type
    struct EffectInfo
    {
        const char* name;
        const char* p1; const char* p2; const char* p3;
        juce::Colour colour;
    };
    static const EffectInfo& infoForType (int type);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FXSlotUI)
};
