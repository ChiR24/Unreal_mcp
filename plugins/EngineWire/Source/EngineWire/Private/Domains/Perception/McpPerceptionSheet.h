// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"

// inspect contact_sheet: tiles captured around an actor (McpPerceptionSheet), shaded and drawn into one labelled image
// (McpPerceptionSheetDraw).
namespace McpPerception::Sheet
{
struct FTile
{
    FString Label;
    TArray<FColor> Pixels; // square
    TArray<float> Columns; // grid lines, in pixels from the tile's left edge (none for a perspective view)
    TArray<float> Rows;    // and from its top edge
};

/** A tile shaded as clay from its passes, the same whatever lights the level has (a face turned from a lone sun read
 *  black): Normals are world normals, Depth the scene depth in cm, Alone the depth with only the subject drawn. The
 *  subject's own surface is orange and what else is drawn grey, both lit from behind the camera, above and to its left;
 *  where the subject lies behind something (a foot sunk in the floor) it shows through it; nothing drawn is background. */
TArray<FColor> Shade(const TArray<FLinearColor>& Normals, const TArray<FLinearColor>& Depth, const TArray<FLinearColor>& Alone,
                     const FVector& Forward, const FVector& Right, const FVector& Up);

/** The tiles laid out three across under a title, each with its label above it and its grid over it. */
TArray<FColor> Compose(const FString& Title, const TArray<FTile>& Tiles, int32 TileSize, int32& OutWidth, int32& OutHeight);
} // namespace McpPerception::Sheet
