#pragma once
#include "WindowFunction.h"

struct Grain
{
    bool       active           = false;
    double     sourceReadPos    = 0.0;   // current fractional read position in source buffer
    double     pitchRatio       = 1.0;   // source samples advanced per output sample
    int        grainLengthSamples = 0;   // total output duration
    int        samplesPlayed    = 0;     // how many output samples have been produced

    float      panL             = 1.0f;
    float      panR             = 1.0f;
    float      amplitude        = 1.0f;

    WindowType windowType       = WindowType::Hanning;
    bool       isReverse        = false;   // true → advance sourceReadPos by -pitchRatio

    void reset()
    {
        active              = false;
        sourceReadPos       = 0.0;
        pitchRatio          = 1.0;
        grainLengthSamples  = 0;
        samplesPlayed       = 0;
        panL = panR         = 1.0f;
        amplitude           = 1.0f;
        windowType          = WindowType::Hanning;
        isReverse           = false;
    }
};
