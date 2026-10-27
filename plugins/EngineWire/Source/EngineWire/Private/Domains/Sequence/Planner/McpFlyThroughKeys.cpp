// Copyright (c) 2024 EngineWire Contributors
//
// fly_through's sequence side (McpFlyThroughKeys.h): the camera bound to the sequence, its flight keyed on a transform
// track of its own, a camera cut to it over the flight, and the playback range grown to hold it, as one undo step.

#include "Domains/Sequence/Planner/McpFlyThroughKeys.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Domains/Sequence/Cinematics/McpSeqCine.h"
#include "Domains/Sequence/McpSeqEditorSupport.h"
#include "Foundation/BridgeHelpers/Actors/McpActorSpawnHelpers.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneObjectBindingID.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#if MCP_HAS_CINEMATIC_CAMERA
#include "CineCameraActor.h"
#endif

namespace McpFlyThrough
{
namespace
{
// The camera's transform track, replaced by one holding only this flight (the flight owns the camera's motion).
UMovieScene3DTransformSection* FreshTransformSection(UMovieScene& MovieScene, const FGuid& Binding, FFrameNumber Start, FFrameNumber End,
                                                     bool& bOutReplaced)
{
    UMovieScene3DTransformTrack* Old = MovieScene.FindTrack<UMovieScene3DTransformTrack>(Binding);
    bOutReplaced = Old != nullptr;
    if (Old)
    {
        MovieScene.RemoveTrack(*Old);
    }
    UMovieScene3DTransformTrack* Track = MovieScene.AddTrack<UMovieScene3DTransformTrack>(Binding);
    UMovieSceneSection* Section = Track ? McpSequenceKeyframes::FindOrAddKeySection(&MovieScene, Track, Start) : nullptr;
    if (Section) // the playback range, grown to hold the whole flight
    {
        Section->SetRange(TRange<FFrameNumber>::Hull(Section->GetRange(), TRange<FFrameNumber>(Start, End + 1)));
    }
    return Cast<UMovieScene3DTransformSection>(Section);
}

// The camera cut to the camera over the flight: its own cut when it has one (a flight flown again keeps one cut), else
// a new one. ponytail: other cameras' cuts stay as they are, so ours may overlap one in an edited sequence; trim them
// (as Sequencer does when a cut is dropped on another) when cutting into edited sequences matters.
UMovieSceneCameraCutSection* CutTo(UMovieScene& MovieScene, const FGuid& Binding, FFrameNumber Start, FFrameNumber End)
{
    UMovieSceneCameraCutTrack* Cuts = Cast<UMovieSceneCameraCutTrack>(MovieScene.GetCameraCutTrack());
    Cuts = Cuts ? Cuts : Cast<UMovieSceneCameraCutTrack>(MovieScene.AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass()));
    if (!Cuts)
    {
        return nullptr;
    }
    UMovieSceneCameraCutSection* Cut = nullptr;
    for (UMovieSceneSection* Section : Cuts->GetAllSections())
    {
        UMovieSceneCameraCutSection* Existing = Cast<UMovieSceneCameraCutSection>(Section);
        Cut = !Cut && Existing && Existing->GetCameraBindingID().GetGuid() == Binding ? Existing : Cut;
    }
    Cut = Cut ? Cut : Cuts->AddNewCameraCut(FMovieSceneObjectBindingID(UE::MovieScene::FRelativeObjectBindingID(Binding)), Start);
    if (Cut)
    {
        Cut->Modify();
        Cut->SetRange(TRange<FFrameNumber>(Start, End));
    }
    return Cut;
}

void KeyFlight(UMovieScene3DTransformSection& Section, const FFlightKeys& Keys, FFrameRate Ticks, FFrameNumber Start)
{
    TArrayView<FMovieSceneDoubleChannel*> Channels = Section.GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
    for (int32 Index = 0; Index < Keys.Locations.Num() && Channels.Num() >= 6; ++Index)
    {
        const FFrameNumber Frame = Start + Ticks.AsFrameNumber(Keys.Times[Index]);
        const FVector& Location = Keys.Locations[Index];
        const FRotator& Rotation = Keys.Rotations[Index];
        // Channel order: location x/y/z, rotation roll/pitch/yaw.
        const double Values[6] = {Location.X, Location.Y, Location.Z, Rotation.Roll, Rotation.Pitch, Rotation.Yaw};
        for (int32 Channel = 0; Channel < 6; ++Channel)
        {
            FMovieSceneDoubleValue Key(Values[Channel]);
            Key.InterpMode = RCIM_Cubic;
            Channels[Channel]->GetData().UpdateOrAddKey(Frame, Key);
        }
    }
    for (int32 Channel = 0; Channel < 6 && Channel < Channels.Num(); ++Channel)
    {
        Channels[Channel]->AutoSetTangents();
    }
}
} // namespace

