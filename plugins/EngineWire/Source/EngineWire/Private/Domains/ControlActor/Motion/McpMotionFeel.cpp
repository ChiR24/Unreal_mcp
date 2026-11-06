// Copyright (c) 2024 EngineWire Contributors

#include "Domains/ControlActor/Motion/McpMotionFeel.h"

#include "Camera/PlayerCameraManager.h"
#include "Domains/ControlActor/McpActorSupport.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"

namespace
{
// Slower than this is standing still; a jump rises at least FeelLiftCm above where it left; a run reaches FeelRunCmS.
constexpr double FeelStillCmS = 10.0;
constexpr double FeelLiftCm = 20.0;
constexpr double FeelRunCmS = 30.0;

double FeelRound(double Value, double Scale)
{
    return FMath::RoundToDouble(Value * Scale) / Scale;
}

// The rate of change of a series, over the frame ending at each sample (the first copies 0).
TArray<FVector> FeelRate(const TArray<double>& Times, const TArray<FVector>& Series)
{
    TArray<FVector> Out;
    Out.Add(FVector::ZeroVector);
    for (int32 Index = 1; Index < Series.Num(); ++Index)
    {
        const double Dt = Times[Index] - Times[Index - 1];
        Out.Add(Dt > 0.0 ? (Series[Index] - Series[Index - 1]) / Dt : Out.Last());
    }
    return Out;
}

TSharedPtr<FJsonObject> FeelJump(const TArray<double>& T, const TArray<FVector>& P, const TArray<FVector>& V)
{
    int32 Apex = 0;
    for (int32 Index = 1; Index < P.Num(); ++Index)
    {
        Apex = P[Index].Z > P[Apex].Z ? Index : Apex;
    }
    int32 Rise = 1; // the first frame rising; it left the ground on the frame before
    while (Rise < Apex && V[Rise].Z <= FeelStillCmS)
    {
        ++Rise;
    }
    const int32 Takeoff = Rise - 1;
    if (Apex == 0 || P[Apex].Z - P[Takeoff].Z < FeelLiftCm)
    {
        return nullptr;
    }
    TSharedPtr<FJsonObject> Jump = MakeShared<FJsonObject>();
    const double Up = T[Apex] - T[Takeoff];
    Jump->SetNumberField(TEXT("takeoffAt"), FeelRound(T[Takeoff], 1000.0));
    Jump->SetNumberField(TEXT("height"), FeelRound(P[Apex].Z - P[Takeoff].Z, 10.0));
    Jump->SetNumberField(TEXT("timeToApex"), FeelRound(Up, 1000.0));
    // Landed: it fell, then stopped falling, on the floor it left or on any other.
    int32 Land = Apex + 1;
    while (Land < P.Num() && V[Land].Z >= -FeelStillCmS)
    {
        ++Land;
    }
    double Fastest = 0.0;
    while (Land < P.Num() && V[Land].Z < -FeelStillCmS)
    {
        Fastest = FMath::Max(Fastest, -V[Land].Z);
        ++Land;
    }
    Jump->SetBoolField(TEXT("landed"), Land < P.Num());
    if (Land < P.Num())
    {
        Jump->SetNumberField(TEXT("airTime"), FeelRound(T[Land - 1] - T[Takeoff], 1000.0));
        Jump->SetNumberField(TEXT("fallOverRise"), FeelRound(Up > 0.0 ? (T[Land - 1] - T[Apex]) / Up : 0.0, 100.0));
        Jump->SetNumberField(TEXT("landingSpeed"), FeelRound(Fastest, 1.0));
    }
    return Jump;
}

TSharedPtr<FJsonObject> FeelRun(const TArray<double>& T, const TArray<FVector>& P, const TArray<FVector>& V, double Released)
{
    int32 Top = 0;
    for (int32 Index = 1; Index < V.Num(); ++Index)
    {
        Top = V[Index].Size2D() > V[Top].Size2D() ? Index : Top;
    }
    const double TopSpeed = V[Top].Size2D();
    if (TopSpeed < FeelRunCmS)
    {
        return nullptr;
    }
    int32 Start = 1; // the first frame moving across the ground
    while (Start < Top && V[Start].Size2D() <= FeelStillCmS)
    {
        ++Start;
    }
    int32 Reach = Start;
    while (Reach < Top && V[Reach].Size2D() < 0.9 * TopSpeed)
    {
        ++Reach;
    }
    TSharedPtr<FJsonObject> Run = MakeShared<FJsonObject>();
    Run->SetNumberField(TEXT("topSpeed"), FeelRound(TopSpeed, 1.0));
    Run->SetNumberField(TEXT("timeTo90"), FeelRound(T[Reach] - T[Start - 1], 1000.0));
    int32 From = 0; // the first frame after the last key went up
    while (From < T.Num() && T[From] < Released)
    {
        ++From;
    }
    int32 Stop = From;
    while (Stop < V.Num() && V[Stop].Size2D() >= FeelStillCmS)
    {
        ++Stop;
    }
    if (Released >= 0.0 && From > 0 && Stop < V.Num())
    {
        // From where the pawn stood when the key went up: the frame stamped with that time (the sampler releases a key
        // after reading its frame), else the one before, as a key's latency counts from the frame it went down in.
        const int32 At = From < T.Num() && T[From] <= Released ? From : From - 1;
        Run->SetNumberField(TEXT("stopTime"), FeelRound(T[Stop] - Released, 1000.0));
        Run->SetNumberField(TEXT("stopDistance"), FeelRound(FVector::Dist2D(P[Stop], P[At]), 1.0));
    }
    return Run;
}

// Each key: from going down to the first frame whose motion differs from the frame it went down in.
TArray<TSharedPtr<FJsonValue>> FeelKeys(const TArray<double>& T, const TArray<FVector>& V, const TArray<FMcpFeelPress>& Presses)
{
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (const FMcpFeelPress& Press : Presses)
    {
        if (Press.DownAt < 0.0)
        {
            continue;
        }
        int32 At = 0;
        while (At + 1 < T.Num() && T[At + 1] <= Press.DownAt)
        {
            ++At;
        }
        int32 Moved = At + 1;
        while (Moved < V.Num() && (V[Moved] - V[At]).Size() <= FeelStillCmS)
        {
            ++Moved;
        }
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("key"), Press.Key);
        Row->SetNumberField(TEXT("downAt"), FeelRound(Press.DownAt, 1000.0));
        Row->SetBoolField(TEXT("moved"), Moved < V.Num());
        if (Moved < V.Num())
        {
            Row->SetNumberField(TEXT("latency"), FeelRound(T[Moved] - Press.DownAt, 1000.0));
            Row->SetNumberField(TEXT("latencyFrames"), Moved - At);
        }
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    return Rows;
}

// The camera's jerk (the rate of change of its acceleration), in m/s3: a camera that snaps instead of easing reads high.
TSharedPtr<FJsonObject> FeelCamera(const TArray<double>& T, const TArray<FVector>& C)
{
    if (C.Num() != T.Num() || C.Num() < 5)
    {
        return nullptr;
    }
    const TArray<FVector> Jerk = FeelRate(T, FeelRate(T, FeelRate(T, C)));
    double Peak = 0.0, Sum = 0.0;
    int32 PeakAt = 3;
    for (int32 Index = 3; Index < Jerk.Num(); ++Index) // the first three hold no full difference
    {
        const double Size = Jerk[Index].Size();
        Sum += Size;
        PeakAt = Size > Peak ? Index : PeakAt;
        Peak = FMath::Max(Peak, Size);
    }
    TSharedPtr<FJsonObject> Camera = MakeShared<FJsonObject>();
    Camera->SetNumberField(TEXT("peakJerk"), FeelRound(Peak / 100.0, 1.0));
    Camera->SetNumberField(TEXT("peakJerkAt"), FeelRound(T[PeakAt], 1000.0));
    Camera->SetNumberField(TEXT("meanJerk"), FeelRound(Sum / (Jerk.Num() - 3) / 100.0, 1.0));
    return Camera;
}
} // namespace

