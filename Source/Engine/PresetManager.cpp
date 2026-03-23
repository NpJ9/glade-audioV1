#include "PresetManager.h"

//==============================================================================
// Helper — concise preset builder
static PresetManager::Preset makePreset (const char* name,
    const char* category,
    std::initializer_list<std::pair<const char*, float>> vals)
{
    PresetManager::Preset p;
    p.name     = name;
    p.category = category;
    for (auto& v : vals)
        p.params[v.first] = v.second;
    return p;
}

//==============================================================================
PresetManager::PresetManager()
{
    // ── Factory presets ───────────────────────────────────────────────────────
    presets.push_back (makePreset ("INIT", "Init",
    {
        {"grainSize",80.f},{"grainDensity",30.f},{"grainPosition",0.5f},{"posJitter",0.f},
        {"pitchShift",0.f},{"pitchJitter",0.f},{"panSpread",0.f},
        {"attack",0.05f},{"decay",0.1f},{"sustain",0.8f},{"release",0.3f},
        {"outputGain",0.f},{"dryWet",1.f},
        {"lfoRate",1.f},{"lfoDepth",0.f},{"pitchScale",0.f},{"pitchRoot",0.f},
    }));

    presets.push_back (makePreset ("FOREST", "Atmospheric",
    {
        {"grainSize",220.f},{"grainDensity",12.f},{"grainPosition",0.3f},{"posJitter",0.25f},
        {"pitchShift",0.f},{"pitchJitter",0.12f},{"panSpread",0.6f},
        {"attack",0.8f},{"decay",0.4f},{"sustain",0.7f},{"release",1.8f},
        {"outputGain",-3.f},{"dryWet",1.f},
        {"lfoRate",0.18f},{"lfoDepth",0.18f},
        {"pitchScale",1.f},{"pitchRoot",0.f},
        {"fxType1",1.f},{"fxP11",0.85f},{"fxP21",0.3f},{"fxP31",0.7f},{"fxMix1",0.55f},
    }));

    presets.push_back (makePreset ("SHIMMER", "Atmospheric",
    {
        {"grainSize",28.f},{"grainDensity",90.f},{"grainPosition",0.5f},{"posJitter",0.15f},
        {"pitchShift",12.f},{"pitchJitter",0.3f},{"panSpread",0.8f},
        {"attack",0.15f},{"decay",0.2f},{"sustain",0.9f},{"release",0.9f},
        {"outputGain",-2.f},{"dryWet",1.f},
        {"lfoRate",0.8f},{"lfoDepth",0.1f},
        {"pitchScale",1.f},{"pitchRoot",0.f},
        {"fxType1",1.f},{"fxP11",0.95f},{"fxP21",0.15f},{"fxP31",0.9f},{"fxMix1",0.7f},
    }));

    presets.push_back (makePreset ("GLITCH", "Experimental",
    {
        {"grainSize",12.f},{"grainDensity",160.f},{"grainPosition",0.5f},{"posJitter",0.9f},
        {"pitchShift",0.f},{"pitchJitter",0.85f},{"panSpread",1.f},
        {"attack",0.001f},{"decay",0.05f},{"sustain",1.f},{"release",0.08f},
        {"outputGain",0.f},{"dryWet",1.f},
        {"lfoRate",7.f},{"lfoDepth",0.4f},
        {"pitchScale",0.f},{"pitchRoot",0.f},
        {"fxType1",4.f},{"fxP11",0.7f},{"fxP21",0.5f},{"fxP31",0.5f},{"fxMix1",0.6f},
    }));

    presets.push_back (makePreset ("TAPE", "Experimental",
    {
        {"grainSize",150.f},{"grainDensity",22.f},{"grainPosition",0.5f},{"posJitter",0.08f},
        {"pitchShift",-0.5f},{"pitchJitter",0.05f},{"panSpread",0.2f},
        {"attack",0.3f},{"decay",0.3f},{"sustain",0.85f},{"release",1.2f},
        {"outputGain",-1.f},{"dryWet",1.f},
        {"lfoRate",0.3f},{"lfoDepth",0.06f},
        {"pitchScale",2.f},{"pitchRoot",0.f},
        {"fxType1",3.f},{"fxP11",0.2f},{"fxP21",0.3f},{"fxP31",0.2f},{"fxMix1",0.4f},
    }));

    presets.push_back (makePreset ("VOID", "Atmospheric",
    {
        {"grainSize",400.f},{"grainDensity",6.f},{"grainPosition",0.5f},{"posJitter",0.05f},
        {"pitchShift",-12.f},{"pitchJitter",0.f},{"panSpread",0.3f},
        {"attack",2.f},{"decay",1.f},{"sustain",1.f},{"release",5.f},
        {"outputGain",-4.f},{"dryWet",1.f},
        {"lfoRate",0.05f},{"lfoDepth",0.08f},
        {"pitchScale",4.f},{"pitchRoot",0.f},
        {"fxType1",1.f},{"fxP11",0.98f},{"fxP21",0.5f},{"fxP31",0.95f},{"fxMix1",0.9f},
    }));

    presets.push_back (makePreset ("CRYSTAL", "Experimental",
    {
        {"grainSize",8.f},{"grainDensity",140.f},{"grainPosition",0.6f},{"posJitter",0.2f},
        {"pitchShift",7.f},{"pitchJitter",0.4f},{"panSpread",0.7f},
        {"attack",0.05f},{"decay",0.15f},{"sustain",0.8f},{"release",0.5f},
        {"outputGain",-3.f},{"dryWet",1.f},
        {"lfoRate",2.f},{"lfoDepth",0.15f},
        {"pitchScale",5.f},{"pitchRoot",0.f},
        {"fxType1",5.f},{"fxP11",0.5f},{"fxP21",0.6f},{"fxP31",0.f},{"fxMix1",0.6f},
    }));

    presets.push_back (makePreset ("DRONE", "Atmospheric",
    {
        {"grainSize",300.f},{"grainDensity",8.f},{"grainPosition",0.45f},{"posJitter",0.02f},
        {"pitchShift",0.f},{"pitchJitter",0.02f},{"panSpread",0.1f},
        {"attack",1.5f},{"decay",0.5f},{"sustain",1.f},{"release",4.f},
        {"outputGain",0.f},{"dryWet",1.f},
        {"lfoRate",0.08f},{"lfoDepth",0.12f},
        {"pitchScale",6.f},{"pitchRoot",9.f},
        {"fxType1",1.f},{"fxP11",0.9f},{"fxP21",0.2f},{"fxP31",0.8f},{"fxMix1",0.75f},
    }));

    // ── Beat-sync presets ─────────────────────────────────────────────────────
    presets.push_back (makePreset ("PULSE", "Rhythmic",   // 1/16 tight rhythmic burst
    {
        {"grainSize",14.f},{"grainDensity",40.f},{"grainPosition",0.5f},{"posJitter",0.3f},
        {"beatSync",1.f},{"beatDivision",2.f},          // 1/16
        {"pitchShift",0.f},{"pitchJitter",0.1f},{"panSpread",0.5f},
        {"attack",0.001f},{"decay",0.05f},{"sustain",0.7f},{"release",0.12f},
        {"outputGain",0.f},{"dryWet",0.85f},
        {"lfoRate",4.f},{"lfoDepth",0.2f},{"lfoTarget",2.f},  // LFO → density
        {"fxType0",2.f},{"fxP10",0.3f},{"fxP20",0.55f},{"fxP30",0.f},{"fxMix0",0.45f},
        {"fxType1",4.f},{"fxP11",0.55f},{"fxP21",0.45f},{"fxP31",0.5f},{"fxMix1",0.4f},
    }));

    presets.push_back (makePreset ("GROOVE", "Rhythmic",  // 1/8 rhythmic shimmer + chorus
    {
        {"grainSize",60.f},{"grainDensity",25.f},{"grainPosition",0.4f},{"posJitter",0.2f},
        {"beatSync",1.f},{"beatDivision",4.f},          // 1/8
        {"pitchShift",0.f},{"pitchJitter",0.2f},{"panSpread",0.7f},
        {"attack",0.05f},{"decay",0.15f},{"sustain",0.85f},{"release",0.5f},
        {"outputGain",-2.f},{"dryWet",1.f},
        {"lfoRate",0.5f},{"lfoDepth",0.15f},{"lfoTarget",3.f},  // LFO → position
        {"fxType0",3.f},{"fxP10",0.35f},{"fxP20",0.55f},{"fxP30",0.25f},{"fxMix0",0.55f},
        {"fxType1",1.f},{"fxP11",0.7f},{"fxP21",0.25f},{"fxP31",0.8f},{"fxMix1",0.5f},
    }));

    presets.push_back (makePreset ("SEQUENCE", "Rhythmic", // 1/4 note gate — sequencer-like
    {
        {"grainSize",40.f},{"grainDensity",15.f},{"grainPosition",0.5f},{"posJitter",0.55f},
        {"beatSync",1.f},{"beatDivision",6.f},          // 1/4
        {"pitchShift",0.f},{"pitchJitter",0.35f},{"panSpread",0.6f},
        {"attack",0.001f},{"decay",0.2f},{"sustain",0.6f},{"release",0.25f},
        {"outputGain",0.f},{"dryWet",1.f},
        {"lfoRate",2.f},{"lfoDepth",0.25f},{"lfoTarget",4.f},  // LFO → pitch
        {"fxType0",2.f},{"fxP10",0.5f},{"fxP20",0.6f},{"fxP30",0.3f},{"fxMix0",0.6f},
        {"fxType1",5.f},{"fxP11",0.55f},{"fxP21",0.4f},{"fxP31",0.f},{"fxMix1",0.7f},
    }));

    presets.push_back (makePreset ("STUTTER", "Rhythmic",
    {
        {"grainSize",18.f},{"grainDensity",55.f},{"grainPosition",0.5f},{"posJitter",0.5f},
        {"beatSync",1.f},{"beatDivision",2.f},
        {"pitchShift",0.f},{"pitchJitter",0.5f},{"panSpread",0.9f},
        {"attack",0.01f},{"decay",0.1f},{"sustain",0.6f},{"release",0.2f},
        {"outputGain",0.f},{"dryWet",1.f},
        {"lfoRate",4.f},{"lfoDepth",0.35f},{"lfoTarget",2.f},
        {"fxType0",2.f},{"fxP10",0.2f},{"fxP20",0.7f},{"fxP30",1.f},{"fxMix0",0.5f},
        {"fxType1",3.f},{"fxP11",0.55f},{"fxP21",0.5f},{"fxP31",0.3f},{"fxMix1",0.4f},
    }));

    // ── Spacey presets ────────────────────────────────────────────────────────
    presets.push_back (makePreset ("NEBULA", "Atmospheric",
    {
        {"grainSize",350.f},{"grainDensity",18.f},{"grainPosition",0.5f},{"posJitter",0.3f},
        {"pitchShift",7.f},{"pitchJitter",0.08f},{"panSpread",0.85f},
        {"attack",3.f},{"decay",1.5f},{"sustain",0.9f},{"release",6.f},
        {"outputGain",-3.f},{"dryWet",1.f},
        {"lfoRate",0.04f},{"lfoDepth",0.2f},{"lfoTarget",1.f},
        {"fxType0",1.f},{"fxP10",0.97f},{"fxP20",0.1f},{"fxP30",1.f},{"fxMix0",0.85f},
        {"fxType1",2.f},{"fxP11",0.6f},{"fxP21",0.5f},{"fxP31",0.f},{"fxMix1",0.35f},
    }));

    presets.push_back (makePreset ("AURORA", "Atmospheric",
    {
        {"grainSize",120.f},{"grainDensity",45.f},{"grainPosition",0.6f},{"posJitter",0.4f},
        {"pitchShift",5.f},{"pitchJitter",0.15f},{"panSpread",1.f},
        {"attack",2.f},{"decay",0.8f},{"sustain",1.f},{"release",4.5f},
        {"outputGain",-2.f},{"dryWet",1.f},
        {"lfoRate",0.12f},{"lfoDepth",0.25f},{"lfoTarget",3.f},
        {"fxType0",3.f},{"fxP10",0.15f},{"fxP20",0.7f},{"fxP30",0.2f},{"fxMix0",0.65f},
        {"fxType1",1.f},{"fxP11",0.88f},{"fxP21",0.35f},{"fxP31",0.9f},{"fxMix1",0.7f},
    }));

    presets.push_back (makePreset ("COSMOS", "Atmospheric",
    {
        {"grainSize",500.f},{"grainDensity",4.f},{"grainPosition",0.5f},{"posJitter",0.1f},
        {"pitchShift",-7.f},{"pitchJitter",0.04f},{"panSpread",0.5f},
        {"attack",5.f},{"decay",2.f},{"sustain",1.f},{"release",8.f},
        {"outputGain",-5.f},{"dryWet",1.f},
        {"lfoRate",0.02f},{"lfoDepth",0.15f},{"lfoTarget",1.f},
        {"fxType0",1.f},{"fxP10",0.99f},{"fxP20",0.05f},{"fxP30",0.98f},{"fxMix0",0.95f},
        {"fxType1",5.f},{"fxP11",0.2f},{"fxP21",0.1f},{"fxP31",0.f},{"fxMix1",0.4f},
    }));

    // ── Rhythmic presets ──────────────────────────────────────────────────────
    presets.push_back (makePreset ("TRAP", "Rhythmic",
    {
        {"grainSize",9.f},{"grainDensity",80.f},{"grainPosition",0.5f},{"posJitter",0.6f},
        {"beatSync",1.f},{"beatDivision",2.f},
        {"pitchShift",0.f},{"pitchJitter",0.3f},{"panSpread",0.6f},
        {"attack",0.001f},{"decay",0.06f},{"sustain",0.5f},{"release",0.1f},
        {"outputGain",1.f},{"dryWet",1.f},
        {"lfoRate",8.f},{"lfoDepth",0.45f},{"lfoTarget",2.f},
        {"fxType0",4.f},{"fxP10",0.6f},{"fxP20",0.7f},{"fxP30",0.5f},{"fxMix0",0.55f},
        {"fxType1",2.f},{"fxP11",0.25f},{"fxP21",0.4f},{"fxP31",0.f},{"fxMix1",0.3f},
    }));

    presets.push_back (makePreset ("BOUNCE", "Rhythmic",
    {
        {"grainSize",30.f},{"grainDensity",35.f},{"grainPosition",0.45f},{"posJitter",0.35f},
        {"beatSync",1.f},{"beatDivision",4.f},
        {"pitchShift",0.f},{"pitchJitter",0.25f},{"panSpread",0.75f},
        {"attack",0.01f},{"decay",0.15f},{"sustain",0.7f},{"release",0.3f},
        {"outputGain",0.f},{"dryWet",1.f},
        {"lfoRate",2.f},{"lfoDepth",0.3f},{"lfoTarget",4.f},
        {"fxType0",3.f},{"fxP10",0.4f},{"fxP20",0.6f},{"fxP30",0.15f},{"fxMix0",0.5f},
        {"fxType1",2.f},{"fxP11",0.5f},{"fxP21",0.55f},{"fxP31",1.f},{"fxMix1",0.45f},
    }));

    presets.push_back (makePreset ("ARPFOG", "Rhythmic",
    {
        {"grainSize",50.f},{"grainDensity",20.f},{"grainPosition",0.5f},{"posJitter",0.2f},
        {"beatSync",1.f},{"beatDivision",3.f},
        {"pitchShift",0.f},{"pitchJitter",0.45f},{"panSpread",0.5f},
        {"attack",0.005f},{"decay",0.2f},{"sustain",0.65f},{"release",0.4f},
        {"outputGain",-1.f},{"dryWet",1.f},
        {"lfoRate",3.f},{"lfoDepth",0.4f},{"lfoTarget",4.f},
        {"fxType0",2.f},{"fxP10",0.33f},{"fxP20",0.5f},{"fxP30",0.5f},{"fxMix0",0.5f},
        {"fxType1",3.f},{"fxP11",0.3f},{"fxP21",0.5f},{"fxP31",0.1f},{"fxMix1",0.55f},
    }));

    // ── Experimental presets ──────────────────────────────────────────────────
    presets.push_back (makePreset ("MELT", "Experimental",
    {
        {"grainSize",180.f},{"grainDensity",10.f},{"grainPosition",0.5f},{"posJitter",0.55f},
        {"pitchShift",-2.f},{"pitchJitter",0.3f},{"panSpread",0.4f},
        {"attack",1.f},{"decay",0.5f},{"sustain",0.8f},{"release",3.f},
        {"outputGain",-2.f},{"dryWet",1.f},
        {"lfoRate",0.07f},{"lfoDepth",0.5f},{"lfoTarget",1.f},
        {"fxType0",4.f},{"fxP10",0.35f},{"fxP20",0.4f},{"fxP30",0.5f},{"fxMix0",0.5f},
        {"fxType1",1.f},{"fxP11",0.75f},{"fxP21",0.45f},{"fxP31",0.75f},{"fxMix1",0.65f},
    }));

    presets.push_back (makePreset ("CHAOS", "Experimental",
    {
        {"grainSize",5.f},{"grainDensity",200.f},{"grainPosition",0.5f},{"posJitter",1.f},
        {"pitchShift",0.f},{"pitchJitter",1.f},{"panSpread",1.f},
        {"attack",0.001f},{"decay",0.03f},{"sustain",1.f},{"release",0.05f},
        {"outputGain",-6.f},{"dryWet",1.f},
        {"lfoRate",12.f},{"lfoDepth",0.6f},{"lfoTarget",2.f},
        {"fxType0",4.f},{"fxP10",0.9f},{"fxP20",1.f},{"fxP30",0.5f},{"fxMix0",0.7f},
        {"fxType1",5.f},{"fxP11",0.7f},{"fxP21",0.8f},{"fxP31",0.67f},{"fxMix1",0.6f},
    }));

    presets.push_back (makePreset ("GLASS", "Experimental",
    {
        {"grainSize",7.f},{"grainDensity",120.f},{"grainPosition",0.55f},{"posJitter",0.1f},
        {"pitchShift",24.f},{"pitchJitter",0.6f},{"panSpread",0.65f},
        {"attack",0.03f},{"decay",0.12f},{"sustain",0.85f},{"release",0.6f},
        {"outputGain",-3.f},{"dryWet",1.f},
        {"lfoRate",1.5f},{"lfoDepth",0.12f},{"lfoTarget",1.f},
        {"fxType0",3.f},{"fxP10",0.6f},{"fxP20",0.85f},{"fxP30",0.35f},{"fxMix0",0.7f},
        {"fxType1",1.f},{"fxP11",0.8f},{"fxP21",0.05f},{"fxP31",0.95f},{"fxMix1",0.5f},
    }));

    presets.push_back (makePreset ("TAPE_LOOP", "Experimental",
    {
        {"grainSize",200.f},{"grainDensity",16.f},{"grainPosition",0.5f},{"posJitter",0.12f},
        {"beatSync",1.f},{"beatDivision",6.f},
        {"pitchShift",-1.f},{"pitchJitter",0.07f},{"panSpread",0.25f},
        {"attack",0.2f},{"decay",0.3f},{"sustain",0.9f},{"release",2.f},
        {"outputGain",-1.f},{"dryWet",0.9f},
        {"lfoRate",0.25f},{"lfoDepth",0.1f},{"lfoTarget",3.f},
        {"fxType0",4.f},{"fxP10",0.15f},{"fxP20",0.25f},{"fxP30",0.5f},{"fxMix0",0.35f},
        {"fxType1",2.f},{"fxP11",0.5f},{"fxP21",0.65f},{"fxP31",0.f},{"fxMix1",0.55f},
    }));

    // ── New atmospheric presets ───────────────────────────────────────────────

    presets.push_back (makePreset ("MIST", "Atmospheric",
    {   // Ultra-sparse, large grains, barely-there pads
        {"grainSize",280.f},{"grainDensity",8.f},{"grainPosition",0.4f},{"posJitter",0.3f},
        {"pitchShift",0.f},{"pitchJitter",0.05f},{"panSpread",0.7f},
        {"attack",3.f},{"decay",1.f},{"sustain",0.85f},{"release",5.f},
        {"outputGain",-4.f},{"dryWet",1.f},
        {"lfoRate",0.06f},{"lfoDepth",0.15f},{"lfoTarget",3.f},
        {"pitchScale",1.f},{"pitchRoot",0.f},
        {"fxType0",1.f},{"fxP10",0.96f},{"fxP20",0.2f},{"fxP30",0.92f},{"fxMix0",0.8f},
    }));

    presets.push_back (makePreset ("TIDE", "Atmospheric",
    {   // Slow rising-and-falling texture, LFO on grain size
        {"grainSize",180.f},{"grainDensity",15.f},{"grainPosition",0.5f},{"posJitter",0.2f},
        {"pitchShift",-3.f},{"pitchJitter",0.08f},{"panSpread",0.6f},
        {"attack",2.5f},{"decay",0.8f},{"sustain",0.9f},{"release",4.f},
        {"outputGain",-3.f},{"dryWet",1.f},
        {"lfoRate",0.08f},{"lfoDepth",0.35f},{"lfoTarget",1.f},
        {"pitchScale",2.f},{"pitchRoot",0.f},
        {"fxType0",1.f},{"fxP10",0.88f},{"fxP20",0.3f},{"fxP30",0.85f},{"fxMix0",0.65f},
    }));

    presets.push_back (makePreset ("LANTERN", "Atmospheric",
    {   // Warm, slightly bright, intimate pad
        {"grainSize",100.f},{"grainDensity",25.f},{"grainPosition",0.55f},{"posJitter",0.15f},
        {"pitchShift",3.f},{"pitchJitter",0.1f},{"panSpread",0.5f},
        {"attack",1.f},{"decay",0.6f},{"sustain",0.9f},{"release",2.5f},
        {"outputGain",-2.f},{"dryWet",1.f},
        {"lfoRate",0.15f},{"lfoDepth",0.1f},{"lfoTarget",3.f},
        {"pitchScale",1.f},{"pitchRoot",5.f},   // Major / F
        {"fxType0",1.f},{"fxP10",0.78f},{"fxP20",0.4f},{"fxP30",0.75f},{"fxMix0",0.55f},
        {"fxType1",3.f},{"fxP11",0.2f},{"fxP21",0.6f},{"fxP31",0.1f},{"fxMix1",0.3f},
    }));

    presets.push_back (makePreset ("ETHER", "Atmospheric",
    {   // Barely-there wisps, extreme wide stereo, ultra-long tails
        {"grainSize",450.f},{"grainDensity",5.f},{"grainPosition",0.5f},{"posJitter",0.4f},
        {"pitchShift",5.f},{"pitchJitter",0.2f},{"panSpread",0.95f},
        {"attack",4.f},{"decay",2.f},{"sustain",1.f},{"release",7.f},
        {"outputGain",-5.f},{"dryWet",1.f},
        {"lfoRate",0.03f},{"lfoDepth",0.2f},{"lfoTarget",1.f},
        {"pitchScale",5.f},{"pitchRoot",0.f},   // Whole Tone
        {"fxType0",1.f},{"fxP10",0.99f},{"fxP20",0.08f},{"fxP30",0.97f},{"fxMix0",0.9f},
    }));

    presets.push_back (makePreset ("AMBER", "Atmospheric",
    {   // Warm golden pad, gentle LFO drift, slow chorusing
        {"grainSize",200.f},{"grainDensity",12.f},{"grainPosition",0.45f},{"posJitter",0.1f},
        {"pitchShift",-1.f},{"pitchJitter",0.06f},{"panSpread",0.4f},
        {"attack",1.5f},{"decay",0.6f},{"sustain",0.9f},{"release",3.5f},
        {"outputGain",-2.f},{"dryWet",1.f},
        {"lfoRate",0.1f},{"lfoDepth",0.12f},{"lfoTarget",3.f},
        {"pitchScale",1.f},{"pitchRoot",7.f},   // Major / G
        {"fxType0",3.f},{"fxP10",0.25f},{"fxP20",0.65f},{"fxP30",0.2f},{"fxMix0",0.45f},
        {"fxType1",1.f},{"fxP11",0.82f},{"fxP21",0.35f},{"fxP31",0.8f},{"fxMix1",0.6f},
    }));
}

