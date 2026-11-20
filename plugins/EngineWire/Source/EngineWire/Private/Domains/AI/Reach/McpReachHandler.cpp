// prove_reachable's handler (McpReach.h): the search over the graph's walk groups and the reply.

#include "Domains/AI/Reach/McpReach.h"

#include "Domains/AI/Reach/McpReachGraph.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/PlatformTime.h"

namespace McpReachDetail
{
const ACharacter* PawnDefaults(UWorld* World, const TSharedPtr<FJsonObject>& Payload, FString& OutName);
McpReach::FMover MoverOf(const ACharacter& Pawn, const UWorld& World);
bool PointOf(UWorld* World, const TSharedPtr<FJsonValue>& Value, FVector& OutPoint, FString& OutName);
TArray<TSharedPtr<FJsonValue>> Vec(const FVector& V);

// What a jump from Near toward Goal lacks: height above the apex, else the gap past its reach; empty when neither.
FString Lacks(const McpReach::FMover& Mover, const FVector& Near, const FVector& Goal)
{
    const double Rise = Goal.Z - Near.Z;
    const double Gap = FVector::Dist2D(Near, Goal);
    if (Rise > McpReach::Apex(Mover))
    {
        return FString::Printf(TEXT("a jump rises %.0f cm; this needs %.0f cm more height (a higher jump, or a step within reach)"), McpReach::Apex(Mover), Rise - McpReach::Apex(Mover));
    }
    const double Reach = McpReach::Reach(Mover, Rise);
    return Gap > Reach
        ? FString::Printf(TEXT("a jump at this rise clears %.0f cm; the gap is %.0f cm, %.0f cm too wide (a ledge within reach, or more speed)"), Reach, Gap, Gap - Reach)
        : FString(TEXT("within a jump's height and reach, but something blocks the arc (a ceiling or wall on the way)"));
}
} // namespace McpReachDetail

