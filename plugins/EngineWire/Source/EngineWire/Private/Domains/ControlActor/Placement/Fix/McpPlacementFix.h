// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/Function.h"

class AActor;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;

namespace McpPlacementFix
{
/** One actor in a separation solve: the move planned for it so far, and how freely it moves (0 pins it). */
struct FBody
{
    AActor* Actor = nullptr;
    FVector Offset = FVector::ZeroVector;
    double InverseMass = 0.0;
};

/** What a separation solve did, and the pairs (body indices) it could not push: never measured, or measured apart. */
struct FSolveResult
{
    int32 Sweeps = 0;
    int32 Calls = 0;
    bool bLimitReached = false;
    TArray<TPair<int32, int32>> Unmeasured;
    TArray<TPair<int32, int32>> Apart;
};

/**
 * Pushes the two bodies of each pair apart the shortest way out of each other, measured on their collision at the
 * offsets planned so far (no actor moves while it runs) and split by inverse mass: projected Gauss-Seidel, until no
 * pair is deeper than half a unit, at most 8 sweeps or MaxCalls engine queries.
 */
FSolveResult Separate(TArray<FBody>& Bodies, const TArray<TPair<int32, int32>>& Pairs, int32 MaxCalls);

/** What one actor's placement check (DescribePlacement) found among the kinds a call fixes. */
struct FFinding
{
    FString Warning;
    /** Sunk or floating: the move along Z that rests it on the surface under it. */
    double Lift = 0.0;
    /** The actors it intersects, as the check names them. */
    TArray<FString> Overlaps;
    /** The kinds asked for that it shows. */
    TArray<FString> Kinds;
};
FFinding ReadFinding(AActor* Actor, const TSet<FString>& Kinds);

/** One fix_placement call: the kinds it fixes, the actors that may move, and what it has moved and skipped. */
struct FFixRun
{
    TSet<FString> Kinds;
    TSet<AActor*> Movers;
    TArray<AActor*> Order;
    TMap<AActor*, FVector> Totals;
    TMap<AActor*, TSet<FString>> Fixed;
    TArray<TSharedPtr<FJsonValue>> Skipped;
    TSet<FString> SkippedSeen;
    int32 Calls = 0;
    bool bLimitReached = false;

    /** Lists Actor under skipped with Reason, once. */
    void Skip(const AActor* Actor, const FString& Reason);
    /** One round: checks Check, plans every move and makes them (not under dryRun). Returns the actors it moved. */
    TArray<AActor*> Round(const TArray<AActor*>& Check, bool bDryRun, TFunctionRef<AActor*(const FString&)> Find,
                          TSet<AActor*>* OutFlagged);
};

/** Why an actor is never moved (a floor or landscape, movement locked, attached to another), or empty when it may be. */
FString PinnedReason(const AActor* Actor);

/** control_actor fix_placement. Find resolves an actor name as every control_actor call does. */
bool HandleFixPlacement(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                        TSharedPtr<FMcpBridgeWebSocket> Socket, TFunctionRef<AActor*(const FString&)> Find);
} // namespace McpPlacementFix
