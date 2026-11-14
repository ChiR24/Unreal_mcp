// Copyright (c) 2025 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"

// The root of Index's set in a union-find parent array (every root is its own parent), halving the path on the way:
// each entry passed is pointed at its grandparent. The caller joins two sets by pointing one root at the other.
inline int32 McpUnionFindRoot(TArray<int32>& Parent, int32 Index)
{
    while (Parent[Index] != Index)
    {
        Parent[Index] = Parent[Parent[Index]];
        Index = Parent[Index];
    }
    return Index;
}
