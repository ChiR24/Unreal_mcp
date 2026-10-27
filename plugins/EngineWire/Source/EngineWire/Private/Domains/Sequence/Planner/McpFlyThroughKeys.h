// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class AActor;
class ULevelSequence;

// fly_through's sequence side: the camera bound, keyed and cut to.
namespace McpFlyThrough
{
/** The camera flown: the actor named when one has the label, else a CineCameraActor spawned under it. */
struct FFlyCamera
{
    AActor* Actor = nullptr;
    FString Label;
};

/** The flight sampled for keys: seconds from its start, where the camera is and how it is turned. */
struct FFlightKeys
{
    TArray<double> Times;
    TArray<FVector> Locations;
    TArray<FRotator> Rotations;
};

/** The camera (spawned if need be) bound to Sequence and keyed along Keys from its playback start, with a camera cut
 *  to it over the flight and the playback range grown to hold it, as one undo step; false with why not. */
bool WriteFlight(ULevelSequence& Sequence, FFlyCamera& Camera, const FFlightKeys& Keys, const TSharedPtr<FJsonObject>& Result,
                 FString& OutCode, FString& OutError);
} // namespace McpFlyThrough
