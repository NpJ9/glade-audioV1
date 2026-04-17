#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Engine/StepSequencer.h"

//==============================================================================
GladeAudioProcessor::GladeAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    fxChain = std::make_unique<FXChain>();

    // Listen for FX type changes on the message thread so we can create new effect
    // instances there (safe to allocate) and hand them to the audio thread lock-free.
    for (int i = 0; i < FXChain::numSlots; ++i)
        apvts.addParameterListener ("fxType" + juce::String (i), this);
}

GladeAudioProcessor::~GladeAudioProcessor()
{
    for (int i = 0; i < FXChain::numSlots; ++i)
        apvts.removeParameterListener ("fxType" + juce::String (i), this);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
GladeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // ── Grain controls ────────────────────────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "grainSize",    "Grain Size",    juce::NormalisableRange<float>(5.f, 500.f, 0.1f, 0.4f), 80.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "grainDensity", "Grain Density", juce::NormalisableRange<float>(1.f, 200.f, 0.1f, 0.5f), 30.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "grainPosition","Grain Position",juce::NormalisableRange<float>(0.f, 1.f),  0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "posJitter",    "Pos Jitter",    juce::NormalisableRange<float>(0.f, 1.f),  0.f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (
        "beatSync",     "Beat Sync",     false));
    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        "beatDivision", "Beat Division",
        juce::StringArray{"1/32","1/16T","1/16","1/8T","1/8","1/4T","1/4","1/2","1 bar"}, 4));

    // ── Pitch controls ────────────────────────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "pitchShift",   "Pitch Shift",   juce::NormalisableRange<float>(-24.f, 24.f, 0.01f), 0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "pitchJitter",  "Pitch Jitter",  juce::NormalisableRange<float>(0.f, 1.f), 0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "panSpread",    "Pan Spread",    juce::NormalisableRange<float>(0.f, 1.f), 0.f));
    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        "windowType",   "Window Type",
        juce::StringArray{"Gaussian","Hanning","Trapezoid","Rectangular"}, 0));

    // ── Harmonic scale quantizer ───────────────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        "pitchScale",   "Pitch Scale",
        juce::StringArray{"Chromatic","Major","Minor","Pent. Maj","Pent. Min","Whole Tone","Dorian"}, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        "pitchRoot",    "Root Note",
        juce::StringArray{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}, 0));

    // ── Envelope (SYNTH mode) ─────────────────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "attack",       "Attack",        juce::NormalisableRange<float>(0.001f, 10.f, 0.001f, 0.3f), 0.05f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "decay",        "Decay",         juce::NormalisableRange<float>(0.001f, 10.f, 0.001f, 0.3f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "sustain",      "Sustain",       juce::NormalisableRange<float>(0.f, 1.f), 0.3f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "release",      "Release",       juce::NormalisableRange<float>(0.001f, 20.f, 0.001f, 0.3f), 0.8f));

    // ── Output ────────────────────────────────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "outputGain",   "Output Gain",   juce::NormalisableRange<float>(-24.f, 12.f, 0.1f), 0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "dryWet",       "Dry/Wet",       juce::NormalisableRange<float>(0.f, 1.f), 1.f));

    // ── LFO ───────────────────────────────────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "lfoRate",      "LFO Rate",      juce::NormalisableRange<float>(0.01f, 20.f, 0.01f, 0.4f), 1.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "lfoDepth",     "LFO Depth",     juce::NormalisableRange<float>(0.f, 1.f), 0.f));
    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        "lfoShape",     "LFO Shape",
        juce::StringArray{"Sine","Triangle","Saw","Square","S&H","Ramp↑","Ramp↓"}, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        "lfoTarget",    "LFO Target",
        juce::StringArray{"None","Grain Size","Density","Position","Pitch Shift","Pan Spread"}, 0));

    // ── LFO 2 ─────────────────────────────────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "lfoRate2",     "LFO 2 Rate",    juce::NormalisableRange<float>(0.01f, 20.f, 0.01f, 0.4f), 1.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "lfoDepth2",    "LFO 2 Depth",   juce::NormalisableRange<float>(0.f, 1.f), 0.f));
    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        "lfoShape2",    "LFO 2 Shape",
        juce::StringArray{"Sine","Triangle","Saw","Square","S&H","Ramp↑","Ramp↓"}, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        "lfoTarget2",   "LFO 2 Target",
        juce::StringArray{"None","Grain Size","Density","Position","Pitch Shift","Pan Spread"}, 0));

    // ── LFO 3 ─────────────────────────────────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "lfoRate3",     "LFO 3 Rate",    juce::NormalisableRange<float>(0.01f, 20.f, 0.01f, 0.4f), 1.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "lfoDepth3",    "LFO 3 Depth",   juce::NormalisableRange<float>(0.f, 1.f), 0.f));
    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        "lfoShape3",    "LFO 3 Shape",
        juce::StringArray{"Sine","Triangle","Saw","Square","S&H","Ramp↑","Ramp↓"}, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        "lfoTarget3",   "LFO 3 Target",
        juce::StringArray{"None","Grain Size","Density","Position","Pitch Shift","Pan Spread"}, 0));

    // ── Step Sequencer ────────────────────────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "seqActive", "Seq Active", false));
    for (int i = 0; i < StepSequencer::kNumSteps; ++i)
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            "seqStep" + juce::String(i), "Seq Step " + juce::String(i),
            juce::NormalisableRange<float>(0.f, 1.f), 0.5f));

    // ── Performance features ──────────────────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "reverseAmount", "Reverse",
        juce::NormalisableRange<float>(0.f, 1.f), 0.f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (
        "freeze",        "Freeze",   false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "velocityDepth", "Velocity Depth",
        juce::NormalisableRange<float>(0.f, 1.f), 0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "glideTime",     "Glide",
        juce::NormalisableRange<float>(0.f, 5.f, 0.001f, 0.3f), 0.f));

    // ── Macro knobs M1–M4 ─────────────────────────────────────────────────────
    juce::StringArray macroTargets {"None","Grain Size","Density","Position","Pitch Shift","Pan Spread"};
    for (int i = 1; i <= 4; ++i)
    {
        const auto s = juce::String(i);
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            "m" + s, "Macro " + s,
            juce::NormalisableRange<float>(0.f, 1.f), 0.5f));
        // Target 1 (existing)
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            "m" + s + "Target", "Macro " + s + " Target", macroTargets, 0));
        // Targets 2–4 (new — each macro can modulate up to 4 parameters)
        for (int t = 2; t <= 4; ++t)
            params.push_back (std::make_unique<juce::AudioParameterChoice> (
                "m" + s + "Target" + juce::String(t),
                "Macro " + s + " Target " + juce::String(t),
                macroTargets, 0));
    }

    // ── FX Chain slots (4 slots, each has type + 4 params + bypass) ──────────
    juce::StringArray fxTypes {"None","Reverb","Delay","Chorus","Distortion","Filter","Shimmer","L.Chorus","Flanger","Harmonic","AutoPan"};
    for (int i = 0; i < 4; ++i)
    {
        auto s = juce::String(i);
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            "fxType"   + s, "FX " + s + " Type",    fxTypes, 0));
        params.push_back (std::make_unique<juce::AudioParameterBool>  (
            "fxBypass" + s, "FX " + s + " Bypass",  false));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            "fxP1"     + s, "FX " + s + " P1",      juce::NormalisableRange<float>(0.f, 1.f), 0.5f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            "fxP2"     + s, "FX " + s + " P2",      juce::NormalisableRange<float>(0.f, 1.f), 0.5f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            "fxP3"     + s, "FX " + s + " P3",      juce::NormalisableRange<float>(0.f, 1.f), 0.5f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            "fxMix"    + s, "FX " + s + " Mix",     juce::NormalisableRange<float>(0.f, 1.f), 1.f));
    }

    return { params.begin(), params.end() };
}

