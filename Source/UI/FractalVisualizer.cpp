#include "FractalVisualizer.h"
#include <cmath>

//==============================================================================
const FractalVisualizer::Vec3 FractalVisualizer::s_verts[8] =
{
    {-1,-1,-1}, { 1,-1,-1}, { 1, 1,-1}, {-1, 1,-1},   // front face
    {-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1}    // back face
};

const int FractalVisualizer::s_edges[12][2] =
{
    {0,1},{1,2},{2,3},{3,0},    // front face ring
    {4,5},{5,6},{6,7},{7,4},    // back face ring
    {0,4},{1,5},{2,6},{3,7}     // depth connectors
};

// Palette: cyan → green → magenta → purple → yellow (loops)
static const juce::Colour s_palette[5] =
{
    juce::Colour (0xff00eeff),   // cyan
    juce::Colour (0xff00ff66),   // green
    juce::Colour (0xffee00ff),   // magenta
    juce::Colour (0xff9933ff),   // purple
    juce::Colour (0xffffcc00),   // yellow
};

//==============================================================================
FractalVisualizer::FractalVisualizer()
{
    setOpaque (false);
}

//==============================================================================
juce::Colour FractalVisualizer::getPaletteColour (float offset) const
{
    const float t   = std::fmod ((float) colourPhase_ + offset, 1.f);
    const float pos = t * 5.f;
    const int   idx = (int) pos % 5;
    const float mix = pos - (float) (int) pos;
    return s_palette[idx].interpolatedWith (s_palette[(idx + 1) % 5], mix);
}

//==============================================================================
void FractalVisualizer::update (float grainDensity, float grainSize,
                                 bool isActive, float rmsLevel, float audioFreqHz)
{
    density_     = grainDensity;
    grainSize_   = grainSize;
    active_      = isActive;
    rmsLevel_    = juce::jlimit (0.f, 1.f, rmsLevel * 3.f);  // boost for visibility
    audioFreqHz_ = audioFreqHz;

    // Rotation speed scales with density + RMS kick
    const double normDensity = (double) (grainDensity - 1.f) / 199.0;
    const double rotSpeed    = 0.004 + normDensity * 0.038 + (double) rmsLevel * 0.025;
    rotY += rotSpeed;
    rotX  = 0.28 + std::sin (rotY * 0.27) * (0.22 + (double) rmsLevel * 0.4);

    // Pulse rate scales inversely with grain size
    const double normSize  = (double) (grainSize - 5.f) / 495.0;
    const double pulseSpd  = 0.05 + (1.0 - normSize) * 0.08;
    pulsePhase = std::fmod (pulsePhase + pulseSpd, juce::MathConstants<double>::twoPi);

    // Oscilloscope phase: advance proportional to audio frequency
    const double freqNorm  = (double) juce::jlimit (0.f, 2000.f, audioFreqHz) / 2000.0;
    const double waveSpeed = 0.04 + freqNorm * 0.18;
    oscPhase_ = std::fmod (oscPhase_ + waveSpeed, juce::MathConstants<double>::twoPi);

    // Colour cycling: drift + RMS burst
    const double rmsBoost = (double) rmsLevel * 0.018;
    colourPhase_ = std::fmod (colourPhase_ + (active_ ? 0.005 : 0.002) + rmsBoost, 1.0);

    if (glitchFrames > 0)
    {
        --glitchFrames;
        glitchAmt = (float) glitchFrames * 0.38f;
        // Glitch accelerates colour cycling
        colourPhase_ = std::fmod (colourPhase_ + 0.04, 1.0);
    }
    else
    {
        glitchAmt = 0.f;
    }

    repaint();
}

void FractalVisualizer::triggerGlitch()
{
    glitchFrames = 14;
    glitchAmt    = 5.f;
    repaint();
}

//==============================================================================
FractalVisualizer::Vec3 FractalVisualizer::applyRotation (Vec3 v) const
{
    const float cosY = (float) std::cos (rotY);
    const float sinY = (float) std::sin (rotY);
    Vec3 r = { v.x * cosY + v.z * sinY, v.y, -v.x * sinY + v.z * cosY };

    const float cosX = (float) std::cos (rotX);
    const float sinX = (float) std::sin (rotX);
    return { r.x, r.y * cosX - r.z * sinX, r.y * sinX + r.z * cosX };
}

juce::Point<float> FractalVisualizer::project (Vec3 v, float cx, float cy, float sz) const
{
    const float fov   = 3.8f;
    const float persp = fov / (fov + v.z * 0.55f);
    return { cx + v.x * persp * sz, cy + v.y * persp * sz };
}

FractalVisualizer::Vec3 FractalVisualizer::applyWobble (Vec3 v, int vertexIdx) const
{
    if (rmsLevel_ < 0.01f) return v;

    // Each vertex gets a unique phase offset so the shape undulates like a wave
    const float phaseOffset = (float) vertexIdx * juce::MathConstants<float>::pi * 0.25f;
    const float freqScale   = juce::jlimit (0.f, 1.f, audioFreqHz_ / 1200.f);
    // More extreme amplitude: up to 1.5 units at full RMS (shape explodes outward)
    const float amplitude   = rmsLevel_ * rmsLevel_ * (1.2f + freqScale * 1.8f);
    const float phase       = (float) oscPhase_ + phaseOffset;

    // Three overlapping Lissajous components — creates wild organic deformation
    v.y += amplitude * std::sin (phase);
    v.x += amplitude * 0.8f  * std::cos (phase * 1.35f);
    v.z += amplitude * 0.6f  * std::sin (phase * 0.7f + 1.f);
    // Extra high-freq ripple when audio frequency is high
    v.y += amplitude * freqScale * 0.5f * std::sin (phase * 3.1f + (float) vertexIdx);
    v.x += amplitude * freqScale * 0.3f * std::cos (phase * 2.7f);

    return v;
}