//==============================================================================
void PresetManager::applyPreset (const Preset& p,
                                  juce::AudioProcessorValueTreeState& apvts)
{
    // Always reset toggle-state params to off before applying so that presets
    // which don't include them don't inherit the previous preset's state.
    static const char* resetIds[] = { "beatSync", "seqActive" };
    for (auto* id : resetIds)
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (0.f);
            param->endChangeGesture();
        }
    }

    for (auto& [id, val] : p.params)
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
        {
            const float norm = param->getNormalisableRange().convertTo0to1 (val);
            param->beginChangeGesture();
            param->setValueNotifyingHost (norm);
            param->endChangeGesture();
        }
    }
}

void PresetManager::loadPreset (int index, juce::AudioProcessorValueTreeState& apvts)
{
    currentIndex = juce::jlimit (0, (int) presets.size() - 1, index);
    applyPreset (presets[currentIndex], apvts);
}

void PresetManager::nextPreset (juce::AudioProcessorValueTreeState& apvts)
{
    loadPreset ((currentIndex + 1) % (int) presets.size(), apvts);
}

void PresetManager::prevPreset (juce::AudioProcessorValueTreeState& apvts)
{
    const int n = (int) presets.size();
    loadPreset ((currentIndex + n - 1) % n, apvts);
}

void PresetManager::setCurrentByName (const juce::String& name)
{
    for (int i = 0; i < (int) presets.size(); ++i)
    {
        if (presets[i].name == name)
        {
            currentIndex = i;
            return;
        }
    }
}
