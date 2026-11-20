// manage_ai prove_reachable (McpReach.h): the pawn's movement numbers, the start and goals, the graph
// (McpReachGraph.cpp), and a breadth-first search over its walk groups that answers each goal with the jumps on the way
// or, for one out of reach, the nearest point reached and what the last jump lacked.

#include "Domains/AI/Reach/McpReach.h"

#include "Domains/AI/Reach/McpReachGraph.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformTime.h"

namespace McpReach
{
double Apex(const FMover& Mover)
{
    return Mover.JumpZ * Mover.JumpZ / (2.0 * Mover.Gravity);
}

double Reach(const FMover& Mover, double Rise)
{
    const double Root = Mover.JumpZ * Mover.JumpZ - 2.0 * Mover.Gravity * Rise;
    return Root < 0.0 ? -1.0 : Mover.Speed * (Mover.JumpZ + FMath::Sqrt(Root)) / Mover.Gravity;
}

TArray<FVector> Arc(const FMover& Mover, const FVector& From, const FVector& To, int32 Count)
{
    const double Rise = To.Z - From.Z;
    const double Root = FMath::Max(0.0, Mover.JumpZ * Mover.JumpZ - 2.0 * Mover.Gravity * Rise);
    const double Land = (Mover.JumpZ + FMath::Sqrt(Root)) / Mover.Gravity;
    const FVector Flat(To.X - From.X, To.Y - From.Y, 0.0);
    TArray<FVector> Points;
    for (int32 Index = 1; Index <= Count; ++Index)
    {
        const double Time = Land * Index / Count;
        const double Z = Index == Count ? Rise : Mover.JumpZ * Time - 0.5 * Mover.Gravity * Time * Time;
        Points.Add(From + Flat * (static_cast<double>(Index) / Count) + FVector(0.0, 0.0, Z));
    }
    return Points;
}
} // namespace McpReach

namespace McpReachDetail
{
using namespace McpReach;

// The pawn class: pawnClass, else the class of a placed actorName, else the level's game mode's default pawn.
const ACharacter* PawnDefaults(UWorld* World, const TSharedPtr<FJsonObject>& Payload, FString& OutName)
{
    UClass* Class = nullptr;
    const FString ClassPath = GetJsonStringField(Payload, TEXT("pawnClass"));
    if (!ClassPath.IsEmpty())
    {
        Class = LoadClass<APawn>(nullptr, *ClassPath);
    }
    else if (const AWorldSettings* Settings = World->GetWorldSettings())
    {
        const UClass* Mode = Settings->DefaultGameMode ? Settings->DefaultGameMode.Get() : nullptr;
        const AGameModeBase* ModeDefaults = Mode ? Mode->GetDefaultObject<AGameModeBase>() : nullptr;
        Class = ModeDefaults ? ModeDefaults->DefaultPawnClass.Get() : nullptr;
    }
    OutName = Class ? Class->GetPathName() : ClassPath;
    return Class ? Cast<ACharacter>(Class->GetDefaultObject()) : nullptr;
}

FMover MoverOf(const ACharacter& Pawn, const UWorld& World)
{
    FMover Mover;
    const UCharacterMovementComponent* Move = Pawn.GetCharacterMovement();
    const UCapsuleComponent* Capsule = Pawn.GetCapsuleComponent();
    Mover.Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : Mover.Radius;
    Mover.HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : Mover.HalfHeight;
    if (Move)
    {
        Mover.JumpZ = Move->JumpZVelocity;
        Mover.Gravity = FMath::Abs(World.GetGravityZ()) * Move->GravityScale;
        Mover.Speed = Move->MaxWalkSpeed;
        Mover.Step = Move->MaxStepHeight;
        Mover.WalkableZ = Move->GetWalkableFloorZ();
    }
    return Mover;
}

// A point from a JSON value: an actor's label, or [x, y, z]. Feet on the floor below it.
bool PointOf(UWorld* World, const TSharedPtr<FJsonValue>& Value, FVector& OutPoint, FString& OutName, FVector* OutTop)
{
    if (!Value.IsValid()) return false;
    if (Value->Type == EJson::Array && Value->AsArray().Num() == 3)
    {
        const TArray<TSharedPtr<FJsonValue>>& Xyz = Value->AsArray();
        OutPoint = FVector(Xyz[0]->AsNumber(), Xyz[1]->AsNumber(), Xyz[2]->AsNumber());
        OutName = OutPoint.ToString();
        if (OutTop) *OutTop = OutPoint;
        return true;
    }
    OutName = Value->AsString();
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorLabel().Equals(OutName, ESearchCase::IgnoreCase) || It->GetName().Equals(OutName, ESearchCase::IgnoreCase))
        {
            FVector Origin, Extent;
            It->GetActorBounds(true, Origin, Extent);
            OutPoint = FVector(Origin.X, Origin.Y, Origin.Z - Extent.Z);
            if (OutTop) *OutTop = FVector(Origin.X, Origin.Y, Origin.Z + Extent.Z); // a platform: its top is where one stands
            return true;
        }
    }
    return false;
}

TArray<TSharedPtr<FJsonValue>> Vec(const FVector& V)
{
    return {MakeShared<FJsonValueNumber>(FMath::RoundToDouble(V.X)), MakeShared<FJsonValueNumber>(FMath::RoundToDouble(V.Y)),
            MakeShared<FJsonValueNumber>(FMath::RoundToDouble(V.Z))};
}
} // namespace McpReachDetail