//==============================================================================
void GladeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    granularEngine.prepare (sampleRate, samplesPerBlock);
    fxChain->prepare (sampleRate, samplesPerBlock);
    // Pre-allocate to the declared max so processBlock never calls setSize on the audio thread.
    dryBuffer.setSize (2, samplesPerBlock, false, true, false);

    dryWetSmoothed.reset (sampleRate, 0.05);
    dryWetSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue ("dryWet")->load());
}

void GladeAudioProcessor::releaseResources()
{
    granularEngine.releaseResources();
    fxChain->reset();
    dryBuffer.setSize (0, 0);
}

//==============================================================================
GrainParams GladeAudioProcessor::buildGrainParams() const
{
    // All APVTS reads are concentrated here.  The engine receives a plain struct
    // and has no dependency on juce::AudioProcessorValueTreeState.
    auto getF = [this] (const char* id) -> float
    {
        return apvts.getRawParameterValue (id)->load();
    };

    GrainParams p;

    p.attack   = getF ("attack");
    p.decay    = getF ("decay");
    p.sustain  = getF ("sustain");
    p.release  = getF ("release");

    p.grainSizeMs = getF ("grainSize");
    p.density     = getF ("grainDensity");
    p.position    = getF ("grainPosition");
    p.posJitter   = getF ("posJitter");
    p.pitchShift  = getF ("pitchShift");
    p.pitchJitter = getF ("pitchJitter");
    p.panSpread   = getF ("panSpread");
    p.windowType  = (int) getF ("windowType");
    p.pitchScale  = (int) getF ("pitchScale");
    p.pitchRoot   = (int) getF ("pitchRoot");
    p.outputGainDb = getF ("outputGain");

    p.lfoRate    = getF ("lfoRate");
    p.lfoDepth   = getF ("lfoDepth");
    p.lfoShape   = (int) getF ("lfoShape");
    p.lfoTarget  = (int) getF ("lfoTarget");
    p.lfoRate2    = getF ("lfoRate2");
    p.lfoDepth2   = getF ("lfoDepth2");
    p.lfoShape2   = (int) getF ("lfoShape2");
    p.lfoTarget2  = (int) getF ("lfoTarget2");

    p.lfoRate3    = getF ("lfoRate3");
    p.lfoDepth3   = getF ("lfoDepth3");
    p.lfoShape3   = (int) getF ("lfoShape3");
    p.lfoTarget3  = (int) getF ("lfoTarget3");

    p.seqActive    = getF ("seqActive") > 0.5f;
    p.beatDivision = (int) getF ("beatDivision");
    p.beatSync     = getF ("beatSync") > 0.5f;

    // Static ID array — avoids juce::String allocation on the audio thread
    static const char* stepIds[] = {
        "seqStep0","seqStep1","seqStep2","seqStep3",
        "seqStep4","seqStep5","seqStep6","seqStep7"
    };
    static_assert (std::size (stepIds) == Glade::kSeqNumSteps, "stepIds size mismatch");

    for (int i = 0; i < Glade::kSeqNumSteps; ++i)
        p.seqSteps[i] = apvts.getRawParameterValue (stepIds[i])->load();

    p.bpm = currentBpm;

    // ── Performance features ──────────────────────────────────────────────────
    p.reverseAmount = getF ("reverseAmount");
    p.freeze        = getF ("freeze") > 0.5f;
    p.velocityDepth = getF ("velocityDepth");
    p.glideTime     = getF ("glideTime");

    // ── Macro knobs M1–M4 ─────────────────────────────────────────────────────
    p.m1 = getF ("m1");
    p.m1Target  = (int) getF ("m1Target");
    p.m1Target2 = (int) getF ("m1Target2");
    p.m1Target3 = (int) getF ("m1Target3");
    p.m1Target4 = (int) getF ("m1Target4");

    p.m2 = getF ("m2");
    p.m2Target  = (int) getF ("m2Target");
    p.m2Target2 = (int) getF ("m2Target2");
    p.m2Target3 = (int) getF ("m2Target3");
    p.m2Target4 = (int) getF ("m2Target4");

    p.m3 = getF ("m3");
    p.m3Target  = (int) getF ("m3Target");
    p.m3Target2 = (int) getF ("m3Target2");
    p.m3Target3 = (int) getF ("m3Target3");
    p.m3Target4 = (int) getF ("m3Target4");

    p.m4 = getF ("m4");
    p.m4Target  = (int) getF ("m4Target");
    p.m4Target2 = (int) getF ("m4Target2");
    p.m4Target3 = (int) getF ("m4Target3");
    p.m4Target4 = (int) getF ("m4Target4");

    return p;
}

