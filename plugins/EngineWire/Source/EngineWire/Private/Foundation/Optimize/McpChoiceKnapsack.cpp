// Copyright (c) 2024 EngineWire Contributors

#include "Foundation/Optimize/McpChoiceKnapsack.h"

namespace McpChoiceKnapsack
{
TArray<int32> Pick(const TArray<FOption>& Options, double Need, double Unit, bool& bOutReachable)
{
    bOutReachable = true;
    if (Need <= 0.0)
    {
        return {};
    }
    const double Step = FMath::Max(Unit, Need / 4096.0);
    const int32 Target = FMath::Max(1, FMath::CeilToInt32(Need / Step - 1e-9));
    TArray<int32> Groups;
    for (const FOption& Option : Options)
    {
        Groups.AddUnique(Option.Group);
    }
    const double Unreached = TNumericLimits<double>::Max();
    TArray<double> Best; // Best[Units]: the least cost reaching Units (Target: Target or more)
    Best.Init(Unreached, Target + 1);
    Best[0] = 0.0;
    // Per group and units reached: the option taken there (INDEX_NONE: none, the units are the group before's) and
    // the units before it.
    TArray<TArray<int32>> Taken;
    TArray<TArray<int32>> From;
    for (const int32 Group : Groups)
    {
        TArray<double> Next = Best;
        TArray<int32>& Took = Taken.AddDefaulted_GetRef();
        TArray<int32>& Came = From.AddDefaulted_GetRef();
        Took.Init(INDEX_NONE, Target + 1);
        Came.Init(INDEX_NONE, Target + 1);
        for (int32 Index = 0; Index < Options.Num(); ++Index)
        {
            const FOption& Option = Options[Index];
            const int32 Units = Option.Group == Group ? FMath::FloorToInt32(Option.Gain / Step + 1e-9) : 0;
            // From the group before's table only, so one group gives at most one option.
            for (int32 Have = 0; Units > 0 && Have <= Target; ++Have)
            {
                const int32 Reach = FMath::Min(Target, Have + Units);
                if (Best[Have] != Unreached && Best[Have] + Option.Cost < Next[Reach])
                {
                    Next[Reach] = Best[Have] + Option.Cost;
                    Took[Reach] = Index;
                    Came[Reach] = Have;
                }
            }
        }
        Best = MoveTemp(Next);
    }
    if (Best[Target] == Unreached)
    {
        bOutReachable = false;
        return {};
    }
    TArray<int32> Picked;
    int32 Units = Target;
    for (int32 Group = Groups.Num() - 1; Group >= 0; --Group)
    {
        if (Taken[Group][Units] != INDEX_NONE)
        {
            Picked.Add(Taken[Group][Units]);
            Units = From[Group][Units];
        }
    }
    Picked.Sort();
    return Picked;
}
} // namespace McpChoiceKnapsack
