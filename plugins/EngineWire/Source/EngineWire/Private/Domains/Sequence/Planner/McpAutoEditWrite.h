// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Domains/Sequence/Planner/McpFrameShot.h"
#include "Misc/FrameRate.h"

class AActor;
class ULevelSequence;
class UWorld;

// auto_edit's plan and write: segments timed from the playback range, every camera's shot scored per segment, the
// camera cut track rewritten with one cut per shot.
namespace McpAutoEdit
{
struct FEditPlan
{
    double SegmentSeconds = 0.5;
    int32 Segments = 0;
    int32 MinRun = 3;
    int32 MaxRun = 16;
    FFrameNumber Start;
    FFrameNumber End;
    FFrameRate Ticks;
    FFrameRate Display;
    TArray<TArray<double>> Shot;      // [segment][camera]
    TArray<TArray<FTransform>> Poses; // [segment][camera], as the sequence plays them
    TArray<FVector> Main;             // the main subject's middle per segment
    TArray<TOptional<FVector>> Second;
    TArray<double> Unseen;            // seconds into the range where no camera sees the main subject

    double CutCost(int32 Segment, int32 From, int32 To) const;
    TArray<TSharedPtr<FJsonValue>> UnseenJson() const;
    FFrameNumber SegmentStart(int32 Segment) const; // ticks
};

/** segmentSeconds, minShotSeconds and maxShotSeconds over the playback range; an error to refuse with, else empty. */
FString ReadTiming(const TSharedPtr<FJsonObject>& Payload, ULevelSequence& Sequence, FEditPlan& Plan);

void ScoreSegments(const UWorld& World, ULevelSequence& Sequence, const TArray<AActor*>& Cameras, const TArray<AActor*>& Subjects,
                   TFunctionRef<McpFrameShot::FLens(AActor&)> LensOf, FEditPlan& Plan);

TSharedPtr<FJsonObject> DescribeEdit(ULevelSequence& Sequence, const TArray<AActor*>& Cameras, const FEditPlan& Plan, const TArray<int32>& Edit);

/** The cut track's sections replaced by one per shot (cameras bound when they are not), one undo step. */
bool WriteEdit(ULevelSequence& Sequence, const TArray<AActor*>& Cameras, const FEditPlan& Plan, const TArray<int32>& Edit,
               const TSharedPtr<FJsonObject>& Result, FString& OutCode, FString& OutError);

FString EditMessage(const TArray<AActor*>& Cameras, const FEditPlan& Plan, const TArray<int32>& Edit, bool bDryRun);
} // namespace McpAutoEdit
