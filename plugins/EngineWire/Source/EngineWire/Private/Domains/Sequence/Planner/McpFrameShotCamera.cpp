// Copyright (c) 2024 EngineWire Contributors

#include "Domains/Sequence/Planner/McpFrameShotCamera.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Foundation/BridgeHelpers/Actors/McpActorMove.h"
#include "Foundation/BridgeHelpers/Actors/McpActorSpawnHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Misc/EngineVersionComparison.h"
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1)
#include "Engine/HitResult.h" // 5.0 declares FHitResult in EngineTypes.h
#endif
#if MCP_HAS_CINEMATIC_CAMERA
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#endif

namespace McpFrameShot
{
namespace
{
constexpr float FrameShotRoom = 15.f; // cm the camera needs clear around it

double FieldOfView(const FLens& Lens)
{
    return FMath::RadiansToDegrees(2.0 * FMath::Atan(Lens.TanHalfH));
}

// Value to 1 / Scale (Scale 10, 100 or 1000): divided, so 26.7 reads 26.7 rather than 26.700000000000003.
double Rounded(double Value, double Scale)
{
    return FMath::RoundToDouble(Value * Scale) / Scale;
}

TArray<TSharedPtr<FJsonValue>> ScreenPoint(const FVector2D& Point)
{
    TArray<TSharedPtr<FJsonValue>> Out;
    Out.Add(MakeShared<FJsonValueNumber>(Rounded(Point.X, 1000.0)));
    Out.Add(MakeShared<FJsonValueNumber>(Rounded(Point.Y, 1000.0)));
    return Out;
}
} // namespace

double HiddenShare(const UWorld& World, const FCollisionQueryParams& Params, const FVector& Camera, const FBox& Box)
{
    const FVector Centre = Box.GetCenter();
    const FVector Extent = Box.GetExtent();
    const FVector Facing = (Centre - Camera).GetSafeNormal();
    const FVector Side = FVector::CrossProduct(FMath::Abs(Facing.Z) > 0.99 ? FVector::ForwardVector : FVector::UpVector, Facing).GetSafeNormal();
    const FVector Rise = FVector::CrossProduct(Facing, Side);
    const auto Reach = [&Extent](const FVector& Axis)
    {
        return 0.6 * (FMath::Abs(Axis.X) * Extent.X + FMath::Abs(Axis.Y) * Extent.Y + FMath::Abs(Axis.Z) * Extent.Z);
    };
    int32 Blocked = 0;
    for (int32 Row = -1; Row <= 1; ++Row)
    {
        for (int32 Column = -1; Column <= 1; ++Column)
        {
            FHitResult Hit;
            const FVector Point = Centre + Side * (Column * Reach(Side)) + Rise * (Row * Reach(Rise));
            Blocked += World.LineTraceSingleByChannel(Hit, Camera, Point, ECC_Visibility, Params) ? 1 : 0;
        }
    }
    return Blocked / 9.0;
}

void ReadCamera(AActor* Actor, const FString& Label, double Asked, double Default, FFrameCamera& Out, FString& OutError)
{
    Out.Actor = Actor;
    Out.Label = Actor ? Actor->GetActorLabel() : Label;
    Out.Component = Actor ? Actor->FindComponentByClass<UCameraComponent>() : nullptr;
    if (Actor && !Out.Component)
    {
        OutError = FString::Printf(TEXT("'%s' is no camera: name a camera actor to move, or a new name to spawn a CineCameraActor under."), *Label);
        return;
    }
#if MCP_HAS_CINEMATIC_CAMERA
    // A cine camera's frame, its sensor read back from its own field of view so a crop or squeeze it has counts.
    const UCineCameraComponent* Cine = Actor ? Cast<UCineCameraComponent>(Out.Component) : GetDefault<UCineCameraComponent>();
    if (Cine)
    {
        const double Now = FMath::Max(static_cast<double>(Cine->CurrentFocalLength), 1.0);
        const double HalfWidth = Now * FMath::Tan(FMath::DegreesToRadians(static_cast<double>(Cine->GetHorizontalFieldOfView())) / 2.0);
        const double HalfHeight = Now * FMath::Tan(FMath::DegreesToRadians(static_cast<double>(Cine->GetVerticalFieldOfView())) / 2.0);
        const double Shortest = Cine->LensSettings.MinFocalLength;
        const double Longest = Cine->LensSettings.MaxFocalLength;
        Out.bCine = true;
        Out.FocalLength = Asked > 0.0 ? Asked : Default;
        Out.FocalLength = Longest > Shortest ? FMath::Clamp(Out.FocalLength, Shortest, Longest) : Out.FocalLength; // as the lens keeps it
        Out.Lens.TanHalfH = HalfWidth / Out.FocalLength;
        Out.Lens.TanHalfV = HalfHeight / Out.FocalLength;
        return;
    }
#endif
    if (Asked > 0.0)
    {
        OutError = FString::Printf(TEXT("'%s' is a plain camera with no lens: leave focalLength out (its field of view is used), or name a CineCameraActor."), *Label);
        return;
    }
    const UCameraComponent* Plain = Out.Component ? Out.Component : GetDefault<UCameraComponent>();
    Out.Lens.TanHalfH = FMath::Tan(FMath::DegreesToRadians(static_cast<double>(Plain->FieldOfView)) / 2.0);
    Out.Lens.TanHalfV = Out.Lens.TanHalfH / FMath::Max(static_cast<double>(Plain->AspectRatio), 10.0);
}

FShot SolveInWorld(const UWorld& World, const FProblem& Problem, const TArray<AActor*>& Subjects, const TArray<FBox>& Boxes, AActor* Camera)
{
    const FCollisionQueryParams Room(SCENE_QUERY_STAT(McpFrameShotRoom), false, Camera);
    TArray<FCollisionQueryParams> Sight;
    for (AActor* Subject : Subjects)
    {
        FCollisionQueryParams& Params = Sight.Add_GetRef(FCollisionQueryParams(SCENE_QUERY_STAT(McpFrameShotSight), true, Subject));
        TArray<AActor*> Attached;
        Subject->GetAttachedActors(Attached);
        Params.AddIgnoredActors(Attached);
        Params.AddIgnoredActor(Camera);
    }
    FProblem Local = Problem;
    Local.Occluded = [&World, &Sight, &Boxes](const FVector& At, int32 Index) { return HiddenShare(World, Sight[Index], At, Boxes[Index]); };
    Local.Inside = [&World, &Room](const FVector& At)
    { return World.OverlapBlockingTestByChannel(At, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(FrameShotRoom), Room); };
    return Solve(Local);
}

bool WriteShot(FFrameCamera& Camera, const FShot& Shot, double FocusDistance, const TSharedPtr<FJsonObject>& Result, FString& OutCode, FString& OutError)
{
    TArray<UObject*> Touched;
    if (Camera.Actor)
    {
        Touched = {Camera.Actor, Camera.Component};
    }
    const FMcpScopedEditorTransaction Transaction(NSLOCTEXT("McpFrameShot", "FrameShot", "Frame Shot"), EMcpMutationDurability::EditorStateOnly, Touched);
    if (!Transaction.IsRecordingUndo()) // a shot is set only as an undo step
    {
        OutCode = Transaction.GetRefusalCode();
        OutError = Transaction.GetRefusalDetail();
        return false;
    }
    if (!Camera.Actor)
    {
#if MCP_HAS_CINEMATIC_CAMERA
        UClass* const Class = ACineCameraActor::StaticClass();
#else
        UClass* const Class = ACameraActor::StaticClass();
#endif
        Camera.Actor = SpawnActorInActiveWorld<AActor>(Class, Shot.Location, Shot.Rotation, Camera.Label);
        Camera.Component = Camera.Actor ? Camera.Actor->FindComponentByClass<UCameraComponent>() : nullptr;
        if (!Camera.Component)
        {
            OutCode = TEXT("SPAWN_FAILED");
            OutError = TEXT("The camera could not be spawned; nothing changed.");
            return false;
        }
    }
    else
    {
        Camera.Actor->SetActorLocationAndRotation(Shot.Location, Shot.Rotation, false, nullptr, ETeleportType::TeleportPhysics);
    }
    McpFinishEditorMove(Camera.Actor);
#if MCP_HAS_CINEMATIC_CAMERA
    if (UCineCameraComponent* Cine = Cast<UCineCameraComponent>(Camera.Component))
    {
        Cine->SetCurrentFocalLength(static_cast<float>(Camera.FocalLength));
        Cine->FocusSettings.FocusMethod = ECameraFocusMethod::Manual;
        Cine->FocusSettings.ManualFocusDistance = static_cast<float>(FocusDistance);
    }
#endif
    Camera.Actor->MarkPackageDirty();
    Transaction.DescribeInto(Result);
    return true;
}

TSharedPtr<FJsonObject> DescribeShot(const FProblem& Problem, const FShot& Shot, const FFrameCamera& Camera, const TArray<AActor*>& Subjects)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("camera"), Camera.Label);
    Result->SetBoolField(TEXT("spawned"), Camera.Actor == nullptr);
    const FVector Location(Rounded(Shot.Location.X, 10.0), Rounded(Shot.Location.Y, 10.0), Rounded(Shot.Location.Z, 10.0));
    Result->SetArrayField(TEXT("location"), McpHandlerUtils::VectorToJsonArray(Location));
    Result->SetArrayField(TEXT("rotation"), McpHandlerUtils::RotatorToJsonArray(FRotator(Rounded(Shot.Rotation.Pitch, 100.0), Rounded(Shot.Rotation.Yaw, 100.0), 0.0)));
    Result->SetNumberField(TEXT("fieldOfView"), Rounded(FieldOfView(Camera.Lens), 10.0));
    if (Camera.bCine)
    {
        Result->SetNumberField(TEXT("focalLength"), Rounded(Camera.FocalLength, 10.0));
        Result->SetNumberField(TEXT("focusDistance"), Rounded(FVector::Dist(Shot.Location, Problem.Subjects[0].Center), 10.0));
    }
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (int32 Index = 0; Index < Subjects.Num(); ++Index)
    {
        const FVector2D& Target = Problem.Targets[Index];
        const FVector2D& Screen = Shot.Screen[Index];
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("actorName"), Subjects[Index]->GetActorLabel());
        Row->SetArrayField(TEXT("target"), ScreenPoint(Target));
        Row->SetArrayField(TEXT("screen"), ScreenPoint(Screen));
        Row->SetNumberField(TEXT("error"), Rounded(FVector2D::Distance(Screen, Target), 1000.0));
        Row->SetNumberField(TEXT("hidden"), Rounded(Shot.Occluded[Index], 100.0));
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Result->SetArrayField(TEXT("subjects"), Rows);
    if (Subjects.Num() == 1)
    {
        Result->SetNumberField(TEXT("fill"), Rounded(Shot.Fill, 100.0));
    }
    return Result;
}