bool WriteFlight(ULevelSequence& Sequence, FFlyCamera& Camera, const FFlightKeys& Keys, const TSharedPtr<FJsonObject>& Result,
                 FString& OutCode, FString& OutError)
{
    UMovieScene* MovieScene = Sequence.GetMovieScene();
    TArray<UObject*> Touched = {&Sequence, MovieScene};
    if (Camera.Actor)
    {
        Touched.Add(Camera.Actor);
    }
    const FMcpScopedEditorTransaction Transaction(NSLOCTEXT("McpFlyThrough", "FlyThrough", "Fly Through"), EMcpMutationDurability::EditorStateOnly, Touched);
    if (!Transaction.IsRecordingUndo()) // a flight is written only as an undo step
    {
        OutCode = Transaction.GetRefusalCode();
        OutError = Transaction.GetRefusalDetail();
        return false;
    }
    const bool bSpawn = Camera.Actor == nullptr;
    if (bSpawn)
    {
#if MCP_HAS_CINEMATIC_CAMERA
        UClass* const Class = ACineCameraActor::StaticClass();
#else
        UClass* const Class = ACameraActor::StaticClass();
#endif
        Camera.Actor = SpawnActorInActiveWorld<AActor>(Class, Keys.Locations[0], Keys.Rotations[0], Camera.Label);
    }
    const FGuid Had = Camera.Actor ? McpSequenceCinematics::FindExistingBinding(&Sequence, Camera.Actor, Camera.Actor->GetWorld()) : FGuid();
    const FGuid Binding = Had.IsValid() || !Camera.Actor ? Had : McpSequenceCinematics::ResolveOrCreateBinding(&Sequence, Camera.Actor);
    const FFrameRate Ticks = MovieScene->GetTickResolution();
    const FFrameNumber Start = MovieScene->GetPlaybackRange().GetLowerBoundValue();
    const FFrameNumber End = Start + Ticks.AsFrameNumber(Keys.Times.Last());
    bool bReplaced = false;
    UMovieScene3DTransformSection* Section = Binding.IsValid() ? FreshTransformSection(*MovieScene, Binding, Start, End, bReplaced) : nullptr;
    if (!Section) // nothing is left behind: the binding and the camera this call made go again
    {
        if (Binding.IsValid() && !Had.IsValid())
        {
            Sequence.UnbindPossessableObjects(Binding);
            MovieScene->RemovePossessable(Binding);
        }
        if (bSpawn && Camera.Actor)
        {
            Camera.Actor->GetWorld()->EditorDestroyActor(Camera.Actor, true);
            Camera.Actor = nullptr;
        }
        OutCode = TEXT("BIND_FAILED");
        OutError = TEXT("The camera could not be bound to the sequence and keyed; nothing changed.");
        return false;
    }
    KeyFlight(*Section, Keys, Ticks, Start);
    const UMovieSceneCameraCutSection* Cut = CutTo(*MovieScene, Binding, Start, End);
    const bool bGrown = MovieScene->GetPlaybackRange().GetUpperBoundValue() < End;
    if (bGrown)
    {
        MovieScene->SetPlaybackRange(Start, (End - Start).Value);
    }
    MovieScene->Modify();
    Sequence.MarkPackageDirty();
    const FFrameRate Display = MovieScene->GetDisplayRate();
    Result->SetStringField(TEXT("bindingGuid"), Binding.ToString());
    Result->SetNumberField(TEXT("startFrame"), FFrameRate::TransformTime(FFrameTime(Start), Ticks, Display).RoundToFrame().Value);
    Result->SetNumberField(TEXT("endFrame"), FFrameRate::TransformTime(FFrameTime(End), Ticks, Display).RoundToFrame().Value);
    Result->SetBoolField(TEXT("playbackGrown"), bGrown);
    Result->SetBoolField(TEXT("cameraCut"), Cut != nullptr);
    Result->SetBoolField(TEXT("replacedTrack"), bReplaced);
    Transaction.DescribeInto(Result);
    return true;
}
} // namespace McpFlyThrough