void FractalVisualizer::drawCubeLayer (juce::Graphics& g,
                                        float cx, float cy,
                                        float cubeRadius,
                                        float offX, float offY,
                                        juce::Colour colour,
                                        float alpha) const
{
    g.setColour (colour.withAlpha (juce::jlimit (0.f, 1.f, alpha)));

    for (auto& e : s_edges)
    {
        // Apply oscillographic wobble per vertex BEFORE rotation
        const Vec3 va = applyRotation (applyWobble (s_verts[e[0]], e[0]));
        const Vec3 vb = applyRotation (applyWobble (s_verts[e[1]], e[1]));

        auto pa = project (va, cx, cy, cubeRadius);
        auto pb = project (vb, cx, cy, cubeRadius);

        float gx1 = 0.f, gy1 = 0.f, gx2 = 0.f, gy2 = 0.f;
        if (glitchAmt > 0.f)
        {
            gx1 = (rng.nextFloat() - 0.5f) * glitchAmt * 2.5f;
            gy1 = (rng.nextFloat() - 0.5f) * glitchAmt * 0.6f;
            gx2 = (rng.nextFloat() - 0.5f) * glitchAmt * 2.5f;
            gy2 = (rng.nextFloat() - 0.5f) * glitchAmt * 0.6f;
        }

        g.drawLine (pa.x + offX + gx1, pa.y + offY + gy1,
                    pb.x + offX + gx2, pb.y + offY + gy2, 1.1f);
    }
}

//==============================================================================
void FractalVisualizer::paint (juce::Graphics& g)
{
    const auto  bounds = getLocalBounds().toFloat();
    const float cx     = bounds.getCentreX();
    const float cy     = bounds.getCentreY();

    // ── Dynamic palette colours ───────────────────────────────────────────────
    const juce::Colour accent   = getPaletteColour (0.f);       // primary
    const juce::Colour aberr1   = getPaletteColour (0.15f);     // first aberration offset
    const juce::Colour aberr2   = getPaletteColour (0.40f);     // second aberration offset
    const juce::Colour aberr3   = getPaletteColour (0.65f);     // third aberration offset

    // ── Background ────────────────────────────────────────────────────────────
    g.setColour (juce::Colour (0xff090e1a));
    g.fillRoundedRectangle (bounds, 4.f);

    // ── Central glow — brightens dramatically with RMS ────────────────────────
    {
        const float r     = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.42f;
        const float alpha = 0.06f + rmsLevel_ * 0.22f;
        juce::ColourGradient grad (accent.withAlpha (alpha),   cx, cy,
                                   juce::Colour (0x00000000),  cx + r, cy, true);
        g.setGradientFill (grad);
        g.fillEllipse (cx - r, cy - r, r * 2.f, r * 2.f);
    }

    // ── Pulse scale — exaggerated with RMS ───────────────────────────────────
    const float pulse = 1.f + 0.12f * (float) std::sin (pulsePhase);
    // RMS adds a hard beat — cube can swell up to 1.4× at peak
    const float rmsPulse = 1.f + rmsLevel_ * 0.4f;
    const float baseR    = juce::jmin (bounds.getWidth(), bounds.getHeight())
                           * 0.29f * pulse * rmsPulse;

    // ── Active note glow — swells with RMS ───────────────────────────────────
    if (active_)
    {
        const float gr    = baseR * (1.8f + rmsLevel_ * 1.2f);
        const float alpha = 0.12f + rmsLevel_ * 0.28f;
        juce::ColourGradient grad (accent.withAlpha (alpha), cx, cy,
                                   juce::Colour (0x00000000), cx + gr, cy, true);
        g.setGradientFill (grad);
        g.fillEllipse (cx - gr, cy - gr, gr * 2.f, gr * 2.f);
    }

    // ── Three nested cubes — chromatic aberration uses dynamic palette ─────────
    const float layerScales[3] = { baseR, baseR * 0.555f, baseR * 0.308f };
    // Alpha surges with RMS so layers become brighter at high volume
    const float layerAlphas[3] = { 0.90f + rmsLevel_ * 0.1f,
                                    0.70f + rmsLevel_ * 0.2f,
                                    0.50f + rmsLevel_ * 0.3f };

    for (int ci = 0; ci < 3; ++ci)
    {
        const float sz = layerScales[ci];
        const float al = layerAlphas[ci] * (active_ ? 1.f : 0.45f);
        const float ab = 2.8f - (float) ci * 0.5f;

        // Chromatic aberration channels — each uses a different palette offset
        drawCubeLayer (g, cx, cy, sz,  -ab, -ab * 0.45f, aberr1, al * 0.42f);
        drawCubeLayer (g, cx, cy, sz,   ab,  ab * 0.45f, aberr2, al * 0.42f);
        drawCubeLayer (g, cx, cy, sz,   0.f, ab * 0.8f,  aberr3, al * 0.34f);

        // Main centred draw — dynamic accent, slightly brightened
        drawCubeLayer (g, cx, cy, sz,   0.f, 0.f,
                       accent.brighter (0.3f), al * 0.92f);
    }

    // ── Corner label ─────────────────────────────────────────────────────────
    g.setColour (juce::Colour (0xff2a3a4a));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (8.f)));
    g.drawText ("GRAIN CLOUD", bounds.reduced (6.f, 4.f),
                juce::Justification::topLeft);
}