std::array<FXSlotParams, FXChain::numSlots>
GladeAudioProcessor::buildFXParams() const
{
    // Pre-built param ID strings — avoids juce::String construction on audio thread.
    static const char* typeIds[]   = { "fxType0",   "fxType1",   "fxType2",   "fxType3"   };
    static const char* bypassIds[] = { "fxBypass0", "fxBypass1", "fxBypass2", "fxBypass3" };
    static const char* p1Ids[]     = { "fxP10",     "fxP11",     "fxP12",     "fxP13"     };
    static const char* p2Ids[]     = { "fxP20",     "fxP21",     "fxP22",     "fxP23"     };
    static const char* p3Ids[]     = { "fxP30",     "fxP31",     "fxP32",     "fxP33"     };
    static const char* mixIds[]    = { "fxMix0",    "fxMix1",    "fxMix2",    "fxMix3"    };

    auto getF = [this] (const char* id) -> float
    {
        return apvts.getRawParameterValue (id)->load();
    };

    std::array<FXSlotParams, FXChain::numSlots> params;
    for (int i = 0; i < FXChain::numSlots; ++i)
    {
        params[i].type   = static_cast<FXType> ((int) getF (typeIds[i]));
        params[i].bypass = getF (bypassIds[i]) > 0.5f;
        params[i].p1     = getF (p1Ids[i]);
        params[i].p2     = getF (p2Ids[i]);
        params[i].p3     = getF (p3Ids[i]);
        params[i].mix    = getF (mixIds[i]);
    }
    return params;
}