namespace McpReach
{
bool HandleProveReachable(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                          TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace McpReachDetail;
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    FString PawnName;
    const ACharacter* Pawn = World ? PawnDefaults(World, Payload, PawnName) : nullptr;
    if (!Pawn)
    {
        Bridge->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("'%s' is no Character class (prove_reachable reads a Character's movement: pass pawnClass, or give the level a game mode whose default pawn is one)."), *PawnName), TEXT("NOT_A_CHARACTER"));
        return true;
    }
    const FMover Mover = MoverOf(*Pawn, *World);
    FVector Start;
    FString StartName;
    TSharedPtr<FJsonValue> From = Payload->TryGetField(TEXT("from"));
    if (!From.IsValid())
    {
        TActorIterator<APlayerStart> First(World);
        if (First)
        {
            From = MakeShared<FJsonValueString>(First->GetActorLabel());
        }
    }
    const TArray<TSharedPtr<FJsonValue>>* GoalValues = nullptr;
    if (!PointOf(World, From, Start, StartName) || !Payload->TryGetArrayField(TEXT("goals"), GoalValues) || GoalValues->Num() == 0)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("goals (actor labels or [x, y, z]) are required, and from must name an actor or a point (default: the level's player start, which it has none of)."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    TArray<FVector> Goals;
    TArray<FString> GoalNames;
    FBox Bounds(Start, Start);
    for (const TSharedPtr<FJsonValue>& Value : *GoalValues)
    {
        FVector Point;
        FString Name;
        if (!PointOf(World, Value, Point, Name))
        {
            Bridge->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("No actor named '%s' in the level."), *Name), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        Goals.Add(Point);
        GoalNames.Add(Name);
        Bounds += Point;
    }
    const double Margin = Reach(Mover, 0.0) + 500.0;
    Bounds = FBox(Bounds.Min - FVector(Margin, Margin, 2000.0), Bounds.Max + FVector(Margin, Margin, Apex(Mover) + Mover.HalfHeight * 2.0 + 500.0));
    const double Deadline = FPlatformTime::Seconds() + FMath::Clamp(GetJsonIntField(Payload, TEXT("budgetMs"), 8000), 500, 20000) / 1000.0;
    TArray<const AActor*> Ignore;
    for (TActorIterator<APawn> It(World); It; ++It) Ignore.Add(*It); // placed pawns are not floors
    const FGraph Graph = Build(World, Mover, Bounds, FMath::Clamp(GetJsonIntField(Payload, TEXT("maxCells"), 20000), 100, 100000), Deadline, Ignore);
    const int32 StartNode = NearestNode(Graph, Start, Mover.HalfHeight);
    if (StartNode == INDEX_NONE)
    {
        Bridge->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Nothing the pawn can stand on is found at the start (%s)."), *StartName), TEXT("NO_START_SURFACE"));
        return true;
    }
    // Breadth-first over groups, keeping the move that first reached each.
    TArray<FJump> Via;
    Via.Init(FJump(), Graph.GroupCount);
    TArray<bool> Seen;
    Seen.Init(false, Graph.GroupCount);
    TArray<int32> Open{Graph.Group[StartNode]};
    Seen[Graph.Group[StartNode]] = true;
    for (int32 Head = 0; Head < Open.Num(); ++Head)
    {
        for (const TPair<int32, FJump>& Edge : Graph.Out[Open[Head]])
        {
            if (!Seen[Edge.Key])
            {
                Seen[Edge.Key] = true;
                Via[Edge.Key] = Edge.Value;
                Open.Add(Edge.Key);
            }
        }
    }
    TArray<TSharedPtr<FJsonValue>> Rows;
    int32 Reached = 0;
    for (int32 Index = 0; Index < Goals.Num(); ++Index)
    {
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("goal"), GoalNames[Index]);
        const int32 GoalNode = NearestNode(Graph, Goals[Index], Mover.HalfHeight);
        const bool bReached = GoalNode != INDEX_NONE && Seen[Graph.Group[GoalNode]];
        Row->SetBoolField(TEXT("reachable"), bReached);
        Reached += bReached ? 1 : 0;
        if (bReached)
        {
            TArray<TSharedPtr<FJsonValue>> Jumps;
            for (int32 Group = Graph.Group[GoalNode]; Via[Group].From != INDEX_NONE; Group = Graph.Group[Via[Group].From])
            {
                TSharedPtr<FJsonObject> Jump = MakeShared<FJsonObject>();
                Jump->SetArrayField(TEXT("takeOff"), Vec(Graph.Feet[Via[Group].From]));
                Jump->SetArrayField(TEXT("landing"), Vec(Graph.Feet[Via[Group].To]));
                Jump->SetNumberField(TEXT("rise"), FMath::RoundToDouble(Graph.Feet[Via[Group].To].Z - Graph.Feet[Via[Group].From].Z));
                Jumps.Insert(MakeShared<FJsonValueObject>(Jump), 0);
            }
            Row->SetArrayField(TEXT("jumps"), Jumps);
        }
        else
        {
            // The nearest point the pawn gets to, and what a jump from it toward the goal lacks.
            int32 Near = INDEX_NONE;
            for (int32 Node = 0; Node < Graph.Feet.Num(); ++Node)
            {
                if (Seen[Graph.Group[Node]] && (Near == INDEX_NONE || FVector::Dist(Graph.Feet[Node], Goals[Index]) < FVector::Dist(Graph.Feet[Near], Goals[Index])))
                {
                    Near = Node;
                }
            }
            Row->SetArrayField(TEXT("nearestReached"), Vec(Graph.Feet[Near]));
            Row->SetStringField(TEXT("missing"), GoalNode == INDEX_NONE ? FString(TEXT("there is nothing the pawn can stand on at the goal"))
                                                                         : Lacks(Mover, Graph.Feet[Near], Graph.Feet[GoalNode]));
        }
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("goals"), Rows);
    TSharedPtr<FJsonObject> Moves = MakeShared<FJsonObject>();
    Moves->SetStringField(TEXT("pawn"), PawnName);
    Moves->SetNumberField(TEXT("jumpHeight"), FMath::RoundToDouble(Apex(Mover)));
    Moves->SetNumberField(TEXT("jumpReach"), FMath::RoundToDouble(Reach(Mover, 0.0)));
    Moves->SetNumberField(TEXT("stepHeight"), Mover.Step);
    Result->SetObjectField(TEXT("pawnMoves"), Moves);
    Result->SetNumberField(TEXT("surfaces"), Graph.Feet.Num());
    Result->SetNumberField(TEXT("cellSize"), FMath::RoundToDouble(Graph.Cell));
    if (Graph.bBudget)
    {
        Result->SetBoolField(TEXT("budgetExhausted"), true);
    }
    Bridge->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("%d of %d goal(s) reachable from %s%s"), Reached, Goals.Num(), *StartName,
        Graph.bBudget ? TEXT(" (the time ran out before every jump was tried: an unreachable answer may be a missed jump)") : TEXT("")), Result);
    return true;
}
} // namespace McpReach
