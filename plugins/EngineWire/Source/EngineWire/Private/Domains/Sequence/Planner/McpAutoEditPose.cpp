// Copyright (c) 2024 EngineWire Contributors
//
// auto_edit's view of the sequence (McpAutoEditPose.h): where a bound actor stands at a time as the sequence plays it
// (its transform track's channels evaluated there), else where it stands in the level.

#include "Domains/Sequence/Planner/McpAutoEditPose.h"

#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Domains/Sequence/Cinematics/McpSeqCine.h"
#include "GameFramework/Actor.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Tracks/MovieScene3DTransformTrack.h"

namespace McpAutoEdit
{
FPoseTrack TrackOf(ULevelSequence& Sequence, AActor& Actor)
{
    FPoseTrack Out;
    Out.Actor = &Actor;
    Out.Rest = Actor.GetActorTransform();
    const FGuid Binding = McpSequenceCinematics::FindExistingBinding(&Sequence, &Actor, Actor.GetWorld());
    UMovieScene* MovieScene = Sequence.GetMovieScene();
    const UMovieScene3DTransformTrack* Track = Binding.IsValid() && MovieScene ? MovieScene->FindTrack<UMovieScene3DTransformTrack>(Binding) : nullptr;
    if (Track)
    {
        for (UMovieSceneSection* Section : Track->GetAllSections())
        {
            if (UMovieScene3DTransformSection* Transform = Cast<UMovieScene3DTransformSection>(Section))
            {
                Out.Sections.Add(Transform);
            }
        }
    }
    return Out;
}

// The pose at Time: the first section holding Time gives each channel it can evaluate; the rest come from the level.
FTransform PoseAt(const FPoseTrack& Track, FFrameTime Time)
{
    FVector Location = Track.Rest.GetLocation();
    FRotator Rotation = Track.Rest.Rotator();
    for (UMovieScene3DTransformSection* Section : Track.Sections)
    {
        if (!Section || !Section->GetRange().Contains(Time.FrameNumber))
        {
            continue;
        }
        TArrayView<FMovieSceneDoubleChannel*> Channels = Section->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
        double* const Targets[6] = {&Location.X, &Location.Y, &Location.Z, &Rotation.Roll, &Rotation.Pitch, &Rotation.Yaw};
        for (int32 Index = 0; Index < 6 && Index < Channels.Num(); ++Index)
        {
            double Value = 0.0;
            if (Channels[Index] && Channels[Index]->Evaluate(Time, Value))
            {
                *Targets[Index] = Value;
            }
        }
        break;
    }
    return FTransform(Rotation, Location, Track.Rest.GetScale3D());
}
} // namespace McpAutoEdit
