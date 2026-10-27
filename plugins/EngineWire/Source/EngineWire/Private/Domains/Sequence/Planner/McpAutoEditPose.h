// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Misc/FrameTime.h"

class AActor;
class ULevelSequence;
class UMovieScene3DTransformSection;

// auto_edit's view of a sequence: where an actor stands at a time as the sequence plays it.
namespace McpAutoEdit
{
/** An actor and the transform sections the sequence keys it with (none: it stays where it stands in the level). */
struct FPoseTrack
{
    AActor* Actor = nullptr;
    FTransform Rest = FTransform::Identity;
    TArray<UMovieScene3DTransformSection*> Sections;
};

FPoseTrack TrackOf(ULevelSequence& Sequence, AActor& Actor);

/** Its pose at Time (tick resolution): the channels a section holding Time keys, the rest as it stands in the level. */
FTransform PoseAt(const FPoseTrack& Track, FFrameTime Time);
} // namespace McpAutoEdit
