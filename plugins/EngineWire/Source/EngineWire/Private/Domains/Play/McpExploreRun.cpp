// control_editor explore_level, the rounds (see McpExplore.h). A round chooses a cell of the archive (the one the caller
// asked for, else by visits: 1/sqrt(chosen + 1)), walks the pawn back to it along the nav mesh (teleporting when no full
// path exists or the walk takes too long), then explores for RoundFrames frames with sticky random moves. Every cell the
// pawn enters for the first time is kept with the moves since the round began.
#include "Domains/Play/McpExplore.h"

#include "AIController.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Domains/Debug/Contracts/McpContracts.h"
#include "Domains/Navigation/McpNavPath.h"
#include "GameFramework/Character.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Navigation/PathFollowingComponent.h"

namespace McpExplore
{
namespace ExploreRunParts
{
constexpr int32 RoundFrames = 120;       // frames explored from a cell: 2 s at 60 fps
constexpr double ReturnSeconds = 8.0;    // a walk back that takes longer ends in a teleport
constexpr int32 RepeatOutOf = 20;        // sticky moves: a new one 1 time in 20, the last repeated with p = 0.95
constexpr int32 JumpOutOf = 5;           // a new move jumps 1 time in 5
constexpr int32 FlagEvery = 10;          // frames between evaluations of the tracked flags
constexpr double FindingBelowKillZ = 100.0;

uint32 Next(FSession& S) // xorshift32: rounds repeat with the same seed
{
    S.Seed ^= S.Seed << 13;
    S.Seed ^= S.Seed >> 17;
    S.Seed ^= S.Seed << 5;
    return S.Seed;
}

FVector DirOf(uint8 Dir)
{
    if (Dir >= 8)
    {
        return FVector::ZeroVector;
    }
    const double Angle = Dir * PI / 4.0;
    return FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0);
}

FCellKey KeyOf(const FSession& S, const FVector& At, uint32 Flags)
{
    FCellKey Key;
    Key.X = FMath::FloorToInt32(At.X / S.CellCm);
    Key.Y = FMath::FloorToInt32(At.Y / S.CellCm);
    Key.Z = FMath::FloorToInt32(At.Z / S.HeightCm);
    Key.Flags = Flags;
    return Key;
}

void Teleport(APawn* Pawn, const FVector& Where)
{
    if (AController* Controller = Pawn->GetController())
    {
        Controller->StopMovement();
    }
    Pawn->SetActorLocation(Where, false, nullptr, ETeleportType::TeleportPhysics);
    if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent())
    {
        Movement->StopMovementImmediately();
    }
}

void Note(FSession& S, const TCHAR* Kind, const FString& Text, const FVector& Where, double Seconds)
{
    // One finding per kind and floor-plan cell: falling off one edge round after round is one finding (nine rows for
    // three edges read as nine holes).
    const FIntPoint At = FloorCell(S, Where);
    if (S.Findings.Num() < MaxFindings &&
        !S.Findings.ContainsByPredicate([&](const FFinding& F) { return F.Kind == Kind && FloorCell(S, F.Where) == At; }))
    {
        S.Findings.Add({Kind, Text, Where, S.From, Seconds});
    }
}

int32 Choose(FSession& S)
{
    if (S.Cells.IsValidIndex(S.Wanted))
    {
        return S.Wanted; // the caller resets it once the round has begun
    }
    double Total = 0.0;
    for (const FCell& Cell : S.Cells)
    {
        Total += 1.0 / FMath::Sqrt(Cell.Chosen + 1.0);
    }
    double Pick = (Next(S) / 4294967296.0) * Total;
    for (int32 Index = 0; Index < S.Cells.Num(); ++Index)
    {
        Pick -= 1.0 / FMath::Sqrt(S.Cells[Index].Chosen + 1.0);
        if (Pick <= 0.0)
        {
            return Index;
        }
    }
    return S.Cells.Num() - 1;
}

void BeginReturn(FSession& S, APawn* Pawn, UWorld* World)
{
    FCell& Cell = S.Cells[S.From];
    ++Cell.Chosen;
    S.ReturnSince = World->GetTimeSeconds();
    S.Stage = EStage::Return;
    McpNavigationHandlers::FMcpNavPath Path;
    FString Code;
    FString Message;
    const bool bWalk = McpNavigationHandlers::FindNavPath(World, Pawn->GetActorLocation(), Cell.Where, FVector(100.0, 100.0, 250.0),
                                                          Path, Code, Message) && Path.Reason == TEXT("ok");
    AController* Controller = Pawn->GetController();
    AAIController* AI = Cast<AAIController>(Controller);
    APlayerController* Player = Cast<APlayerController>(Controller);
    if (bWalk && AI && AI->MoveToLocation(Cell.Where, static_cast<float>(S.CellCm * 0.25)) != EPathFollowingRequestResult::Failed)
    {
        return;
    }
    if (bWalk && Player)
    {
        UAIBlueprintHelperLibrary::SimpleMoveToLocation(Player, Cell.Where);
        return;
    }
    Teleport(Pawn, Cell.Where);
    S.Stage = EStage::Explore;
    S.ExploreLeft = RoundFrames;
    S.Trail.Reset();
    S.Move = FMove();
}
} // namespace ExploreRunParts

using namespace ExploreRunParts;

FIntPoint FloorCell(const FSession& S, const FVector& At)
{
    return FIntPoint(FMath::FloorToInt32(At.X / S.CellCm), FMath::FloorToInt32(At.Y / S.CellCm));
}

