// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class AActor;
struct FMcpMotionInput;

// sample_motion feel: how a move feels, in numbers, from the run's own frames. Whether a jump floats, a stop slides or a
// key is slow to answer was a judgement read off a list of positions; with feel the run samples every frame and sums it
// up: the jump (time to apex, height, air time, the fall against the rise), the run (top speed, time to reach it, time and
// distance to stop once the keys are let go), each key's latency to the first motion it caused, and the camera's jerk.
struct FMcpMotionFeel
{
    bool bOn = false;
    TArray<double> Times;      // game seconds from the run's start
    TArray<FVector> Locations; // the actor
    TArray<FVector> Cameras;   // the first local player's camera, one per frame while there is one
};

/** One key as the summary reads it: game seconds from the run's start it went down and up (-1: never). */
struct FMcpFeelPress
{
    FString Key;
    double DownAt = -1.0;
    double UpAt = -1.0;
};

/** Reads feel; on, the run samples every frame unless intervalSeconds says otherwise. */
void McpParseMotionFeel(const TSharedPtr<FJsonObject>& Payload, FMcpMotionFeel& Feel, double& Interval);

/** Records one frame: the actor's location and the player's camera. */
void McpSampleMotionFeel(FMcpMotionFeel& Feel, const AActor* Actor, double Time);

/** Adds feel (the summary) to the result when the run asked for it. */
void McpAddMotionFeel(const FMcpMotionFeel& Feel, const TArray<FMcpMotionInput>& Inputs, FJsonObject& Data);

/** The summary from the frames, with no engine state: jump, run, keys and camera, each only where it applies. */
TSharedPtr<FJsonObject> McpMotionFeelSummary(const TArray<double>& Times, const TArray<FVector>& Locations,
                                             const TArray<FVector>& Cameras, const TArray<FMcpFeelPress>& Presses);