//==============================================================================
void GladeAudioProcessor::parameterChanged (const juce::String& paramId, float newValue)
{
    // Called on the message thread whenever an fxType parameter changes.
    // We create the new effect here (allocation is fine on the message thread)
    // and enqueue it for the audio thread to install lock-free.
    if (paramId.startsWith ("fxType"))
    {
        const int    slot   = paramId.substring (6).getIntValue();
        const FXType fxType = static_cast<FXType> ((int) newValue);
        fxChain->requestTypeChange (slot, fxType);
    }
}

//==============================================================================
void GladeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // ── Read host BPM ─────────────────────────────────────────────────────────
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                currentBpm = *bpm;

    // ── Build parameter structs from APVTS — single point of APVTS access ────
    const GrainParams grainParams = buildGrainParams();
    const auto        fxParams    = buildFXParams();

    // ── Run granular engine ───────────────────────────────────────────────────
    granularEngine.process (buffer, midiMessages, grainParams);

    // ── Output RMS for visualizer ─────────────────────────────────────────────
    float rmsSum = 0.f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        rmsSum += buffer.getRMSLevel (ch, 0, buffer.getNumSamples());
    outputRms.store (rmsSum / (float) juce::jmax (1, buffer.getNumChannels()));

    // ── Dry/wet: blend pre-FX (dry) and post-FX (wet) ────────────────────────
    dryWetSmoothed.setTargetValue (apvts.getRawParameterValue ("dryWet")->load());

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    // dryBuffer is pre-allocated in prepareToPlay; assert guards against non-conformant hosts.
    jassert (numSamples  <= dryBuffer.getNumSamples());
    jassert (numChannels <= dryBuffer.getNumChannels());
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    fxChain->process (buffer, fxParams, currentBpm);

    // Per-sample smoothed blend eliminates zipper noise on dryWet automation
    for (int s = 0; s < numSamples; ++s)
    {
        const float w = dryWetSmoothed.getNextValue();
        const float d = 1.f - w;
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample (ch, s,   buffer.getSample  (ch, s) * w
                                     + dryBuffer.getSample (ch, s) * d);
    }
}