uint32 FlagsNow(const FSession& S, UWorld* World)
{
    uint32 Flags = 0;
    for (int32 Index = 0; Index < S.Track.Num() && Index < 32; ++Index)
    {
        Flags |= McpContracts::Judge(S.Track[Index], World, nullptr).Verdict == TEXT("ok") ? (1u << Index) : 0u;
    }
    return Flags;
}

bool Step(FSession& S, UWorld* World, double Until, FString& OutStop)
{
    APawn* Pawn = S.Pawn.Get();
    if (!Pawn || !World)
    {
        OutStop = Pawn ? TEXT("world_ended") : TEXT("pawn_lost");
        return false;
    }
    if (World->IsPaused())
    {
        OutStop = TEXT("paused");
        return false;
    }
    if (FPlatformTime::Seconds() >= Until)
    {
        return false;
    }
    const FVector At = Pawn->GetActorLocation();
    const double Seconds = World->GetTimeSeconds();
    // Escapes: under the kill height, or out of what the nav mesh covers (with a margin of two cells).
    const bool bBelow = At.Z < S.KillZ + FindingBelowKillZ;
    const bool bOutside = S.Bounds.IsValid && !S.Bounds.ExpandBy(S.CellCm * 2.0).IsInsideOrOn(At);
    if ((bBelow || bOutside) && S.Cells.IsValidIndex(S.From))
    {
        Note(S, bBelow ? TEXT("escape_killz") : TEXT("escape_bounds"),
             bBelow ? TEXT("the pawn fell below the level's kill height") : TEXT("the pawn left the area the nav mesh covers"), At,
             Seconds);
        Teleport(Pawn, S.Cells[S.From].Where);
        S.Stage = EStage::Choose;
        return true;
    }
    if (S.Stage == EStage::Choose)
    {
        // Counted as it begins: a round an escape cut short is a round too.
        ++S.Rounds;
        S.From = Choose(S);
        S.Wanted = INDEX_NONE;
        BeginReturn(S, Pawn, World);
        return true;
    }
    if (S.Stage == EStage::Return)
    {
        const bool bThere = FVector::Dist2D(At, S.Cells[S.From].Where) < S.CellCm * 0.4;
        if (!bThere && Seconds - S.ReturnSince < ReturnSeconds)
        {
            return true;
        }
        if (!bThere)
        {
            Teleport(Pawn, S.Cells[S.From].Where);
        }
        else if (AController* Controller = Pawn->GetController())
        {
            Controller->StopMovement();
        }
        S.Stage = EStage::Explore;
        S.ExploreLeft = RoundFrames;
        S.Trail.Reset();
        S.Move = FMove();
        return true;
    }
    // Explore: the last move again with p = 0.95, else a new one (a jump 1 time in 5).
    if (--S.ExploreLeft <= 0)
    {
        FCell& From = S.Cells[S.From];
        if (From.Chosen >= 3 && From.Leads == 0)
        {
            McpNavigationHandlers::FMcpNavPath Back;
            FString Code;
            FString Message;
            const bool bBack = McpNavigationHandlers::FindNavPath(World, From.Where, S.Start, FVector(100.0, 100.0, 250.0), Back, Code,
                                                                  Message) && Back.Reason == TEXT("ok");
            if (!bBack)
            {
                Note(S, TEXT("trap"), TEXT("explored from three times, it led nowhere new and no path leads back to the start"),
                     From.Where, Seconds);
            }
        }
        S.Stage = EStage::Choose;
        return true;
    }
    if (S.Trail.Num() == 0 || Next(S) % RepeatOutOf == 0)
    {
        S.Move.Dir = static_cast<uint8>(Next(S) % 9);
        S.Move.bJump = Next(S) % JumpOutOf == 0;
        S.Move.Frames = 0;
        S.Trail.Add(S.Move);
        if (ACharacter* Character = Cast<ACharacter>(Pawn); Character && S.Move.bJump)
        {
            Character->Jump();
        }
    }
    ++S.Trail.Last().Frames;
    Pawn->AddMovementInput(DirOf(S.Move.Dir), 1.0f);
    // Cells are places the pawn can stand: a fall filed every band it passed through, and rounds began in mid-air.
    const UPawnMovementComponent* Movement = Pawn->GetMovementComponent();
    if (Movement && Movement->IsFalling())
    {
        return true;
    }
    // The cell it stands in: the tracked flags are judged every FlagEvery frames, the last ones standing between.
    if (S.ExploreLeft % FlagEvery == 0)
    {
        S.LastFlags = FlagsNow(S, World);
    }
    // Banded by the feet, which rest just above the floor: the origin of a pawn on a floor at z 10 hovered at about 100,
    // a band edge, and one spot was filed twice.
    const FCellKey Key = KeyOf(S, At - FVector(0.0, 0.0, Pawn->GetSimpleCollisionHalfHeight()), S.LastFlags);
    if (!S.Index.Contains(Key))
    {
        FCell& Cell = S.Cells.AddDefaulted_GetRef();
        Cell.Key = Key;
        Cell.Where = At;
        Cell.Parent = S.From;
        Cell.Moves = S.Trail;
        S.Index.Add(Key, S.Cells.Num() - 1);
        ++S.Cells[S.From].Leads;
    }
    return true;
}

void Park(FSession& S)
{
    // The game runs on between calls: a pawn left walking or falling went over an edge and past the kill height, and
    // the engine destroyed it. Parked on its round's first cell, still.
    APawn* Pawn = S.Pawn.Get();
    if (Pawn && S.Cells.IsValidIndex(S.From))
    {
        Teleport(Pawn, S.Cells[S.From].Where);
    }
    S.Stage = EStage::Choose;
}
} // namespace McpExplore
