// Copyright (c) 2024 EngineWire Contributors
//
// manage_sequence frame_shot (McpFrameShot.h): the subjects and the camera are found, the lens read, the shot solved
// against the editor world's sight lines, and the camera moved, or spawned, with its lens and focus as one undo step.

#include "Domains/Sequence/Planner/McpFrameShot.h"

#include "Domains/ControlActor/Placement/McpPlacementMount.h"
#include "Domains/Sequence/Planner/McpFrameShotCamera.h"
#include "Editor.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Responses/McpResponses.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "GameFramework/Actor.h"

namespace McpFrameShot
{
namespace
{
const TCHAR* const FrameShotSizes[] = {TEXT("close"), TEXT("medium"), TEXT("wide")};
constexpr double FrameShotFills[] = {0.9, 0.55, 0.25};   // the share of the frame each size fills
constexpr double FrameShotLenses[] = {85.0, 50.0, 24.0}; // and its default focal length, mm
constexpr double FrameShotTwoLens = 35.0;
constexpr double FrameShotMostHidden = 0.5; // a subject hidden past this has no clear shot

// The subjects named, each with its drawn box; OutError names the first problem, OutMissing the names no actor has.
bool ReadSubjects(const TSharedPtr<FJsonObject>& Payload, TFunctionRef<AActor*(const FString&)> Find, TArray<AActor*>& OutActors,
                  TArray<FBox>& OutBoxes, FProblem& Problem, TArray<FString>& OutMissing, FString& OutError)
{
    const TArray<FString> Names = McpHandlerUtils::GetStringArrayField(Payload, TEXT("subjects"));
    if (Names.Num() < 1 || Names.Num() > 2)
    {
        OutError = TEXT("subjects names one or two actors to frame.");
        return false;
    }
    for (const FString& Name : Names)
    {
        AActor* Actor = Find(Name);
        const FBox Box = Actor ? McpPlacement::DrawnBounds(*Actor) : FBox(ForceInit);
        if (!Actor)
        {
            OutMissing.Add(Name);
        }
        else if (!Box.IsValid || OutActors.Contains(Actor))
        {
            OutError = Box.IsValid ? TEXT("subjects names the same actor twice.") : FString::Printf(TEXT("'%s' draws nothing to frame."), *Name);
            return false;
        }
        else
        {
            OutActors.Add(Actor);
            OutBoxes.Add(Box);
            Problem.Subjects.Add({Box.GetCenter(), Box.GetExtent(), Actor->GetActorForwardVector()});
        }
    }
    if (OutMissing.Num() == 0 && Problem.Subjects.Num() == 2 && FVector::Dist(Problem.Subjects[0].Center, Problem.Subjects[1].Center) < 1.0)
    {
        OutError = TEXT("The two subjects stand in one place: frame one of them.");
    }
    return OutMissing.Num() == 0 && OutError.IsEmpty();
}

// Each subject's screen point: framing's, else the third lines (one subject on the left one, two on both).
bool ReadFraming(const TSharedPtr<FJsonObject>& Payload, FProblem& Problem, FString& OutError)
{
    const int32 Count = Problem.Subjects.Num();
    const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
    if (!Payload->TryGetArrayField(TEXT("framing"), Points))
    {
        Problem.Targets.Add(FVector2D(-1.0 / 3.0, 0.0));
        if (Count == 2)
        {
            Problem.Targets.Add(FVector2D(1.0 / 3.0, 0.0));
        }
        return true;
    }
    for (const TSharedPtr<FJsonValue>& Value : *Points)
    {
        const TSharedPtr<FJsonObject>* Point = nullptr;
        double X = 0.0;
        double Y = 0.0;
        if (!Value.IsValid() || !Value->TryGetObject(Point) || !(*Point)->TryGetNumberField(TEXT("x"), X) ||
            !(*Point)->TryGetNumberField(TEXT("y"), Y) || FMath::Abs(X) > 1.0 || FMath::Abs(Y) > 1.0)
        {
            OutError = TEXT("framing lists one {x, y} per subject, each from -1 to 1 (x left to right, y bottom to top).");
            return false;
        }
        Problem.Targets.Add(FVector2D(X, Y));
    }
    if (Problem.Targets.Num() != Count)
    {
        OutError = FString::Printf(TEXT("framing lists %d point(s) for %d subject(s): give one per subject, in order."), Problem.Targets.Num(), Count);
        return false;
    }
    if (Count == 2 && FVector2D::Distance(Problem.Targets[0], Problem.Targets[1]) < 0.05)
    {
        OutError = TEXT("The two subjects' framing points must be apart on screen.");
        return false;
    }
    return true;
}

int32 SizeIndexOf(const FString& Size)
{
    for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(FrameShotSizes)); ++Index)
    {
        if (Size.Equals(FrameShotSizes[Index], ESearchCase::IgnoreCase))
        {
            return Index;
        }
    }
    return INDEX_NONE;
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
    FProblem Problem;
    TArray<AActor*> Subjects;
    TArray<FBox> Boxes;
    TArray<FString> Missing;
    FString Error;
    if (!ReadSubjects(Payload, Find, Subjects, Boxes, Problem, Missing, Error) && Missing.Num() > 0)
    {
        TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
        Details->SetArrayField(TEXT("missing"), McpHandlerUtils::ToJsonStringArray(Missing));
        SendStandardErrorResponse(&Bridge, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                                  FString::Printf(TEXT("No actor is named %s; nothing changed."), *FString::Join(Missing, TEXT(", "))), Details);
        return true;
    }
    FString CameraLabel;
    Payload->TryGetStringField(TEXT("camera"), CameraLabel);
    CameraLabel = CameraLabel.IsEmpty() ? FString(TEXT("ShotCamera")) : CameraLabel;
    AActor* const CameraActor = Find(CameraLabel);
    FString Size = TEXT("medium");
    Payload->TryGetStringField(TEXT("shotSize"), Size);
    const int32 SizeIndex = SizeIndexOf(Size);
    double Asked = 0.0;
    Payload->TryGetNumberField(TEXT("focalLength"), Asked);
    FFrameCamera Camera;
    if (Error.IsEmpty() && SizeIndex == INDEX_NONE)
    {
        Error = TEXT("shotSize is close, medium or wide.");
    }
    if (Error.IsEmpty() && CameraActor && Subjects.Contains(CameraActor))
    {
        Error = TEXT("The camera cannot frame itself: name another camera, or another subject.");
    }
    if (Error.IsEmpty() && ReadFraming(Payload, Problem, Error))
    {
        Problem.Fill = FrameShotFills[SizeIndex];
        ReadCamera(CameraActor, CameraLabel, Asked, Subjects.Num() == 2 ? FrameShotTwoLens : FrameShotLenses[SizeIndex], Camera, Error);
    }
    if (!Error.IsEmpty() || Camera.Lens.TanHalfH <= 0.0 || Camera.Lens.TanHalfV <= 0.0)
    {
        Bridge.SendAutomationError(Socket, RequestId, Error.IsEmpty() ? TEXT("The camera's field of view reads as zero.") : Error, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    Problem.Lens = Camera.Lens;
    const FShot Shot = SolveInWorld(*World, Problem, Subjects, Boxes, CameraActor);
    const TSharedPtr<FJsonObject> Result = DescribeShot(Problem, Shot, Camera, Subjects);
    bool bClear = !Shot.bInside;
    for (const double Hidden : Shot.Occluded)
    {
        bClear &= Hidden <= FrameShotMostHidden;
    }
    if (!bClear)
    {
        SendStandardErrorResponse(&Bridge, Socket, RequestId, TEXT("NO_CLEAR_SHOT"),
                                  Shot.bInside ? TEXT("Every place found for the camera sits in something solid; nothing changed.")
                                               : TEXT("Something stands between every place found for the camera and a subject (over half of it "
                                                      "hidden); nothing changed. Move what is in the way, or frame the subjects from another side."),
                                  Result);
        return true;
    }
    bool bDryRun = false;
    Payload->TryGetBoolField(TEXT("dryRun"), bDryRun);
    const bool bSpawn = Camera.Actor == nullptr;
    FString Code;
    if (!bDryRun && !WriteShot(Camera, Shot, FVector::Dist(Shot.Location, Problem.Subjects[0].Center), Result, Code, Error))
    {
        Bridge.SendAutomationError(Socket, RequestId, Error, Code);
        return true;
    }
    Result->SetBoolField(TEXT("dryRun"), bDryRun);
    Bridge.SendAutomationResponse(Socket, RequestId, true, ShotMessage(Problem, Shot, Camera, Subjects, bSpawn, bDryRun), Result);
    return true;
}
} // namespace McpFrameShot
