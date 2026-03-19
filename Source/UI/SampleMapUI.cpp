#include "SampleMapUI.h"

SampleMapUI::SampleMapUI (juce::AudioProcessorValueTreeState& apvts)
{
    positionSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    positionSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    positionSlider.setAlpha (0.f);
    positionSlider.addListener (this);
    addAndMakeVisible (positionSlider);

    posAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "grainPosition", positionSlider);
}

SampleMapUI::~SampleMapUI()
{
    positionSlider.removeListener (this);
}

//==============================================================================
void SampleMapUI::setSlotBuffer (int slot, const juce::AudioBuffer<float>& buffer)
{
    if (slot < 0 || slot >= 4) return;
    buildPath (slot, buffer);
    hasContent[slot] = true;
    repaint();
}

void SampleMapUI::setActiveSlot (int slot)
{
    if (activeSlot != slot)
    {
        activeSlot = slot;
        repaint();
    }
}

void SampleMapUI::setSlotRootNote (int slot, const juce::String& name)
{
    if (slot >= 0 && slot < 4)
    {
        rootNoteNames[slot] = name;
        repaint();
    }
}

void SampleMapUI::setWavetableMode (bool on, const juce::AudioBuffer<float>* wavetable)
{
    wavetableMode = on;
    accentColour  = on ? GladeColors::green : GladeColors::cyan;

    if (on && wavetable != nullptr)
    {
        buildPath (0, *wavetable);
        hasContent[0] = true;
    }
    repaint();
}

//==============================================================================
int SampleMapUI::slotFromY (int y) const
{
    if (wavetableMode || getHeight() == 0) return 0;
    const int slotH = getHeight() / 4;
    if (slotH == 0) return 0;
    return juce::jlimit (0, 3, y / slotH);
}

void SampleMapUI::buildPath (int slot, const juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumSamples() == 0)
    {
        peakData[slot].clear();
        waveformPaths[slot].clear();
        return;
    }

    const int   numSamples = buffer.getNumSamples();
    const auto* data       = buffer.getReadPointer (0);
    const int   w          = juce::jmax (1, getWidth());
    const int   step       = juce::jmax (1, numSamples / w);

    peakData[slot].resize ((size_t) w);
    for (int x = 0; x < w; ++x)
    {
        float peak = 0.f;
        const int start = (int) ((float) x / (float) w * (float) numSamples);
        for (int s = start; s < juce::jmin (start + step, numSamples); ++s)
            peak = juce::jmax (peak, std::abs (data[s]));
        peakData[slot][(size_t) x] = peak;
    }

    rebuildPath (slot);
}

void SampleMapUI::rebuildPath (int slot)
{
    waveformPaths[slot].clear();
    const auto& peaks = peakData[slot];
    if (peaks.empty()) return;

    const float w     = (float) getWidth();
    const float fullH = (float) getHeight();
    const float slotH = wavetableMode ? fullH : (fullH / 4.f);
    const float slotY = wavetableMode ? 0.f   : (float) slot * slotH;
    const float mid   = slotY + slotH * 0.5f;

    waveformPaths[slot].startNewSubPath (0.f, mid);
    for (int x = 0; x < (int) peaks.size(); ++x)
        waveformPaths[slot].lineTo ((float) x, mid - peaks[(size_t) x] * slotH * 0.42f);
    for (int x = (int) peaks.size() - 1; x >= 0; --x)
        waveformPaths[slot].lineTo ((float) x, mid + peaks[(size_t) x] * slotH * 0.42f);
    waveformPaths[slot].closeSubPath();

    juce::ignoreUnused (w);
}

