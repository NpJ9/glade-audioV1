#pragma once
#include <JuceHeader.h>

/** Simple factory-preset system for Glade.
 *  All UI interaction is on the message thread. */
class PresetManager
{
public:
    struct Preset
    {
        juce::String name;
        std::map<juce::String, float> params;   // paramId → raw value
    };

    PresetManager();

    int  getNumPresets()    const { return (int) presets.size(); }
    int  getCurrentIndex()  const { return currentIndex; }
    const juce::String& getCurrentName() const { return presets[currentIndex].name; }

    void loadPreset (int index, juce::AudioProcessorValueTreeState& apvts);
    void nextPreset (juce::AudioProcessorValueTreeState& apvts);
    void prevPreset (juce::AudioProcessorValueTreeState& apvts);

private:
    std::vector<Preset> presets;
    int currentIndex = 0;

    static void applyPreset (const Preset& p, juce::AudioProcessorValueTreeState& apvts);
};
