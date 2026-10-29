// Copyright (c) 2024 EngineWire Contributors

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/Function.h"

class AActor;

namespace McpSpawnRepeat {
/**
 * A spawn_batch item's `repeat` ({count, offset, rows, rowOffset}) unrolled into count x rows copies of the item,
 * each moved from the item's location by offset per copy and rowOffset per row, its actorName numbered _1, _2, ...
 * A row of racks or a grid of ceiling lights was one hand-written item per actor. With alongSpline ({alongSpline,
 * count or spacingCm, startCm, sideOffsetCm, heightCm, alignToSpline}) the copies stand on the spline of the actor
 * Find resolves instead: lamps along a road, posts along a path. An item without repeat is appended as it is. False,
 * with Error, for a malformed repeat, a spline actor not found, or when Out would pass MaxActors.
 */
bool Expand(const TSharedPtr<FJsonObject> &Item, int32 MaxActors, TArray<TSharedPtr<FJsonObject>> &Out,
            FString &Error, TFunctionRef<AActor *(const FString &)> Find);
} // namespace McpSpawnRepeat