//==============================================================================
void SampleMapUI::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (GladeColors::panel);
    g.fillRoundedRectangle (bounds, 4.f);
    g.setColour (GladeColors::border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.f, 1.f);

    // ── Wavetable mode — single waveform ──────────────────────────────────────
    if (wavetableMode)
    {
        if (hasContent[0])
        {
            g.setColour (accentColour.withAlpha (0.25f));
            g.fillPath (waveformPaths[0]);
            g.setColour (accentColour.withAlpha (0.6f));
            g.strokePath (waveformPaths[0], juce::PathStrokeType (1.f));

            const float pos     = (float) positionSlider.getValue();
            const float markerX = pos * bounds.getWidth();
            const float jitterW = bounds.getWidth() * 0.05f;
            g.setColour (accentColour.withAlpha (0.08f));
            g.fillRect (juce::Rectangle<float> (markerX - jitterW, 0.f, jitterW * 2.f, bounds.getHeight()));
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.fillRect (juce::Rectangle<float> (markerX - 1.f, 0.f, 2.f, bounds.getHeight()));
            g.setColour (accentColour);
            g.fillEllipse (markerX - 5.f, bounds.getCentreY() - 5.f, 10.f, 10.f);
        }
        else
        {
            g.setColour (GladeColors::textDim);
            g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.f)));
            g.drawFittedText ("SYNTH MODE", getLocalBounds(), juce::Justification::centred, 1);
        }
        return;
    }

    // ── Sample mode — 4 lanes ─────────────────────────────────────────────────
    const float slotH = bounds.getHeight() / 4.f;
    const float pos   = (float) positionSlider.getValue();

    for (int i = 0; i < 4; ++i)
    {
        const bool isActive    = (i == activeSlot);
        const bool isDragTarget = (i == dragOverSlot);
        const float laneY = bounds.getY() + (float) i * slotH;
        const juce::Rectangle<float> lane (bounds.getX(), laneY, bounds.getWidth(), slotH);

        // Background highlight
        if (isActive)
        {
            g.setColour (accentColour.withAlpha (0.07f));
            g.fillRect (lane);
        }
        if (isDragTarget)
        {
            g.setColour (accentColour.withAlpha (0.15f));
            g.fillRect (lane);
        }

        // Active border
        if (isActive)
        {
            g.setColour (accentColour.withAlpha (0.45f));
            g.drawRect (lane.reduced (0.5f), 1.f);
        }

        // Slot divider
        if (i < 3)
        {
            g.setColour (GladeColors::border);
            g.fillRect (juce::Rectangle<float> (lane.getX() + 4.f,
                                                lane.getBottom() - 0.5f,
                                                lane.getWidth() - 8.f, 1.f));
        }

        // Slot number
        g.setColour (isActive ? accentColour : GladeColors::textDim);
        g.setFont (juce::Font (juce::FontOptions{}.withHeight (9.f).withStyle ("Bold")));
        g.drawText (juce::String (i + 1),
                    lane.reduced (4.f, 0.f).removeFromLeft (12.f).toNearestInt(),
                    juce::Justification::centred);

        if (hasContent[i])
        {
            // Waveform
            g.setColour (accentColour.withAlpha (isActive ? 0.28f : 0.12f));
            g.fillPath (waveformPaths[i]);
            g.setColour (accentColour.withAlpha (isActive ? 0.65f : 0.28f));
            g.strokePath (waveformPaths[i], juce::PathStrokeType (1.f));

            // Root note label
            g.setColour (isActive ? accentColour : GladeColors::textDim);
            g.setFont (juce::Font (juce::FontOptions{}.withHeight (9.f).withStyle ("Bold")));
            g.drawText (rootNoteNames[i],
                        lane.reduced (4.f, 0.f).removeFromRight (24.f).toNearestInt(),
                        juce::Justification::centred);

            // Position marker on active slot only
            if (isActive)
            {
                const float markerX  = pos * bounds.getWidth();
                const float markerTop = lane.getY() + 2.f;
                const float markerH   = slotH - 4.f;
                const float jitterW  = bounds.getWidth() * 0.05f;

                g.setColour (accentColour.withAlpha (0.08f));
                g.fillRect (juce::Rectangle<float> (markerX - jitterW, lane.getY(),
                                                    jitterW * 2.f, slotH));
                g.setColour (juce::Colours::white.withAlpha (0.8f));
                g.fillRect (juce::Rectangle<float> (markerX - 1.f, markerTop, 2.f, markerH));
                g.setColour (accentColour);
                g.fillEllipse (markerX - 4.f, lane.getCentreY() - 4.f, 8.f, 8.f);
            }
        }
        else
        {
            g.setColour (isDragTarget ? accentColour : GladeColors::textDim.withAlpha (0.5f));
            g.setFont (juce::Font (juce::FontOptions{}.withHeight (10.f)));
            auto textArea = lane.reduced (18.f, 2.f).toNearestInt();
            g.drawFittedText (isDragTarget ? "DROP SAMPLE" : "DRAG & DROP / CLICK TO LOAD",
                              textArea, juce::Justification::centred, 1);
        }
    }
}

void SampleMapUI::resized()
{
    positionSlider.setBounds (getLocalBounds());

    // Rebuild waveform paths with new bounds
    for (int i = 0; i < 4; ++i)
        if (!peakData[i].empty())
            rebuildPath (i);
}

//==============================================================================
bool SampleMapUI::isInterestedInFileDrag (const juce::StringArray& files)
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

void SampleMapUI::fileDragEnter (const juce::StringArray&, int, int y)
{
    dragOverSlot = slotFromY (y);
    repaint();
}

void SampleMapUI::fileDragMove (const juce::StringArray&, int, int y)
{
    const int s = slotFromY (y);
    if (s != dragOverSlot) { dragOverSlot = s; repaint(); }
}

void SampleMapUI::fileDragExit (const juce::StringArray&)
{
    dragOverSlot = -1;
    repaint();
}

void SampleMapUI::filesDropped (const juce::StringArray& files, int, int y)
{
    const int slot = slotFromY (y);
    dragOverSlot = -1;
    if (files.size() > 0 && onFileLoad)
        onFileLoad (slot, juce::File (files[0]));
    repaint();
}

void SampleMapUI::mouseDown (const juce::MouseEvent& e)
{
    if (wavetableMode) return;
    const int slot = slotFromY (e.y);

    fileChooser = std::make_unique<juce::FileChooser> (
        "Load Sample for Slot " + juce::String (slot + 1),
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.aiff;*.aif;*.flac");

    const int capturedSlot = slot;
    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, capturedSlot] (const juce::FileChooser& fc)
        {
            if (fc.getResults().size() > 0 && onFileLoad)
                onFileLoad (capturedSlot, fc.getResult());
        });
}
