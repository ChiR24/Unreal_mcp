// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"

// A multiple-choice knapsack by dynamic programming (Foundation/Optimize): from groups of options, at most one option per
// group, the pick whose gains add up to at least what is needed at the least summed cost (system_control
// fit_frame_budget: frame milliseconds saved against how much of the picture changes). Gains are counted in whole steps,
// rounded down, so a pick never promises more than its options measured.
namespace McpChoiceKnapsack
{
struct FOption
{
    int32 Group = 0;
    double Gain = 0.0;
    double Cost = 0.0;
};

/** Indexes into Options, ascending, of the cheapest pick whose gains reach Need, at most one option per group; empty
 *  with bOutReachable false when every group's largest gain together falls short. A Need at or below zero is reached by
 *  picking nothing. Gains are counted in steps of Unit, or coarser when Need would take more than 4096 of them. */
TArray<int32> Pick(const TArray<FOption>& Options, double Need, double Unit, bool& bOutReachable);
} // namespace McpChoiceKnapsack