FString ShotMessage(const FProblem& Problem, const FShot& Shot, const FFrameCamera& Camera, const TArray<AActor*>& Subjects, bool bSpawn, bool bDryRun)
{
    TArray<FString> Names;
    TArray<FString> Hidden;
    double Worst = 0.0;
    FVector Middle = FVector::ZeroVector;
    for (int32 Index = 0; Index < Subjects.Num(); ++Index)
    {
        Names.Add(Subjects[Index]->GetActorLabel());
        Worst = FMath::Max(Worst, FVector2D::Distance(Shot.Screen[Index], Problem.Targets[Index]));
        Middle += Problem.Subjects[Index].Center / Subjects.Num();
        if (Shot.Occluded[Index] > 0.0)
        {
            Hidden.Add(FString::Printf(TEXT("%s %d%% hidden"), *Names.Last(), FMath::RoundToInt(Shot.Occluded[Index] * 100.0)));
        }
    }
    const bool bOne = Subjects.Num() == 1;
    const FString Verb = bDryRun ? (bSpawn ? TEXT("Would spawn") : TEXT("Would move")) : (bSpawn ? TEXT("Spawned") : TEXT("Moved"));
    const FString Marks = Worst < 0.02 ? FString(bOne ? TEXT("on its mark") : TEXT("on their marks"))
                                       : FString::Printf(TEXT("within %.2f of %s"), Worst, bOne ? TEXT("its mark") : TEXT("their marks"));
    const FString Lens = Camera.bCine ? FString::Printf(TEXT("%.0f mm lens"), Camera.FocalLength)
                                      : FString::Printf(TEXT("%.0f degree field of view"), FieldOfView(Camera.Lens));
    return FString::Printf(TEXT("%s %s to frame %s %s from %.0f cm (%s); %s."), *Verb, *Camera.Label, *FString::Join(Names, TEXT(" and ")),
                           *Marks, FVector::Dist(Shot.Location, Middle), *Lens,
                           Hidden.Num() > 0 ? *FString::Join(Hidden, TEXT(", ")) : TEXT("nothing in the way"));
}
} // namespace McpFrameShot
