#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Domains/ControlEditor/McpEditorScreenshotSupport.h" // FMcpEditorRunHold

class APawn;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;
class UGameInstance;
class UWorld;
namespace McpPratt
{
struct FNode;
}

// control_editor explore_level (S5): Go-Explore without learning (Ecoffet et al. 2019, first phase; written from the
// paper, its code is non-commercial). In a Play session this server started, the player's pawn is walked back to a cell
// of an archive (by nav path, else teleported) and explores from there with sticky random input (the last move repeated
// with p = 0.95, jumps included), never through the OS: AddMovementInput and Jump on the pawn. Every cell the pawn
// enters is kept with the moves that reached it. A session lives in editor memory across calls (op start, continue,
// read, stop), each call advanced per editor frame for at most its seconds, with the window held on screen without focus.
// What it reports: escapes (below KillZ, outside the navigable bounds), traps (cells explored from that lead nowhere and
// from which the start cannot be reached), nav islands no path from the start reaches, script errors per cell, and
// coverage (visited cells over the nav cells sampled).
namespace McpExplore
{
constexpr int32 MaxFindings = 64;

/** A place on the explorer's grid: the cell, the height band, and the tracked flags' truth as bits. */
struct FCellKey
{
    int32 X = 0;
    int32 Y = 0;
    int32 Z = 0;
    uint32 Flags = 0;
    bool operator==(const FCellKey& Other) const
    {
        return X == Other.X && Y == Other.Y && Z == Other.Z && Flags == Other.Flags;
    }
    friend uint32 GetTypeHash(const FCellKey& Key)
    {
        return HashCombine(HashCombine(GetTypeHash(Key.X), GetTypeHash(Key.Y)), HashCombine(GetTypeHash(Key.Z), Key.Flags));
    }
};

/** One sticky move: a direction of eight (8 = stand), a jump, held for Frames. */
struct FMove
{
    uint8 Dir = 8;
    bool bJump = false;
    int32 Frames = 0;
};

struct FCell
{
    FCellKey Key;
    FVector Where = FVector::ZeroVector; // where the pawn first stood in it
    int32 Parent = INDEX_NONE;           // the cell explored from when it was found
    TArray<FMove> Moves;                 // from Parent's Where, the moves that reached it
    int32 Chosen = 0;                    // times explored from
    int32 Leads = 0;                     // cells first found from it
    bool bReturnedByTeleport = false;    // its round began with a teleport: tracked flags may not match
};

struct FFinding
{
    FString Kind; // escape_killz, escape_bounds, trap, island, script_error, stuck
    FString Text;
    FVector Where = FVector::ZeroVector;
    int32 Cell = INDEX_NONE;
    double GameSeconds = 0.0;
};

enum class EStage : uint8
{
    Choose,
    Return,
    Explore
};

struct FSession
{
    TWeakObjectPtr<UGameInstance> Game;
    TWeakObjectPtr<APawn> Pawn;
    double CellCm = 200.0, HeightCm = 100.0;
    TArray<FString> TrackTexts;
    TArray<TSharedPtr<McpPratt::FNode>> Track;
    TArray<FCell> Cells;
    TMap<FCellKey, int32> Index;
    TArray<FFinding> Findings;
    TArray<FVector> NavSamples; // random nav points; those no path from the start reaches are islands
    FBox Bounds = FBox(ForceInit);
    FVector Start = FVector::ZeroVector;
    double KillZ = -1048575.0;
    int32 Rounds = 0;
    // The round in progress.
    EStage Stage = EStage::Choose;
    int32 From = INDEX_NONE;
    int32 Wanted = INDEX_NONE; // the cell the caller asked to explore from next
    FMove Move;
    int32 ExploreLeft = 0;
    TArray<FMove> Trail;
    double ReturnSince = 0.0;
    uint32 Seed = 0;
    uint32 LastFlags = 0; // the tracked flags as last judged
};

/** The session of the running game; null when none was started or the game it explored has ended. */
FSession* Current(const UWorld* PlayWorld);

/** The floor-plan cell a point lies in, height aside (findings, coverage and islands are judged on the floor plan). */
FIntPoint FloorCell(const FSession& Session, const FVector& At);

/** The track assertions judged now, as bits (bit i: assertion i holds). */
uint32 FlagsNow(const FSession& Session, UWorld* World);

/** One editor frame of a call: false once its seconds are up (or the session cannot go on: OutStop says why). */
bool Step(FSession& Session, UWorld* World, double Until, FString& OutStop);

/** Between calls: the pawn back on its round's first cell, still, and the next call starts a round. */
void Park(FSession& Session);

/** The reply: archive summary, findings, coverage; Detail adds the cells (at most 200) and their parents. */
TSharedPtr<FJsonObject> Report(const FSession& Session, bool bDetail);

bool Handle(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket);
} // namespace McpExplore
