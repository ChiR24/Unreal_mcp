// Copyright (c) 2024 EngineWire Contributors

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace McpSpawnRepeat {
/**
 * A spawn_batch item's `repeat` ({count, offset, rows, rowOffset}) unrolled into count x rows copies of the item,
 * each moved from the item's location by offset per copy and rowOffset per row, its actorName numbered _1, _2, ...
 * A row of racks or a grid of ceiling lights was one hand-written item per actor. An item without repeat is
 * appended as it is. False, with Error, for a malformed repeat or when Out would pass MaxActors.
 */
bool Expand(const TSharedPtr<FJsonObject> &Item, int32 MaxActors, TArray<TSharedPtr<FJsonObject>> &Out,
            FString &Error);
} // namespace McpSpawnRepeat
