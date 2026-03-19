#pragma once
#include <JuceHeader.h>
#include <cmath>

/** Quantizes a pitch-jitter value (in semitones) to the nearest note in a
 *  musical scale.  scaleIdx 0 = no quantization (chromatic pass-through). */
namespace ScaleQuantizer
{
    // Scale intervals relative to root (0 = root, in semitones)
    struct ScaleDef { int intervals[7]; int count; };

    inline const ScaleDef& getScale (int scaleIdx)
    {
        static const ScaleDef scales[] =
        {
            { {0},                    1 },  // 0: Chromatic (unused – pass-through)
            { {0, 2, 4, 5, 7, 9,11}, 7 },  // 1: Major
            { {0, 2, 3, 5, 7, 8,10}, 7 },  // 2: Minor
            { {0, 2, 4, 7, 9, 0, 0}, 5 },  // 3: Pent. Major
            { {0, 3, 5, 7,10, 0, 0}, 5 },  // 4: Pent. Minor
            { {0, 2, 4, 6, 8,10, 0}, 6 },  // 5: Whole Tone
            { {0, 2, 3, 5, 7, 9,10}, 7 },  // 6: Dorian
        };
        return scales[juce::jlimit (0, 6, scaleIdx)];
    }

    /** Returns the nearest in-scale semitone offset to `semitones`.
     *  rootNote 0-11 = C .. B (shifts which chromatic notes are "in scale").
     *  scaleIdx 0 → returns semitones unchanged. */
    inline float quantize (float semitones, int scaleIdx, int /*rootNote*/)
    {
        if (scaleIdx == 0) return semitones;

        const auto& sd = getScale (scaleIdx);
        float bestDist = 1e9f;
        float bestSemi = 0.f;

        for (int oct = -2; oct <= 2; ++oct)
        {
            for (int i = 0; i < sd.count; ++i)
            {
                const float c2 = (float)(sd.intervals[i] + oct * 12);
                const float dist = std::abs (semitones - c2);
                if (dist < bestDist) { bestDist = dist; bestSemi = c2; }
            }
        }
        return bestSemi;
    }
}
