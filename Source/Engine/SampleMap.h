#pragma once
#include "SampleBuffer.h"
#include "PitchDetector.h"
#include <array>

// Holds up to 4 sample slots. Each slot has its own SampleBuffer and detected root note.
// getBestBuffer(midiNote) returns the slot whose root note is closest to the played note.
// Thread safety: loadFile() is message-thread only. All getters are audio-thread safe.
class SampleMap
{
public:
    static constexpr int kNumSlots = 4;

    SampleMap() = default;

    // Load a file into the given slot. Call from message thread only.
    bool loadFile (int slot, const juce::File& file, double targetSampleRate);

    // Returns buffer of the slot closest in pitch to midiNote.
    // Falls back to any populated slot. Returns empty buffer if nothing loaded.
    const juce::AudioBuffer<float>& getBestBuffer (int midiNote) const;

    // Returns the slot index that getBestBuffer() would pick (-1 if nothing loaded).
    int getActiveSlot (int midiNote) const;

    bool hasAnyContent() const;
    bool slotHasContent (int slot) const;

    int  getRootNote (int slot) const;
    void setRootNote (int slot, int midiNote);

    const juce::AudioBuffer<float>& getBuffer (int slot) const;
    const juce::File&               getFile   (int slot) const;

private:
    struct Slot
    {
        SampleBuffer        buffer;
        std::atomic<int>    rootNote { 60 };
        juce::File          file;

        Slot() = default;
        Slot (const Slot&) = delete;
        Slot& operator= (const Slot&) = delete;
    };

    std::array<Slot, kNumSlots> slots;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleMap)
};
