// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Domains/AssetQuery/Retrieval/McpAssetIndex.h"

// find_assets_like's ranking: each criterion asked ranks the candidates it can judge, and the rankings are fused.
namespace McpAssetRetrieval
{
struct FAsk
{
    TArray<FString> Words;       // the query's words (WordsOf)
    FString Like;                // a package path: its words, look, size and class
    TOptional<FVector3f> Colour; // OKLab
    double SizeCm = 0.0;         // the largest dimension wanted
};

struct FRanked
{
    FString Path;
    double Score = 0.0;     // the sum over criteria of 1 / (60 + place)
    TArray<FString> Why;    // each criterion that ranked it, with its place there ("look 2")
};

/** Paths (each with an entry in Index.Assets) ranked by every criterion Ask names and fused, best first; Ask.Like itself
 *  is left out. */
TArray<FRanked> Rank(const FIndex& Index, const TArray<FString>& Paths, const FAsk& Ask);
} // namespace McpAssetRetrieval
