// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Domains/Sequence/Planner/McpFrameShot.h"

struct FCollisionQueryParams;
class UWorld;

// auto_edit's costs: what a camera's shot of the subjects is worth at one time, and what a cut costs.
namespace McpAutoEdit
{
/** A subject where the sequence has it at the time scored, with the sight-line query that leaves it out. */
struct FSubjectAt
{
    FBox Box = FBox(ForceInit);
    const FCollisionQueryParams* Params = nullptr;
};

struct FShotScore
{
    double Cost = 0.0;
    bool bMainSeen = false; // the main subject is in frame and not wholly hidden
};

/** Hidden or out-of-frame share of each subject (the first, the main one, counts double) plus the main one's size. */
FShotScore ScoreShot(const UWorld& World, const FTransform& Camera, const McpFrameShot::FLens& Lens, const TArray<FSubjectAt>& Subjects);

/** A cut from From to To: a base cost, a jump (within 30 degrees round Main), a crossing of the line Main-Second. */
double ScoreCut(const FTransform& From, const FTransform& To, const FVector& Main, const TOptional<FVector>& Second);
} // namespace McpAutoEdit
