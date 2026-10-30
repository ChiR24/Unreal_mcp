// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UAnimSequence;

// A locomotion clip's ground velocity read from its own frames, root motion left on the root bone: a root that travels
// (10 cm/s or more) is the clip's motion; a root that stays is an in-place clip, whose planted feet (frames within
// ContactHeight of a foot's lowest point) slide backward at the ground speed.
namespace McpAnimLocomotion
{
/** The feet to check: the bones Params names in feet, else every bone whose name holds "foot" but not "ik". */
TArray<FName> FeetOf(const TSharedPtr<FJsonObject>& Params, const TArray<FName>& Bones);

/** Where the root and each foot are over up to 120 frames spread evenly, the first and the last included. */
struct FFeetSample
{
    TArray<FName> Feet;
    TArray<TArray<FVector>> Paths; // per foot, component space
    TArray<double> Times;          // seconds, per sample
    FVector RootFirst = FVector::ZeroVector;
    FVector RootLast = FVector::ZeroVector;
    FString Error;                 // set when the clip could not be read
    FString ErrorCode;
};

FFeetSample SampleFeet(UAnimSequence* Sequence, const TSharedPtr<FJsonObject>& Params);

struct FMotion
{
    FVector2D Velocity = FVector2D::ZeroVector; // component space, cm/s
    bool bFromFeet = false;                     // in place: read from the planted feet
    FString Error;                              // set when the clip could not be read
    FString ErrorCode;
};

FMotion Measure(UAnimSequence* Sequence, const TSharedPtr<FJsonObject>& Params, double ContactHeight);
} // namespace McpAnimLocomotion
