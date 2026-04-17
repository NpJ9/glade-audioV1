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
        juce::String category;                  // e.g. "Atmospheric", "Rhythmic"
        std::map<juce::String, float> params;   // paramId → raw value
        bool isUserPreset = false;
    };

    PresetManager();

    int  getNumPresets()    const { return (int) presets.size(); }
    int  getCurrentIndex()  const { return currentIndex; }
    const juce::String& getCurrentName() const { return presets[currentIndex].name; }
    const std::vector<Preset>& getPresets() const { return presets; }

    void loadPreset        (int index, juce::AudioProcessorValueTreeState& apvts);
    void setCurrentIndex   (int index) { currentIndex = juce::jlimit (0, (int)presets.size()-1, index); }
    void setCurrentByName  (const juce::String& name);
    void nextPreset (juce::AudioProcessorValueTreeState& apvts);
    void prevPreset (juce::AudioProcessorValueTreeState& apvts);

    /** Capture current APVTS state, save to disk, and add to the preset list.
     *  Returns true on success.  Call from the message thread only. */
    bool saveUserPreset (const juce::String& name,
                         juce::AudioProcessorValueTreeState& apvts);

    /** Delete a user preset by index.  No-op on factory presets.
     *  Returns true if the preset was deleted. */
    bool deleteUserPreset (int index);

    static juce::File getUserPresetsDir();

private:
    std::vector<Preset> presets;
    int currentIndex = 0;

    static void applyPreset (const Preset& p, juce::AudioProcessorValueTreeState& apvts);

    /** Scan getUserPresetsDir() and append any .glade files to presets[]. */
    void loadUserPresets();

    /** Write a single preset to disk as XML. */
    static void writePresetFile (const Preset& p);
};