TSharedPtr<FJsonObject> McpMotionFeelSummary(const TArray<double>& Times, const TArray<FVector>& Locations,
                                             const TArray<FVector>& Cameras, const TArray<FMcpFeelPress>& Presses)
{
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetNumberField(TEXT("frames"), Times.Num());
    if (Times.Num() < 3 || Locations.Num() != Times.Num())
    {
        return Out;
    }
    Out->SetNumberField(TEXT("frameMs"), FeelRound((Times.Last() - Times[0]) / (Times.Num() - 1) * 1000.0, 10.0));
    const TArray<FVector> Velocities = FeelRate(Times, Locations);
    if (const TSharedPtr<FJsonObject> Jump = FeelJump(Times, Locations, Velocities))
    {
        Out->SetObjectField(TEXT("jump"), Jump);
    }
    double Released = -1.0;
    for (const FMcpFeelPress& Press : Presses)
    {
        Released = FMath::Max(Released, Press.UpAt);
    }
    if (const TSharedPtr<FJsonObject> Run = FeelRun(Times, Locations, Velocities, Released))
    {
        Out->SetObjectField(TEXT("run"), Run);
    }
    const TArray<TSharedPtr<FJsonValue>> Keys = FeelKeys(Times, Velocities, Presses);
    if (Keys.Num() > 0)
    {
        Out->SetArrayField(TEXT("keys"), Keys);
    }
    if (const TSharedPtr<FJsonObject> Camera = FeelCamera(Times, Cameras))
    {
        Out->SetObjectField(TEXT("camera"), Camera);
    }
    return Out;
}

void McpParseMotionFeel(const TSharedPtr<FJsonObject>& Payload, FMcpMotionFeel& Feel, double& Interval)
{
    Payload->TryGetBoolField(TEXT("feel"), Feel.bOn);
    if (Feel.bOn && !Payload->HasField(TEXT("intervalSeconds")))
    {
        Interval = 0.0; // every frame: a key's latency is a frame or two
    }
}

void McpSampleMotionFeel(FMcpMotionFeel& Feel, const AActor* Actor, double Time)
{
    if (!Feel.bOn || !Actor)
    {
        return;
    }
    Feel.Times.Add(Time);
    Feel.Locations.Add(Actor->GetActorLocation());
    const UWorld* World = Actor->GetWorld();
    const APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
    // Kept only while every frame has one, so the camera's frames line up with the actor's.
    if (Player && Player->PlayerCameraManager && Feel.Cameras.Num() == Feel.Times.Num() - 1)
    {
        Feel.Cameras.Add(Player->PlayerCameraManager->GetCameraLocation());
    }
}

void McpAddMotionFeel(const FMcpMotionFeel& Feel, const TArray<FMcpMotionInput>& Inputs, FJsonObject& Data)
{
    if (!Feel.bOn)
    {
        return;
    }
    TArray<FMcpFeelPress> Presses;
    for (const FMcpMotionInput& Input : Inputs)
    {
        Presses.Add({Input.Key, Input.DownAt, Input.UpAt});
    }
    Data.SetObjectField(TEXT("feel"), McpMotionFeelSummary(Feel.Times, Feel.Locations, Feel.Cameras, Presses));
}
