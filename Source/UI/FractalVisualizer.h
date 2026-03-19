#pragma once
#include <JuceHeader.h>

/** Rotating wireframe cube (3 nested scales = Menger-sponge feel)
 *  with chromatic-aberration rendering, oscillographic audio wave,
 *  and dynamic palette colour cycling.
 *  Driven by grain density (rotation speed), grain size (pulse rate),
 *  output RMS (wave amplitude) and audio frequency (wave frequency).
 *  Call triggerGlitch() when the user hits a randomise button. */
class FractalVisualizer : public juce::Component
{
public:
    FractalVisualizer();

    void paint (juce::Graphics&) override;

    /** Call at 30 Hz from the editor's timerCallback. */
    void update (float grainDensity,   // 1 – 200
                 float grainSize,      // 5 – 500 ms
                 bool  isActive,       // true = MIDI note held
                 float rmsLevel,       // 0 – 1  (output RMS)
                 float audioFreqHz);   // Hz of current note (0 = silent)

    void triggerGlitch();

private:
    struct Vec3 { float x, y, z; };

    // Unit-cube vertices and edges
    static const Vec3 s_verts[8];
    static const int  s_edges[12][2];

    double rotY       = 0.0;
    double rotX       = 0.18;
    double pulsePhase = 0.0;

    float  density_      = 30.f;
    float  grainSize_    = 80.f;
    bool   active_       = false;
    float  rmsLevel_     = 0.f;
    float  audioFreqHz_  = 0.f;

    // Oscillographic wave state
    double oscPhase_     = 0.0;

    // Colour cycling through palette
    double colourPhase_  = 0.0;

    int   glitchFrames = 0;
    float glitchAmt    = 0.f;

    mutable juce::Random rng;   // mutable so paint() can use it

    Vec3               applyRotation (Vec3 v) const;
    juce::Point<float> project       (Vec3 v, float cx, float cy, float sz) const;

    // Applies oscillographic vertex displacement before rotation
    Vec3 applyWobble (Vec3 v, int vertexIdx) const;

    void drawCubeLayer (juce::Graphics& g,
                        float cx, float cy,
                        float cubeRadius,
                        float offX, float offY,
                        juce::Colour colour,
                        float alpha) const;

    // Returns interpolated palette colour for the current colourPhase
    juce::Colour getPaletteColour (float offset = 0.f) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FractalVisualizer)
};
