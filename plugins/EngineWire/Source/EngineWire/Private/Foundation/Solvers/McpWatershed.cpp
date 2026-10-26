// Copyright (c) 2024 EngineWire Contributors

#include "Foundation/Solvers/McpWatershed.h"

namespace McpWatershed
{
namespace
{
int32 McpBasinRoot(TArray<int32>& Parent, int32 Basin)
{
    while (Parent[Basin] != Basin)
    {
        Parent[Basin] = Parent[Parent[Basin]];
        Basin = Parent[Basin];
    }
    return Basin;
}
} // namespace

TArray<int32> Basins(const TArray<TArray<int32>>& Adjacency, const TArray<int32>& Height, double MergeRatio,
                     int32& OutCount)
{
    const int32 Num = Height.Num();
    TArray<int32> Order;
    Order.Reserve(Num);
    for (int32 Index = 0; Index < Num; ++Index)
    {
        Order.Add(Index);
    }
    Order.Sort([&Height](int32 L, int32 R) { return Height[L] != Height[R] ? Height[L] > Height[R] : L < R; });

    TArray<int32> Label;
    Label.Init(INDEX_NONE, Num);
    TArray<int32> Parent; // basins as a union-find, each root holding its own peak
    TArray<int32> Peak;
    for (const int32 Node : Order)
    {
        int32 Best = INDEX_NONE;
        for (const int32 Next : Adjacency[Node])
        {
            if (Label[Next] == INDEX_NONE)
            {
                continue;
            }
            const int32 Basin = McpBasinRoot(Parent, Label[Next]);
            if (Best == INDEX_NONE || Basin == Best)
            {
                Best = Basin;
                continue;
            }
            const int32 High = Peak[Best] >= Peak[Basin] ? Best : Basin;
            if (Height[Node] >= MergeRatio * FMath::Min(Peak[Best], Peak[Basin]))
            {
                Parent[High == Best ? Basin : Best] = High; // the higher peak's basin keeps the merged one
            }
            Best = High;
        }
        if (Best == INDEX_NONE)
        {
            Best = Parent.Num();
            Parent.Add(Best);
            Peak.Add(Height[Node]);
        }
        Label[Node] = Best;
    }

    TMap<int32, int32> Compact; // roots to 0..n-1 in the order their first node appears
    for (int32& Basin : Label)
    {
        const int32 Root = McpBasinRoot(Parent, Basin);
        const int32* Found = Compact.Find(Root);
        Basin = Found ? *Found : Compact.Add(Root, Compact.Num());
    }
    OutCount = Compact.Num();
    return Label;
}
} // namespace McpWatershed
