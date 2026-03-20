#pragma once

/** Compile-time constants shared between the Engine and UI layers.
 *  Keep this header dependency-free so both layers can include it
 *  without pulling in JUCE or other engine headers. */
namespace Glade
{
    /** Number of step-sequencer steps.  Changing this value is the single
     *  point of truth — StepSequencer, GrainParams, and StepSequencerUI
     *  all derive their array sizes from here. */
    inline constexpr int kSeqNumSteps = 8;
}