//==============================================================================
PluginState GladeAudioProcessor::getState() const noexcept
{
    PluginState s;
    s.lfoPhase[0]  = granularEngine.getLfoPhase();
    s.lfoPhase[1]  = granularEngine.getLfoPhase2();
    s.lfoPhase[2]  = granularEngine.getLfoPhase3();
    s.lfoOutput[0] = granularEngine.getLfoOutput();
    s.lfoOutput[1] = granularEngine.getLfoOutput2();
    s.lfoOutput[2] = granularEngine.getLfoOutput3();

    s.adsrCursor        = granularEngine.getAdsrCursor();
    s.modulatedPosition = granularEngine.getModulatedPosition();
    s.currentMidiNote   = granularEngine.getCurrentMidiNote();
    s.detectedRootNote  = granularEngine.getDetectedRootNote();
    s.seqCurrentStep    = granularEngine.getSeqCurrentStep();
    s.activeGrainCount  = granularEngine.getActiveGrainCount();
    s.outputRms         = outputRms.load();
    s.sampleLoaded      = granularEngine.isReady();
    return s;
}

bool GladeAudioProcessor::loadSample (const juce::File& file)
{
    // Reject files larger than 512 MB before attempting to load them.
    // A 512 MB stereo float32 buffer is ~32M samples — far beyond any practical
    // granular use.  Preventing the load avoids an OOM crash on low-memory systems.
    static constexpr juce::int64 kMaxBytes = 512LL * 1024 * 1024;
    if (file.getSize() > kMaxBytes)
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "File Too Large",
            "The selected file is larger than 512 MB and cannot be loaded.\n"
            "Please use a shorter or lower-resolution sample.");
        return false;
    }

    const bool ok = granularEngine.loadSample (file);
    if (ok) lastLoadedFile = file;
    return ok;
}

juce::String GladeAudioProcessor::getRootNoteName() const
{
    return PitchDetector::midiNoteToName (granularEngine.getDetectedRootNote());
}

//==============================================================================
void GladeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (granularEngine.isReady())
        state.setProperty ("samplePath", lastLoadedFile.getFullPathName(), nullptr);
    state.setProperty ("presetName", currentPresetName, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void GladeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Some DAWs call setStateInformation on the audio thread.  We must not
    // perform file I/O, allocate large buffers, or call apvts.replaceState()
    // (which fires parameterChanged → requestTypeChange → allocation) on the
    // audio thread.  Defer everything to the message thread via callAsync.
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    if (!xml->hasTagName (apvts.state.getType())) return;

    // Copy the XmlElement so the shared_ptr owns it safely in the lambda.
    auto xmlOwned = std::make_shared<juce::XmlElement> (*xml);

    // Capture a WeakReference so the lambda is safe if the plugin is destroyed
    // before the message thread processes it (e.g. rapid DAW load/unload).
    juce::WeakReference<GladeAudioProcessor> weakThis (this);

    juce::MessageManager::callAsync ([weakThis, xmlOwned]
    {
        // If the processor was destroyed while this was queued, bail out.
        auto* self = weakThis.get();
        if (self == nullptr) return;

        auto state = juce::ValueTree::fromXml (*xmlOwned);

        // replaceState fires parameterChanged for every parameter — fxType changes
        // arrive on the message thread and call requestTypeChange(), which is safe.
        self->apvts.replaceState (state);

        if (state.hasProperty ("presetName"))
            self->currentPresetName = state.getProperty ("presetName").toString();

        if (state.hasProperty ("samplePath"))
        {
            const juce::File f (state.getProperty ("samplePath").toString());
            if (f.existsAsFile())
                self->loadSample (f);   // loadSample is message-thread-only; correct here
        }
    });
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GladeAudioProcessor();
}
