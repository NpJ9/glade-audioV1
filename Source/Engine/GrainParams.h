#pragma once
#include "GladeConstants.h"

/** All parameters consumed by GranularEngine::process().
 *
 *  GladeAudioProcessor::processBlock() reads every parameter from APVTS and
 *  fills this struct.  The engine receives a plain value struct — it has no
 *  knowledge of juce::AudioProcessorValueTreeState and can be unit-tested
 *  by constructing a GrainParams directly.
 *
 *  Default values match the APVTS defaults in createParameterLayout(). */
struct GrainParams
{
    // ── ADSR ─────────────────────────────────────────────────────────────────
    float attack   = 0.05f;
    float decay    = 0.10f;
    float sustain  = 0.80f;
    float release  = 0.30f;

    // ── Grain core ───────────────────────────────────────────────────────────
    float grainSizeMs = 80.f;
    float density     = 30.f;
    float position    = 0.5f;
    float posJitter   = 0.f;

    // ── Pitch / pan ───────────────────────────────────────────────────────────
    float pitchShift  = 0.f;
    float pitchJitter = 0.f;
    float panSpread   = 0.f;
    int   windowType  = 0;   // WindowType enum value (0–3)
    int   pitchScale  = 0;   // 0=Chromatic … 6=Dorian
    int   pitchRoot   = 0;   // 0=C … 11=B

    // ── Output ────────────────────────────────────────────────────────────────
    float outputGainDb = 0.f;

    // ── LFO 1 ─────────────────────────────────────────────────────────────────
    float lfoRate    = 1.f;
    float lfoDepth   = 0.f;
    int   lfoShape   = 0;
    int   lfoTarget  = 0;
    bool  lfoSync    = false;
    int   lfoSyncDiv = 3;    // index into syncMults[]

    // ── LFO 2 ─────────────────────────────────────────────────────────────────
    float lfoRate2   = 1.f;
    float lfoDepth2  = 0.f;
    int   lfoShape2  = 0;
    int   lfoTarget2 = 0;

    // ── LFO 3 ─────────────────────────────────────────────────────────────────
    float lfoRate3   = 1.f;
    float lfoDepth3  = 0.f;
    int   lfoShape3  = 0;
    int   lfoTarget3 = 0;

    // ── Envelope follower modulation ──────────────────────────────────────────
    bool  envActive  = false;
    float envAttack  = 0.05f;
    float envRelease = 0.30f;
    float envDepth   = 0.f;
    int   envTarget  = 0;

    // ── Step sequencer ────────────────────────────────────────────────────────
    bool  seqActive                       = false;
    float seqSteps[Glade::kSeqNumSteps]   = {};
    int   beatDivision                    = 4;   // index into beat mults[]
    bool  beatSync                        = false;

    // ── Host state ────────────────────────────────────────────────────────────
    double bpm = 120.0;
};
