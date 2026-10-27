// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Foundation/Solvers/McpBoxSat.h"
#include "Templates/Function.h"

class AActor;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;
class UWorld;

// control_actor describe_relations: how the actors of a level stand to each other, as one would describe a room: what
// rests on what, what hangs on a wall, what is inside what, what touches what and what faces what; which actors form a
// group, a row, a grid or a ring; and the scene's roles (ground, sun, sky, player start). A model states relations far
// more reliably than it reads coordinates, so this is the level read back in its own terms. Read only.
namespace McpRelations
{
constexpr double SupportTolerance = 3.0; // cm between a bottom and the top it rests on

/** One actor of the scene: its bounds (every primitive, as a viewer sees it), whether it is ground, and its shape. */
struct FItem
{
    AActor* Actor = nullptr;
    FString Name;
    FBox Box = FBox(ForceInit);
    bool bGround = false;
    /** Copies of one prop share it: the class, the root static mesh, and the box size to 5 cm (the footprint's sides in
     *  either order). */
    FString Shape;
    /** What it draws as a box turned with it about Z, seen from above: what side by side is judged on. */
    McpBoxSat::FFootprint Print;
};

/** Subject Kind Object, by item index; Detail says more (how much of it rests there, which side touches). */
struct FRelation
{
    int32 Subject = INDEX_NONE;
    const TCHAR* Kind = TEXT("");
    int32 Object = INDEX_NONE;
    FString Detail;
};

/** The gap between two boxes seen from above, 0 where their footprints overlap. */
inline double FlatGap(const FBox& A, const FBox& B)
{
    const double X = FMath::Max3(0.0, B.Min.X - A.Max.X, A.Min.X - B.Max.X);
    const double Y = FMath::Max3(0.0, B.Min.Y - A.Max.Y, A.Min.Y - B.Max.Y);
    return FMath::Sqrt(X * X + Y * Y);
}

/** The scene judged (McpRelations.cpp): every visible actor with bounds, within Radius of Center when bArea, at most
 *  MaxActors (the Named first, then the nearest the centre, else the largest). OutInScope counts them before the cap. */
TArray<FItem> CollectItems(UWorld* World, const TSet<AActor*>& Named, bool bArea, const FVector& Center, double Radius,
                           int32 MaxActors, int32& OutInScope);

/** on, mounted, inside, nextTo and faces between the items (McpRelationsPairs.cpp), for the kinds asked. */
void FindPairRelations(UWorld* World, const TArray<FItem>& Items, const TSet<FString>& Kinds, TArray<FRelation>& Out);

/** How two actors meet side by side, judged on their turned footprints (McpRelationsPairs.cpp): describe_relations'
 *  nextTo and expect's nextTo and against read the same. */
struct FContact
{
    bool bTouching = false; // within 2 cm across one axis, sharing more than 1 cm of side and of height
    double Gap = 0.0;       // cm along the axis that parts them most; below 0 they overlap
    FVector2D Toward = FVector2D::ZeroVector; // that axis, from A toward B
    FString Side;           // the side of A that faces B: "its +X side", by the world axis nearest Toward
};
FContact Contact(const McpBoxSat::FFootprint& A, const FBox& BoxA, const McpBoxSat::FFootprint& B, const FBox& BoxB);

/** What an actor draws as a box turned with it about Z, seen from above (McpRelationsJudge.cpp): a chair turned to face
 *  a table spans less than its bounds' box. */
McpBoxSat::FFootprint DrawnFootprint(const AActor& Actor);

/** One relation between two actors for an assertion (McpRelationsJudge.cpp): Kind is on, inside, nextTo, against,
 *  facing or near (within Within cm, the default when negative). OutActual says what was found; OutNextCall is the fixing
 *  call, if any. */
bool JudgePair(UWorld* World, AActor* A, const FString& Kind, AActor* B, double Within, FString& OutActual,
               TSharedPtr<FJsonObject>& OutNextCall);

/** control_actor arrange_by_relations (McpArrange.cpp). */
bool HandleArrangeByRelations(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                              TSharedPtr<FMcpBridgeWebSocket> Socket, TFunctionRef<AActor*(const FString&)> Find);

/** The wider side of an item's footprint. */
inline double Footprint(const FItem& Item)
{
    const FVector Size = Item.Box.GetSize();
    return FMath::Max(Size.X, Size.Y);
}

/** The names of the items Members indexes, as a JSON array. */
inline TArray<TSharedPtr<FJsonValue>> MemberNames(const TArray<FItem>& Items, const TArray<int32>& Members)
{
    TArray<TSharedPtr<FJsonValue>> Out;
    for (const int32 Index : Members)
    {
        Out.Add(MakeShared<FJsonValueString>(Items[Index].Name));
    }
    return Out;
}

/** The patterns among the items (McpRelationsPatterns.cpp), each a JSON row. */
struct FPatterns
{
    TArray<TSharedPtr<FJsonValue>> Groups;
    TArray<TSharedPtr<FJsonValue>> Rows;
    TArray<TSharedPtr<FJsonValue>> Grids;
    TArray<TSharedPtr<FJsonValue>> Rings;
};
FPatterns FindPatterns(const TArray<FItem>& Items, const TSet<FString>& Kinds);

/** The rows, grids and ring that Copies, copies of one prop, form (McpRelationsCopies.cpp), for the kinds asked. */
void FindArrangements(const TArray<FItem>& Items, const TArray<int32>& Copies, const TSet<FString>& Kinds, FPatterns& Out);

/** The scene's roles (McpRelationsRoles.cpp): the ground most things rest on, the sun, the sky and the player starts. */
TSharedPtr<FJsonObject> FindRoles(UWorld* World, const TArray<FItem>& Items, const TArray<FRelation>& Relations);

/** control_actor describe_relations. Find resolves an actor name as every control_actor call does. */
bool HandleDescribeRelations(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                             TSharedPtr<FMcpBridgeWebSocket> Socket, TFunctionRef<AActor*(const FString&)> Find);
} // namespace McpRelations
