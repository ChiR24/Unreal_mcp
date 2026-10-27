// Copyright (c) 2024 EngineWire Contributors
//
// manage_sequence fly_through (McpFlyThrough.h): the points read, the flight planned and smoothed against the editor
// world's collision, sampled into keys (facing lookAt, else the way ahead), and written by McpFlyThroughKeys.

#include "Domains/Sequence/Planner/McpFlyThrough.h"

#include "Camera/CameraComponent.h"
#include "Domains/ControlActor/Placement/McpPlacementMount.h"
#include "Domains/Sequence/Cinematics/McpSeqCine.h"
#include "Domains/Sequence/Planner/McpFlyThroughKeys.h"
#include "Editor.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Responses/McpResponses.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "LevelSequence.h"

namespace McpFlyThrough
{
namespace
{
constexpr int32 FlyMaxPoints = 16;
constexpr int32 FlyMaxCells = 100000; // grid cells A* may expand, all legs together
constexpr double FlyDeadline = 20.0;  // seconds the planning may take: the reply comes well inside the client's 30 s
constexpr double FlyKeysPerSecond = 10.0;
constexpr double FlySpeed = 300.0;   // cm a second when seconds is left out
constexpr double FlyLookAhead = 0.5; // seconds ahead the camera faces when nothing is to be looked at

bool FlyReadThrough(const TSharedPtr<FJsonObject>& Payload, TArray<FVector>& Out, FString& OutError)
{
    const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
    if (!Payload->TryGetArrayField(TEXT("through"), Points) || Points->Num() < 2 || Points->Num() > FlyMaxPoints)
    {
        OutError = FString::Printf(TEXT("through lists 2 to %d points as {x, y, z} in cm."), FlyMaxPoints);
        return false;
    }
    for (const TSharedPtr<FJsonValue>& Value : *Points)
    {
        const TSharedPtr<FJsonObject>* Point = nullptr;
        FVector Location = FVector::ZeroVector;
        if (!Value.IsValid() || !Value->TryGetObject(Point) || !(*Point)->TryGetNumberField(TEXT("x"), Location.X) ||
            !(*Point)->TryGetNumberField(TEXT("y"), Location.Y) || !(*Point)->TryGetNumberField(TEXT("z"), Location.Z))
        {
            OutError = TEXT("Each point of through is {x, y, z} in cm.");
            return false;
        }
        if (Out.Num() > 0 && FVector::Dist(Out.Last(), Location) < 1.0)
        {
            OutError = FString::Printf(TEXT("through[%d] repeats the point before it."), Out.Num());
            return false;
        }
        Out.Add(Location);
    }
    return true;
}

// Keys every tenth of a second, facing Target, else the way ahead; yaw carried on so no key spins round a full turn.
FFlightKeys FlySampleKeys(const TArray<McpMinSnap::FLeg>& Legs, double Seconds, const TOptional<FVector>& Target)
{
    FFlightKeys Keys;
    const int32 Count = FMath::Max(2, FMath::CeilToInt(Seconds * FlyKeysPerSecond) + 1);
    FRotator Last = FRotator::ZeroRotator;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const double T = Seconds * Index / (Count - 1);
        const FVector Here = Position(Legs, T);
        const FVector Toward = (Target.IsSet() ? Target.GetValue() : Position(Legs, FMath::Min(T + FlyLookAhead, Seconds))) - Here;
        FRotator Turn = Toward.SizeSquared() > 1.0 ? Toward.Rotation() : Last; // at rest at the end it keeps its heading
        Turn.Roll = 0.0;
        Turn.Yaw = Index > 0 ? Last.Yaw + FMath::FindDeltaAngleDegrees(Last.Yaw, Turn.Yaw) : Turn.Yaw;
        Keys.Times.Add(T);
        Keys.Locations.Add(Here);
        Keys.Rotations.Add(Turn);
        Last = Turn;
    }
    return Keys;
}

double FlyPathLength(const TArray<FVector>& Points)
{
    double Length = 0.0;
    for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
    {
        Length += FVector::Dist(Points[Index], Points[Index + 1]);
    }
    return Length;
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
        FString Message;
        FString Code = TEXT("INVALID_SEQUENCE");
        if (Failure.IsValid())
        {
            Failure->TryGetStringField(TEXT("error"), Message);
            Failure->TryGetStringField(TEXT("errorCode"), Code);
        }
        Bridge.SendAutomationError(Socket, RequestId, Message.IsEmpty() ? TEXT("sequencePath names no Level Sequence.") : Message, Code);
        return true;
    }
    TArray<FVector> Through;
    FString Error;
    double Clearance = 50.0;
    double Seconds = 0.0;
    FString LookAt;
    FFlyCamera Camera;
    Payload->TryGetNumberField(TEXT("clearanceCm"), Clearance);
    Payload->TryGetNumberField(TEXT("seconds"), Seconds);
    Payload->TryGetStringField(TEXT("lookAt"), LookAt);
    Payload->TryGetStringField(TEXT("camera"), Camera.Label);
    Camera.Label = Camera.Label.IsEmpty() ? FString(TEXT("FlyCamera")) : Camera.Label;
    Camera.Actor = Find(Camera.Label);
    AActor* const Target = LookAt.IsEmpty() ? nullptr : Find(LookAt);
    if (!LookAt.IsEmpty() && !Target)
    {
        SendStandardErrorResponse(&Bridge, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"), FString::Printf(TEXT("No actor is named %s; nothing changed."), *LookAt));
        return true;
    }
    if (FlyReadThrough(Payload, Through, Error) && (Clearance < 10.0 || Clearance > 500.0))
    {
        Error = TEXT("clearanceCm is 10 to 500.");
    }
    if (Error.IsEmpty() && Seconds != 0.0 && (Seconds < 0.5 || Seconds > 600.0))
    {
        Error = TEXT("seconds is 0.5 to 600.");
    }
    if (Error.IsEmpty() && Camera.Actor && !Camera.Actor->FindComponentByClass<UCameraComponent>())
    {
        Error = FString::Printf(TEXT("'%s' is no camera: name a camera actor, or a new name to spawn a CineCameraActor under."), *Camera.Label);
    }
    if (!Error.IsEmpty())
    {
        Bridge.SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    FSpace Space;
    Space.Radius = Clearance;
    const FCollisionQueryParams Params(SCENE_QUERY_STAT(McpFlyThrough), false, Camera.Actor);
    Space.Free = [World, &Params](const FVector& Point, double Radius)
    { return !World->OverlapBlockingTestByChannel(Point, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(Radius), Params); };
    Space.Clear = [World, &Params](const FVector& From, const FVector& To, double Radius)
    { return !World->SweepTestByChannel(From, To, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(Radius), Params); };
    FSearchBudget Budget;
    Budget.Cells = FlyMaxCells;
    Budget.Deadline = FPlatformTime::Seconds() + FlyDeadline;
    FPlan Plan = PlanCorners(Space, Through, Budget);
    Seconds = Seconds > 0.0 ? Seconds : FMath::Max(1.0, FlyPathLength(Plan.Points) / FlySpeed);
    TArray<McpMinSnap::FLeg> Legs;
    if (!Plan.Error.IsEmpty() || !Fly(Space, Plan, Seconds, Budget, Legs, Plan.Error))
    {
        TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
        Details->SetNumberField(TEXT("point"), Plan.FailedPoint);
        SendStandardErrorResponse(&Bridge, Socket, RequestId, Plan.bInSolid ? TEXT("POINT_IN_SOLID") : TEXT("NO_PATH"), Plan.Error + TEXT(" Nothing changed."), Details);
        return true;
    }
    TOptional<FVector> Facing;
    if (Target)
    {
        const FBox Drawn = McpPlacement::DrawnBounds(*Target);
        Facing = Drawn.IsValid ? Drawn.GetCenter() : Target->GetActorLocation();
    }
    const FFlightKeys Keys = FlySampleKeys(Legs, Seconds, Facing);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> Corners;
    for (int32 Index = 0; Index < Plan.Points.Num() && Index < 64; ++Index)
    {
        const FVector& Corner = Plan.Points[Index];
        const FVector Rounded(FMath::RoundToDouble(Corner.X * 10.0) / 10.0, FMath::RoundToDouble(Corner.Y * 10.0) / 10.0, FMath::RoundToDouble(Corner.Z * 10.0) / 10.0);
        Corners.Add(MakeShared<FJsonValueArray>(McpHandlerUtils::VectorToJsonArray(Rounded)));
    }
    const double Length = FlyPathLength(Keys.Locations);
    const bool bSpawn = Camera.Actor == nullptr;
    Result->SetStringField(TEXT("camera"), Camera.Label);
    Result->SetBoolField(TEXT("spawned"), bSpawn);
    Result->SetStringField(TEXT("sequencePath"), Sequence->GetOutermost()->GetName());
    Result->SetNumberField(TEXT("seconds"), FMath::RoundToDouble(Seconds * 100.0) / 100.0);
    Result->SetNumberField(TEXT("lengthCm"), FMath::RoundToDouble(Length));
    Result->SetNumberField(TEXT("keys"), Keys.Times.Num());
    Result->SetArrayField(TEXT("corners"), Corners);
    bool bDryRun = false;
    Payload->TryGetBoolField(TEXT("dryRun"), bDryRun);
    FString Code;
    if (!bDryRun && !WriteFlight(*Sequence, Camera, Keys, Result, Code, Error))
    {
        Bridge.SendAutomationError(Socket, RequestId, Error, Code);
        return true;
    }
    Result->SetBoolField(TEXT("dryRun"), bDryRun);
    const FString Verb = bDryRun ? TEXT("Would fly") : TEXT("Flew");
    Bridge.SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("%s %s through %d point(s) in %.1f s along %.0f cm, %d corner(s), never nearer to geometry than %.0f cm; %d key(s) in %s."),
                        *Verb, *Camera.Label, Through.Num(), Seconds, Length, Plan.Points.Num(), Clearance * 0.8, Keys.Times.Num(), *Sequence->GetName()),
        Result);
    return true;
}
} // namespace McpFlyThrough
