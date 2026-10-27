// Copyright (c) 2024 EngineWire Contributors
//
// auto_edit's reply and write (McpAutoEditWrite.h): the shots the edit holds, and the camera cut track rewritten with
// one cut per shot over the playback range, cameras bound when they are not, as one undo step.

#include "Domains/Sequence/Planner/McpAutoEditWrite.h"

#include "Domains/Sequence/Cinematics/McpSeqCine.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "GameFramework/Actor.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneObjectBindingID.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Tracks/MovieSceneCameraCutTrack.h"

namespace McpAutoEdit
{
namespace
{
struct FEditShot
{
    int32 Camera = 0;
    int32 First = 0;
    int32 Last = 0;
};

TArray<FEditShot> EditShots(const TArray<int32>& Edit)
{
    TArray<FEditShot> Shots;
    for (int32 Segment = 0; Segment < Edit.Num(); ++Segment)
    {
        if (Shots.Num() == 0 || Shots.Last().Camera != Edit[Segment])
        {
            Shots.Add({Edit[Segment], Segment, Segment});
        }
        Shots.Last().Last = Segment;
    }
    return Shots;
}

FFrameNumber EditShotEnd(const FEditPlan& Plan, const TArray<int32>& Edit, const FEditShot& Shot)
{
    return Shot.Last + 1 == Edit.Num() ? Plan.End : Plan.SegmentStart(Shot.Last + 1);
}

double EditDisplayFrame(const FEditPlan& Plan, FFrameNumber Tick)
{
    return FMath::RoundToDouble(FFrameRate::TransformTime(FFrameTime(Tick), Plan.Ticks, Plan.Display).AsDecimal());
}

// Cuts across the line between the first two subjects in the edit (ScoreCut's crossing term, counted).
int32 EditCrossings(const FEditPlan& Plan, const TArray<FEditShot>& Shots)
{
    int32 Crossings = 0;
    for (int32 Index = 1; Index < Shots.Num(); ++Index)
    {
        const int32 At = Shots[Index].First;
        if (Plan.Second[At].IsSet())
        {
            const FVector Line = Plan.Second[At].GetValue() - Plan.Main[At];
            const double Before = FVector::CrossProduct(Line, Plan.Poses[At][Shots[Index - 1].Camera].GetLocation() - Plan.Main[At]).Z;
            const double After = FVector::CrossProduct(Line, Plan.Poses[At][Shots[Index].Camera].GetLocation() - Plan.Main[At]).Z;
            Crossings += Before * After < 0.0 ? 1 : 0;
        }
    }
    return Crossings;
}
} // namespace

TSharedPtr<FJsonObject> DescribeEdit(ULevelSequence& Sequence, const TArray<AActor*>& Cameras, const FEditPlan& Plan, const TArray<int32>& Edit)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    const TArray<FEditShot> Shots = EditShots(Edit);
    TArray<TSharedPtr<FJsonValue>> Rows;
    TArray<FString> Used;
    for (const FEditShot& Shot : Shots)
    {
        Used.AddUnique(Cameras[Shot.Camera]->GetActorLabel());
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("camera"), Cameras[Shot.Camera]->GetActorLabel());
        Row->SetNumberField(TEXT("startFrame"), EditDisplayFrame(Plan, Plan.SegmentStart(Shot.First)));
        Row->SetNumberField(TEXT("endFrame"), EditDisplayFrame(Plan, EditShotEnd(Plan, Edit, Shot)));
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Result->SetStringField(TEXT("sequencePath"), Sequence.GetOutermost()->GetName());
    Result->SetArrayField(TEXT("shots"), Rows);
    Result->SetArrayField(TEXT("camerasUsed"), McpHandlerUtils::ToJsonStringArray(Used));
    Result->SetNumberField(TEXT("cuts"), FMath::Max(Shots.Num() - 1, 0));
    Result->SetNumberField(TEXT("crossings"), EditCrossings(Plan, Shots));
    Result->SetNumberField(TEXT("segments"), Plan.Segments);
    return Result;
}

bool WriteEdit(ULevelSequence& Sequence, const TArray<AActor*>& Cameras, const FEditPlan& Plan, const TArray<int32>& Edit,
               const TSharedPtr<FJsonObject>& Result, FString& OutCode, FString& OutError)
{
    UMovieScene* MovieScene = Sequence.GetMovieScene();
    UMovieSceneCameraCutTrack* Cuts = Cast<UMovieSceneCameraCutTrack>(MovieScene->GetCameraCutTrack());
    TArray<UObject*> Touched = {&Sequence, MovieScene};
    if (Cuts)
    {
        Touched.Add(Cuts);
    }
    const FMcpScopedEditorTransaction Transaction(NSLOCTEXT("McpAutoEdit", "AutoEdit", "Auto Edit"), EMcpMutationDurability::EditorStateOnly, Touched);
    if (!Transaction.IsRecordingUndo()) // an edit is written only as an undo step
    {
        OutCode = Transaction.GetRefusalCode();
        OutError = Transaction.GetRefusalDetail();
        return false;
    }
    Cuts = Cuts ? Cuts : Cast<UMovieSceneCameraCutTrack>(MovieScene->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass()));
    if (!Cuts)
    {
        OutCode = TEXT("CUT_TRACK_FAILED");
        OutError = TEXT("The sequence would not take a camera cut track; nothing changed.");
        return false;
    }
    const TArray<UMovieSceneSection*> Old = Cuts->GetAllSections(); // the edit owns the range's cuts
    for (UMovieSceneSection* Section : Old)
    {
        Cuts->RemoveSection(*Section);
    }
    for (const FEditShot& Shot : EditShots(Edit))
    {
        const FGuid Binding = McpSequenceCinematics::ResolveOrCreateBinding(&Sequence, Cameras[Shot.Camera]);
        const FFrameNumber From = Plan.SegmentStart(Shot.First);
        UMovieSceneCameraCutSection* Cut = Binding.IsValid()
            ? Cuts->AddNewCameraCut(FMovieSceneObjectBindingID(UE::MovieScene::FRelativeObjectBindingID(Binding)), From)
            : nullptr;
        if (Cut)
        {
            Cut->SetRange(TRange<FFrameNumber>(From, EditShotEnd(Plan, Edit, Shot)));
        }
    }
    MovieScene->Modify();
    Sequence.MarkPackageDirty();
    Transaction.DescribeInto(Result);
    return true;
}

FString EditMessage(const TArray<AActor*>& Cameras, const FEditPlan& Plan, const TArray<int32>& Edit, bool bDryRun)
{
    const TArray<FEditShot> Shots = EditShots(Edit);
    TSet<int32> Used;
    for (const FEditShot& Shot : Shots)
    {
        Used.Add(Shot.Camera);
    }
    const int32 Crossings = EditCrossings(Plan, Shots);
    return FString::Printf(TEXT("%s %d shot(s) from %d of %d camera(s) over %.1f s (shots %.1f to %.1f s); %s."),
                           bDryRun ? TEXT("Would cut") : TEXT("Cut"), Shots.Num(), Used.Num(), Cameras.Num(),
                           Plan.Segments * Plan.SegmentSeconds, Plan.MinRun * Plan.SegmentSeconds, Plan.MaxRun * Plan.SegmentSeconds,
                           Crossings == 0 ? TEXT("no cut crosses the line") : *FString::Printf(TEXT("%d cut(s) cross the line"), Crossings));
}
} // namespace McpAutoEdit
