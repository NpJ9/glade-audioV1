#pragma once
#include <JuceHeader.h>
#include "GladeLookAndFeel.h"

// Displays a loaded audio sample waveform with a draggable position marker.
// Accepts file drag-and-drop to load new samples.
class WaveformDisplay : public juce::Component,
                        public juce::FileDragAndDropTarget,
                        private juce::Slider::Listener
{
public:
    // Called when user drops a file or the load button fires.
    // Returns true if load succeeded.
    std::function<bool(juce::File)> onFileLoad;

    explicit WaveformDisplay (juce::AudioProcessorValueTreeState& apvts);
    ~WaveformDisplay() override;

    // Call after a sample loads to rebuild the waveform path.
    void setSampleBuffer (const juce::AudioBuffer<float>& buffer);

    // Update the root note label shown in the bottom-right corner (e.g. "C4").
    void setRootNote (const juce::String& name)  { rootNoteName = name; repaint(); }

    // Switch to wavetable display mode (pass nullptr to clear).
    void setWavetableMode (bool on, const juce::AudioBuffer<float>* wavetable);

    void paint   (juce::Graphics&) override;
    void resized () override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

    // Click to open file browser
    void mouseDown (const juce::MouseEvent&) override;

    // Keep FileChooser alive for async operation
    std::unique_ptr<juce::FileChooser> fileChooser;

private:
    void sliderValueChanged (juce::Slider*) override {}   // triggers repaint via attachment

    juce::Slider positionSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> posAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> jitterAttachment;

    juce::Path   waveformPath;
    bool         hasWaveform  = false;
    bool         isDragOver   = false;
    juce::String rootNoteName;

    juce::Colour accentColour { GladeColors::cyan };
    bool         wavetableMode = false;

    void buildWaveformPath (const juce::AudioBuffer<float>& buffer);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformDisplay)
};
