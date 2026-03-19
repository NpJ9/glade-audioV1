#pragma once
#include <JuceHeader.h>
#include "GladeLookAndFeel.h"
#include <array>

// Shows up to 4 sample slots as horizontal lanes.
// Each lane accepts drag-and-drop or click-to-load; shows a mini waveform.
// The active slot (closest root note to current MIDI note) is highlighted.
// In wavetable/synth mode, shows a single waveform.
class SampleMapUI : public juce::Component,
                    public juce::FileDragAndDropTarget,
                    private juce::Slider::Listener
{
public:
    // Called when the user loads a file. slot = 0-3.
    std::function<bool(int slot, juce::File)> onFileLoad;

    explicit SampleMapUI (juce::AudioProcessorValueTreeState& apvts);
    ~SampleMapUI() override;

    // Call after loading to update the waveform path for that slot.
    void setSlotBuffer (int slot, const juce::AudioBuffer<float>& buffer);

    // Set which slot is currently being played (-1 = none).
    void setActiveSlot (int slot);

    // Root note label per slot (e.g. "C4") — pass note name string.
    void setSlotRootNote (int slot, const juce::String& name);

    // Switch to wavetable display mode (shows single waveform, no slots).
    void setWavetableMode (bool on, const juce::AudioBuffer<float>* wavetable);

    void paint   (juce::Graphics&) override;
    void resized () override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int x, int y) override;
    void fileDragMove  (const juce::StringArray&, int x, int y) override;
    void fileDragExit  (const juce::StringArray&) override;
    void filesDropped  (const juce::StringArray& files, int x, int y) override;

    void mouseDown (const juce::MouseEvent&) override;

    std::unique_ptr<juce::FileChooser> fileChooser;

private:
    void sliderValueChanged (juce::Slider*) override { repaint(); }

    // Which slot does a y-coordinate fall in (sample mode only)?
    int slotFromY (int y) const;

    // Build waveform path from downsampled peak data (also stores peaks for resize).
    void buildPath      (int slot, const juce::AudioBuffer<float>& buffer);
    void rebuildPath    (int slot);  // rebuild from stored peaks after resize

    juce::Slider positionSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> posAttachment;

    // Stored peak data for rebuilding on resize (one float per pixel column)
    std::array<std::vector<float>, 4> peakData;
    std::array<juce::Path, 4>         waveformPaths;
    std::array<bool, 4>               hasContent    { false, false, false, false };
    std::array<juce::String, 4>       rootNoteNames { "C4", "C4", "C4", "C4" };

    int  activeSlot   = -1;
    int  dragOverSlot = -1;
    bool wavetableMode = false;
    juce::Colour accentColour { GladeColors::cyan };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleMapUI)
};
