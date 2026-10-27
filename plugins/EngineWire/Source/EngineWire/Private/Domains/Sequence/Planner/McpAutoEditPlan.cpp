// Copyright (c) 2024 EngineWire Contributors
//
// auto_edit's plan (McpAutoEditWrite.h): the playback range cut into segments, and every camera's shot of the subjects
// scored at each segment's middle as the sequence plays them.

#include "Domains/ControlActor/Placement/McpPlacementMount.h"
#include "Domains/Sequence/Planner/McpAutoEditPose.h"
#include "Domains/Sequence/Planner/McpAutoEditScore.h"
#include "Domains/Sequence/Planner/McpAutoEditWrite.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "LevelSequence.h"
#include "MovieScene.h"

namespace McpAutoEdit
{
namespace
{
constexpr int32 EditMostSegments = 2400;
} // namespace

double FEditPlan::CutCost(int32 Segment, int32 From, int32 To) const
{
    return ScoreCut(Poses[Segment][From], Poses[Segment][To], Main[Segment], Second[Segment]);
}

TArray<TSharedPtr<FJsonValue>> FEditPlan::UnseenJson() const
{
    TArray<TSharedPtr<FJsonValue>> Out;
    for (int32 Index = 0; Index < Unseen.Num() && Index < 20; ++Index)
    {
        Out.Add(MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Unseen[Index] * 100.0) / 100.0));
    }
    return Out;
}

FFrameNumber FEditPlan::SegmentStart(int32 Segment) const
{
    return Start + Ticks.AsFrameNumber(Segment * SegmentSeconds);
}

FString ReadTiming(const TSharedPtr<FJsonObject>& Payload, ULevelSequence& Sequence, FEditPlan& Plan)
{
    double Shortest = 1.5;
    double Longest = 8.0;
    Payload->TryGetNumberField(TEXT("segmentSeconds"), Plan.SegmentSeconds);
    Payload->TryGetNumberField(TEXT("minShotSeconds"), Shortest);
    Payload->TryGetNumberField(TEXT("maxShotSeconds"), Longest);
    if (Plan.SegmentSeconds < 0.1 || Plan.SegmentSeconds > 5.0 || Shortest < Plan.SegmentSeconds || Longest < Shortest)
    {
        return TEXT("segmentSeconds is 0.1 to 5, minShotSeconds at least segmentSeconds, maxShotSeconds at least minShotSeconds.");
    }
    UMovieScene* MovieScene = Sequence.GetMovieScene();
    Plan.Ticks = MovieScene->GetTickResolution();
    Plan.Display = MovieScene->GetDisplayRate();
    Plan.Start = MovieScene->GetPlaybackRange().GetLowerBoundValue();
    Plan.End = MovieScene->GetPlaybackRange().GetUpperBoundValue();
    const double Seconds = Plan.Ticks.AsSeconds(FFrameTime(Plan.End - Plan.Start));
    Plan.Segments = FMath::Min(FMath::CeilToInt(Seconds / Plan.SegmentSeconds), EditMostSegments);
    Plan.MinRun = FMath::Max(1, FMath::CeilToInt(Shortest / Plan.SegmentSeconds - 1e-6));
    Plan.MaxRun = FMath::Max(Plan.MinRun, FMath::FloorToInt(Longest / Plan.SegmentSeconds + 1e-6));
    return Plan.Segments < 1 ? FString(TEXT("The sequence's playback range is empty.")) : FString();
}

void ScoreSegments(const UWorld& World, ULevelSequence& Sequence, const TArray<AActor*>& Cameras, const TArray<AActor*>& Subjects,
                   TFunctionRef<McpFrameShot::FLens(AActor&)> LensOf, FEditPlan& Plan)
{
    TArray<FPoseTrack> CameraTracks;
    TArray<McpFrameShot::FLens> Lenses;
    for (AActor* Camera : Cameras)
    {
        CameraTracks.Add(TrackOf(Sequence, *Camera));
        Lenses.Add(LensOf(*Camera));
    }
    TArray<FPoseTrack> SubjectTracks;
    TArray<FBox> Boxes;
    TArray<FCollisionQueryParams> Sight;
    for (AActor* Subject : Subjects)
    {
        SubjectTracks.Add(TrackOf(Sequence, *Subject));
        const FBox Drawn = McpPlacement::DrawnBounds(*Subject);
        Boxes.Add(Drawn.IsValid ? Drawn : FBox::BuildAABB(Subject->GetActorLocation(), FVector(50.0)));
        FCollisionQueryParams& Params = Sight.Add_GetRef(FCollisionQueryParams(SCENE_QUERY_STAT(McpAutoEditSight), true, Subject));
        TArray<AActor*> Attached;
        Subject->GetAttachedActors(Attached);
        Params.AddIgnoredActors(Attached);
        Params.AddIgnoredActors(Cameras);
    }
    // ponytail: a subject's box only follows the sequence's translation of it, not its turn; turn it when subjects
    // spin in place under cameras close enough for it to matter.
    for (int32 Segment = 0; Segment < Plan.Segments; ++Segment)
    {
        const double Seconds = (Segment + 0.5) * Plan.SegmentSeconds;
        const FFrameTime Time = FFrameTime(Plan.Start) + Plan.Ticks.AsFrameTime(Seconds);
        TArray<FSubjectAt> At;
        for (int32 Index = 0; Index < Subjects.Num(); ++Index)
        {
            const FVector Shift = PoseAt(SubjectTracks[Index], Time).GetLocation() - SubjectTracks[Index].Rest.GetLocation();
            At.Add({Boxes[Index].ShiftBy(Shift), &Sight[Index]});
        }
        Plan.Main.Add(At[0].Box.GetCenter());
        Plan.Second.Add(At.Num() > 1 ? TOptional<FVector>(At[1].Box.GetCenter()) : TOptional<FVector>());
        TArray<double>& Row = Plan.Shot.AddDefaulted_GetRef();
        TArray<FTransform>& Poses = Plan.Poses.AddDefaulted_GetRef();
        bool bSeen = false;
        for (int32 Index = 0; Index < Cameras.Num(); ++Index)
        {
            Poses.Add(PoseAt(CameraTracks[Index], Time));
            const FShotScore Score = ScoreShot(World, Poses.Last(), Lenses[Index], At);
            Row.Add(Score.Cost);
            bSeen |= Score.bMainSeen;
        }
        if (!bSeen)
        {
            Plan.Unseen.Add(Seconds);
        }
    }
}
} // namespace McpAutoEdit
