// Copyright (c) 2024 EngineWire Contributors
//
// describe_relations' patterns among the items that are not ground. groups: DBSCAN over footprints (a neighbour within
// 1.25 median footprints of one, gap to gap, at least 3 members, never the whole scene): a table with the chairs drawn
// up to it and the lamp on it. The rows, grids and rings that copies of one prop form come from McpRelationsCopies.cpp.

#include "Domains/ControlActor/Relations/McpRelations.h"

#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"

namespace McpRelations
{
namespace
{
TArray<int32> Within(const TArray<FItem>& Items, const TArray<int32>& Props, int32 At, double Eps)
{
    TArray<int32> Out;
    for (const int32 Other : Props)
    {
        if (Other != At && FlatGap(Items[At].Box, Items[Other].Box) <= Eps)
        {
            Out.Add(Other);
        }
    }
    return Out;
}

// DBSCAN with a minimum of 3 points: an item with 2 neighbours within Eps of its footprint is a core point and brings
// its neighbours in.
TArray<TArray<int32>> Cluster(const TArray<FItem>& Items, const TArray<int32>& Props, double Eps)
{
    TMap<int32, int32> Label; // item -> its group, or -1 while it is noise
    TArray<TArray<int32>> Groups;
    for (const int32 Seed : Props)
    {
        TArray<int32> Open = Label.Contains(Seed) ? TArray<int32>() : Within(Items, Props, Seed, Eps);
        if (Label.Contains(Seed) || Open.Num() < 2)
        {
            Label.FindOrAdd(Seed, -1);
            continue;
        }
        const int32 Group = Groups.AddDefaulted();
        Groups[Group].Add(Seed);
        Label.Add(Seed, Group);
        while (Open.Num() > 0)
        {
            const int32 At = Open.Pop();
            if (Label.Contains(At) && Label[At] >= 0)
            {
                continue;
            }
            Label.Add(At, Group);
            Groups[Group].Add(At);
            const TArray<int32> More = Within(Items, Props, At, Eps);
            if (More.Num() >= 2)
            {
                Open.Append(More);
            }
        }
    }
    return Groups;
}
} // namespace

FPatterns FindPatterns(const TArray<FItem>& Items, const TSet<FString>& Kinds)
{
    FPatterns Out;
    TArray<int32> Props;
    TArray<double> Sizes;
    TMap<FString, TArray<int32>> Copies;
    for (int32 Index = 0; Index < Items.Num(); ++Index)
    {
        if (!Items[Index].bGround)
        {
            Props.Add(Index);
            Sizes.Add(Footprint(Items[Index]));
            Copies.FindOrAdd(Items[Index].Shape).Add(Index);
        }
    }
    if ((Kinds.Num() == 0 || Kinds.Contains(TEXT("groups"))) && Props.Num() >= 3)
    {
        Sizes.Sort();
        for (const TArray<int32>& Members : Cluster(Items, Props, FMath::Clamp(1.25 * Sizes[Sizes.Num() / 2], 50.0, 1000.0)))
        {
            if (Members.Num() >= 3 && Members.Num() < Props.Num()) // the whole scene as one group says nothing
            {
                FBox Bounds(ForceInit);
                for (const int32 Index : Members)
                {
                    Bounds += Items[Index].Box;
                }
                TSharedPtr<FJsonObject> Group = MakeShared<FJsonObject>();
                Group->SetArrayField(TEXT("members"), MemberNames(Items, Members));
                Group->SetArrayField(TEXT("center"), McpHandlerUtils::VectorToJsonArray(Bounds.GetCenter()));
                Group->SetArrayField(TEXT("size"), McpHandlerUtils::VectorToJsonArray(Bounds.GetSize()));
                Out.Groups.Add(MakeShared<FJsonValueObject>(Group));
            }
        }
    }
    for (const TPair<FString, TArray<int32>>& Prop : Copies)
    {
        FindArrangements(Items, Prop.Value, Kinds, Out);
    }
    return Out;
}
} // namespace McpRelations
