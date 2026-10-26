// Copyright (c) 2024 EngineWire Contributors
//
// manage_sequence auto_edit (McpAutoEdit.h): the cameras and subjects read as the sequence plays them, every segment
// scored per camera, the cheapest edit found (Cut), and the camera cut track rewritten with one cut per shot.

#include "Domains/Sequence/Planner/McpAutoEdit.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Domains/ControlActor/Placement/McpPlacementMount.h"
#include "Domains/Sequence/Cinematics/McpSeqCine.h"
#include "Domains/Sequence/Planner/McpAutoEditPose.h"
#include "Domains/Sequence/Planner/McpAutoEditScore.h"
#include "Domains/Sequence/Planner/McpAutoEditWrite.h"
#include "Editor.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Responses/McpResponses.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "GameFramework/Actor.h"
#include "LevelSequence.h"
#include "MovieScene.h"

namespace McpAutoEdit
{
namespace
{
constexpr int32 EditMaxCameras = 12;
constexpr int32 EditMaxSubjects = 4;
constexpr int32 EditMaxSegments = 2400;

bool EditReadActors(const TSharedPtr<FJsonObject>& Payload, const TCHAR* Field, int32 Most, TFunctionRef<AActor*(const FString&)> Find,
                    TArray<AActor*>& Out, TArray<FString>& OutMissing)
{
    const TArray<FString> Names = McpHandlerUtils::GetStringArrayField(Payload, Field);
    for (const FString& Name : Names)
    {
        AActor* Actor = Find(Name);
        if (Actor)
        {
            Out.AddUnique(Actor);
        }
        else
        {
            OutMissing.Add(Name);
        }
    }
    return Names.Num() > 0 && Names.Num() <= Most;
}

// The frame a camera gives as the engine would draw it now: a cine camera's from its lens and filmback.
McpFrameShot::FLens EditLensOf(AActor& Camera)
{
    McpFrameShot::FLens Lens;
    UCameraComponent* Component = Camera.FindComponentByClass<UCameraComponent>();
    FMinimalViewInfo View;
    Component->GetCameraView(0.f, View);
    Lens.TanHalfH = FMath::Tan(FMath::DegreesToRadians(static_cast<double>(View.FOV)) / 2.0);
    Lens.TanHalfV = Lens.TanHalfH / FMath::Max(static_cast<double>(View.AspectRatio), 0.1);
    return Lens;
}
} // namespace

bool Handle(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket, TFunctionRef<AActor*(const FString&)> Find)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld)
    {
        Bridge.SendAutomationError(Socket, RequestId, TEXT("Needs the editor world with Play In Editor stopped."), TEXT("NO_WORLD"));
        return true;
    }
    TSharedPtr<FJsonObject> Failure;
    ULevelSequence* Sequence = McpSequenceCinematics::LoadSequence(Payload, Failure);
    if (!Sequence)
    {
        FString Message = TEXT("sequencePath names no Level Sequence.");
        FString Code = TEXT("INVALID_SEQUENCE");
        if (Failure.IsValid())
        {
            Failure->TryGetStringField(TEXT("error"), Message);
            Failure->TryGetStringField(TEXT("errorCode"), Code);
        }
        Bridge.SendAutomationError(Socket, RequestId, Message, Code);
        return true;
    }
    TArray<AActor*> Cameras;
    TArray<AActor*> Subjects;
    TArray<FString> Missing;
    const bool bCameras = EditReadActors(Payload, TEXT("cameras"), EditMaxCameras, Find, Cameras, Missing);
    const bool bSubjects = EditReadActors(Payload, TEXT("subjects"), EditMaxSubjects, Find, Subjects, Missing);
    if (Missing.Num() > 0)
    {
        TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
        Details->SetArrayField(TEXT("missing"), McpHandlerUtils::ToJsonStringArray(Missing));
        SendStandardErrorResponse(&Bridge, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                                  FString::Printf(TEXT("No actor is named %s; nothing changed."), *FString::Join(Missing, TEXT(", "))), Details);
        return true;
    }
    FString Error = !bCameras || Cameras.Num() < 2 ? FString::Printf(TEXT("cameras names 2 to %d camera actors."), EditMaxCameras)
                  : !bSubjects                     ? FString::Printf(TEXT("subjects names 1 to %d actors."), EditMaxSubjects)
                                                   : FString();
    for (AActor* Camera : Cameras)
    {
        Error = Error.IsEmpty() && !Camera->FindComponentByClass<UCameraComponent>() ? FString::Printf(TEXT("'%s' is no camera."), *Camera->GetActorLabel()) : Error;
    }
    FEditPlan Plan;
    Error = Error.IsEmpty() ? ReadTiming(Payload, *Sequence, Plan) : Error;
    if (!Error.IsEmpty())
    {
        Bridge.SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    // ponytail: poses are sampled at each segment's middle only; a camera whipping past mid-segment is judged there.
    ScoreSegments(*World, *Sequence, Cameras, Subjects, [](AActor& Camera) { return EditLensOf(Camera); }, Plan);
    if (Plan.Unseen.Num() > 0)
    {
        TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
        Details->SetArrayField(TEXT("unseenSeconds"), Plan.UnseenJson());
        SendStandardErrorResponse(&Bridge, Socket, RequestId, TEXT("NO_CUT"),
                                  FString::Printf(TEXT("No camera sees %s at %d moment(s) (unseenSeconds): add a camera there; nothing changed."),
                                                  *Subjects[0]->GetActorLabel(), Plan.Unseen.Num()),
                                  Details);
        return true;
    }
    const TArray<int32> Edit = Cut(Plan.Shot, [&Plan](int32 Segment, int32 From, int32 To) { return Plan.CutCost(Segment, From, To); }, Plan.MinRun, Plan.MaxRun);
    bool bDryRun = false;
    Payload->TryGetBoolField(TEXT("dryRun"), bDryRun);
    TSharedPtr<FJsonObject> Result = DescribeEdit(*Sequence, Cameras, Plan, Edit);
    FString Code;
    if (Edit.Num() == 0 || (!bDryRun && !WriteEdit(*Sequence, Cameras, Plan, Edit, Result, Code, Error)))
    {
        Bridge.SendAutomationError(Socket, RequestId, Edit.Num() == 0 ? TEXT("The range is shorter than the shortest shot allowed.") : Error,
                                   Edit.Num() == 0 ? TEXT("INVALID_ARGUMENT") : Code);
        return true;
    }
    Result->SetBoolField(TEXT("dryRun"), bDryRun);
    Bridge.SendAutomationResponse(Socket, RequestId, true, EditMessage(Cameras, Plan, Edit, bDryRun), Result);
    return true;
}
} // namespace McpAutoEdit
