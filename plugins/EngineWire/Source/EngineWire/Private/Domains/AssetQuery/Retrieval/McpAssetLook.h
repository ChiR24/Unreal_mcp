// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"

// What an asset's stored thumbnail shows, kept small enough to hold for thousands of assets: the colours of the drawn
// part (the backdrop is what the tile's frame shows: a plain colour or the editor's checker), the orientation of its
// edges, and its silhouette's shape; a picture with no backdrop (a texture) is drawn all over.
namespace McpAssetRetrieval
{
struct FLook
{
    TArray<FVector4f> Palette; // OKLab L, a, b and the share of the drawn part, largest first (4 at most)
    TArray<float> Edges;       // 8 orientation bins of the drawn part's edges, summing to 1
    TArray<float> Hu;          // the silhouette's 7 Hu moments, on a sign-keeping log scale
    float Drawn = 0.f;         // share of the tile the asset covers
};

/** The look of a BGRA thumbnail; false when nothing in it stands out from its backdrop. */
bool LookOf(const TArray<uint8>& Bgra, int32 Width, int32 Height, FLook& Out);

/** How unlike two looks are (0 the same): palette by closest colour weighted by share, edges and silhouette by L1. */
double Unlike(const FLook& A, const FLook& B);

/** The share of a look's drawn part within DeltaE (OKLab distance) of Colour. */
double ShareNear(const FLook& Look, const FVector3f& Colour, double DeltaE);
} // namespace McpAssetRetrieval
