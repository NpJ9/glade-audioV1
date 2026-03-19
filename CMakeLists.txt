cmake_minimum_required(VERSION 3.22)

project(Glade VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── JUCE ──────────────────────────────────────────────────────────────────────
add_subdirectory(C:/JUCE glade_juce)

# ── Plugin target ─────────────────────────────────────────────────────────────
juce_add_plugin(Glade
    COMPANY_NAME            "GladeAudio"
    BUNDLE_ID               "com.gladeaudio.glade"
    PLUGIN_MANUFACTURER_CODE GlAd
    PLUGIN_CODE             Gld1
    FORMATS                 VST3 Standalone
    PRODUCT_NAME            "Glade"
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES         "Instrument" "Synth" "Fx"
    COPY_PLUGIN_AFTER_BUILD FALSE
)

# ── Generate JuceHeader.h ─────────────────────────────────────────────────────
juce_generate_juce_header(Glade)

# ── Source files ──────────────────────────────────────────────────────────────
target_sources(Glade PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Engine/SampleBuffer.cpp
    Source/Engine/GrainPool.cpp
    Source/Engine/GrainScheduler.cpp
    Source/Engine/GranularEngine.cpp
    Source/Engine/FXChain.cpp
    Source/Engine/PresetManager.cpp
    Source/UI/GladeLookAndFeel.cpp
    Source/UI/GladeKnob.cpp
    Source/UI/WaveformDisplay.cpp
    Source/UI/FXSlotUI.cpp
    Source/UI/FractalVisualizer.cpp
)

# ── Compile definitions ───────────────────────────────────────────────────────
target_compile_definitions(Glade PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

# ── JUCE modules ──────────────────────────────────────────────────────────────
target_link_libraries(Glade PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_plugin_client
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_graphics
    juce::juce_audio_formats
    juce::juce_audio_devices
    juce::juce_core
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
