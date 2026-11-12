// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Templates/Function.h"

// Simulated annealing by the Metropolis rule, over a state the caller keeps (Foundation/Solvers). The caller makes one
// random change at a time and says how much it changed the energy: a change that lowers it is kept, one that raises it by
// D is kept with probability exp(-D / T) while the temperature T falls geometrically from start to end, so the search can
// climb out of a local minimum early on and settles late. Deterministic for a seed.
namespace McpAnneal
{
struct FSettings
{
    int32 Steps = 20000;
    double StartTemperature = 0.25;
    double EndTemperature = 0.001;
    int32 Seed = 1;
    double DeadlineSeconds = 10.0;
};

struct FOutcome
{
    double BestEnergy = 0.0;
    int32 StepsRun = 0;
    int32 Accepted = 0;
    bool bDeadline = false;
};

/** Propose makes one change (Progress runs from 0 to 1, so steps can shrink) and returns the energy difference it made;
 *  Revert undoes that change; KeepBest is called whenever the current state is the best yet, so the caller can copy it.
 *  The caller's state is left at the last accepted change: restore the copy KeepBest made. */
FOutcome Minimize(double StartEnergy, const FSettings& Settings, TFunctionRef<double(FRandomStream&, double)> Propose,
                  TFunctionRef<void()> Revert, TFunctionRef<void()> KeepBest);
} // namespace McpAnneal
