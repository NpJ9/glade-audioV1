#include "SampleMap.h"
#include <climits>

bool SampleMap::loadFile (int slot, const juce::File& file, double targetSampleRate)
{
    if (slot < 0 || slot >= kNumSlots) return false;

    const bool ok = slots[slot].buffer.loadFile (file, targetSampleRate);
    if (ok)
    {
        slots[slot].file = file;
        const int detected = PitchDetector::detectMidiNote (
            slots[slot].buffer.getBuffer(), targetSampleRate);
        slots[slot].rootNote.store (detected >= 0 ? detected : 60);
    }
    return ok;
}

int SampleMap::getActiveSlot (int midiNote) const
{
    int best     = -1;
    int bestDist = INT_MAX;

    for (int i = 0; i < kNumSlots; ++i)
    {
        if (!slots[i].buffer.hasContent()) continue;
        const int dist = std::abs (midiNote - slots[i].rootNote.load());
        if (dist < bestDist)
        {
            bestDist = dist;
            best     = i;
        }
    }
    return best;
}

const juce::AudioBuffer<float>& SampleMap::getBestBuffer (int midiNote) const
{
    const int slot = getActiveSlot (midiNote);
    if (slot >= 0) return slots[slot].buffer.getBuffer();

    static const juce::AudioBuffer<float> empty;
    return empty;
}

bool SampleMap::hasAnyContent() const
{
    for (int i = 0; i < kNumSlots; ++i)
        if (slots[i].buffer.hasContent()) return true;
    return false;
}

bool SampleMap::slotHasContent (int slot) const
{
    return slot >= 0 && slot < kNumSlots && slots[slot].buffer.hasContent();
}

int SampleMap::getRootNote (int slot) const
{
    if (slot < 0 || slot >= kNumSlots) return 60;
    return slots[slot].rootNote.load();
}

void SampleMap::setRootNote (int slot, int midiNote)
{
    if (slot >= 0 && slot < kNumSlots)
        slots[slot].rootNote.store (midiNote);
}

const juce::AudioBuffer<float>& SampleMap::getBuffer (int slot) const
{
    static const juce::AudioBuffer<float> empty;
    if (slot < 0 || slot >= kNumSlots) return empty;
    return slots[slot].buffer.getBuffer();
}

const juce::File& SampleMap::getFile (int slot) const
{
    static const juce::File empty;
    if (slot < 0 || slot >= kNumSlots) return empty;
    return slots[slot].file;
}
