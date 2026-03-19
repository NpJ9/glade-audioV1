#include "WaveformDisplay.h"

WaveformDisplay::WaveformDisplay (juce::AudioProcessorValueTreeState& apvts)
{
    // Invisible slider drives the grainPosition parameter
    positionSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    positionSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    positionSlider.setAlpha (0.f); // invisible — we draw it ourselves
    positionSlider.addListener (this);
    addAndMakeVisible (positionSlider);

    posAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "grainPosition", positionSlider);
}

WaveformDisplay::~WaveformDisplay()
{
    positionSlider.removeListener (this);
}

//==============================================================================
void WaveformDisplay::setSampleBuffer (const juce::AudioBuffer<float>& buffer)
{
    wavetableMode = false;
    accentColour  = GladeColors::cyan;
    buildWaveformPath (buffer);
    hasWaveform = true;
    repaint();
}

void WaveformDisplay::setWavetableMode (bool on, const juce::AudioBuffer<float>* wavetable)
{
    wavetableMode = on;
    accentColour  = on ? GladeColors::green : GladeColors::cyan;
    if (on && wavetable != nullptr)
    {
        buildWaveformPath (*wavetable);
        hasWaveform = true;
    }
    else if (!on)
    {
        // Returning to sample mode — only show waveform if a sample was loaded
        // (caller is responsible for re-calling setSampleBuffer if needed)
    }
    repaint();
}

void WaveformDisplay::buildWaveformPath (const juce::AudioBuffer<float>& buffer)
{
    waveformPath.clear();
    if (buffer.getNumSamples() == 0) return;

    const float w = (float) getWidth();
    const float h = (float) getHeight();
    const float mid = h * 0.5f;

    const int numSamples = buffer.getNumSamples();
    const auto* data = buffer.getReadPointer (0);
    const int step = juce::jmax (1, numSamples / (int) w);

    waveformPath.startNewSubPath (0.f, mid);

    for (int x = 0; x < (int) w; ++x)
    {
        float peak = 0.f;
        const int start = (int) ((float) x / w * (float) numSamples);
        for (int s = start; s < juce::jmin (start + step, numSamples); ++s)
            peak = juce::jmax (peak, std::abs (data[s]));

        waveformPath.lineTo ((float) x, mid - peak * mid * 0.9f);
    }

    // Mirror for bottom half
    for (int x = (int) w - 1; x >= 0; --x)
    {
        float peak = 0.f;
        const int start = (int) ((float) x / w * (float) numSamples);
        for (int s = start; s < juce::jmin (start + step, numSamples); ++s)
            peak = juce::jmax (peak, std::abs (data[s]));

        waveformPath.lineTo ((float) x, mid + peak * mid * 0.9f);
    }

    waveformPath.closeSubPath();
}

//==============================================================================
void WaveformDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background
    g.setColour (GladeColors::panel);
    g.fillRoundedRectangle (bounds, 4.f);

    g.setColour (GladeColors::border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.f, 1.f);

    if (hasWaveform)
    {
        // Waveform fill
        g.setColour (accentColour.withAlpha (0.25f));
        g.fillPath (waveformPath);

        // Waveform outline
        g.setColour (accentColour.withAlpha (0.6f));
        g.strokePath (waveformPath, juce::PathStrokeType (1.f));

        // ── Position marker (uses LFO-modulated position when available) ─────
        const float pos = (modPos >= 0.f) ? modPos : (float) positionSlider.getValue();
        const float markerX = pos * bounds.getWidth();

        // Jitter range shading (read jitter param directly)
        // (visual guide only — exact value not stored here)
        g.setColour (accentColour.withAlpha (0.08f));
        const float jitterW = bounds.getWidth() * 0.05f;
        g.fillRect (juce::Rectangle<float> (markerX - jitterW, 0.f,
                                             jitterW * 2.f, bounds.getHeight()));

        // Marker line
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.fillRect (juce::Rectangle<float> (markerX - 1.f, 0.f, 2.f, bounds.getHeight()));

        // Handle dot
        g.setColour (accentColour);
        g.fillEllipse (markerX - 5.f, bounds.getHeight() * 0.5f - 5.f, 10.f, 10.f);

        // Root note label — bottom-right corner pill badge
        if (rootNoteName.isNotEmpty())
        {
            g.setFont (juce::Font (juce::FontOptions{}.withHeight (13.f).withStyle ("Bold")));
            const int textW  = (int) g.getCurrentFont().getStringWidthFloat (rootNoteName) + 12;
            const int textH  = 20;
            const int bx     = getWidth()  - textW - 5;
            const int by     = getHeight() - textH - 5;
            juce::Rectangle<float> badge ((float) bx, (float) by, (float) textW, (float) textH);
            g.setColour (GladeColors::background.withAlpha (0.75f));
            g.fillRoundedRectangle (badge, 4.f);
            g.setColour (accentColour);
            g.drawRoundedRectangle (badge.reduced (0.5f), 4.f, 1.f);
            g.setColour (accentColour);
            g.drawText (rootNoteName, badge.toNearestInt(), juce::Justification::centred);
        }
    }
    else
    {
        // Empty state
        if (isDragOver)
        {
            g.setColour (accentColour.withAlpha (0.15f));
            g.fillRoundedRectangle (bounds.reduced (2.f), 4.f);
        }

        g.setColour (isDragOver ? accentColour : GladeColors::textDim);
        g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.f)));
        g.drawFittedText (isDragOver ? "DROP SAMPLE" : "DRAG & DROP  /  CLICK TO LOAD",
                          getLocalBounds(), juce::Justification::centred, 1);
    }
}

void WaveformDisplay::resized()
{
    positionSlider.setBounds (getLocalBounds());
}

//==============================================================================
void WaveformDisplay::mouseDown (const juce::MouseEvent&)
{
    if (wavetableMode) return;   // file picker disabled in synth mode

    fileChooser = std::make_unique<juce::FileChooser> (
        "Load Sample",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.aiff;*.aif;*.flac");

    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            if (fc.getResults().size() > 0 && onFileLoad)
                onFileLoad (fc.getResult());
        });
}

bool WaveformDisplay::isInterestedInFileDrag (const juce::StringArray& files)
{
    if (wavetableMode) return false;
    for (const auto& f : files)
    {
        const juce::String ext = juce::File (f).getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".aiff" || ext == ".aif" || ext == ".flac")
            return true;
    }
    return false;
}

void WaveformDisplay::filesDropped (const juce::StringArray& files, int, int)
{
    isDragOver = false;
    if (files.size() > 0 && onFileLoad)
        onFileLoad (juce::File (files[0]));
    repaint();
}

