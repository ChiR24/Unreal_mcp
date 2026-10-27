// manage_ai analyze_level_layout (see McpLevelLayout.h): reads the editor level's nav mesh and answers
// with its spaces, doors, chokes, dead ends, loops, the start-to-goal walk and how far one sees from each space.
#include "Domains/Navigation/Analytics/McpLevelLayout.h"

#include "Domains/Navigation/McpNavPrivate.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/Paths.h"

namespace McpLevelLayout
{
namespace
{
constexpr double LayoutEndRiseCm = 300.0; // an actor's origin stands above its floor (a PlayerStart's capsule centre ~90 cm)

// The sample an end stands on: the nearest one, when it lies within a cell across and a floor up or down; INDEX_NONE
// otherwise (the end is outside the area read, and the nearest sample would answer for another place). OutAway: how far.
int32 LayoutEndNode(const FGraph& Graph, const FVector& Where, double& OutAway)
{
    int32 Best = INDEX_NONE;
    for (int32 Node = 0; Node < Graph.Points.Num(); ++Node)
    {
        if (Best == INDEX_NONE || FVector::DistSquared(Graph.Points[Node], Where) < FVector::DistSquared(Graph.Points[Best], Where))
        {
            Best = Node;
        }
    }
    OutAway = Best == INDEX_NONE ? 0.0 : FVector::Dist(Graph.Points[Best], Where);
    const bool bOn = Best != INDEX_NONE && FVector::Dist2D(Graph.Points[Best], Where) <= 1.5 * Graph.CellCm &&
                     FMath::Abs(Graph.Points[Best].Z - Where.Z) <= LayoutEndRiseCm;
    return bOn ? Best : INDEX_NONE;
}

// Each sample's parent on a walk back to Start (Start its own; INDEX_NONE where Start cannot walk).
TArray<int32> LayoutWalk(const FGraph& Graph, int32 Start)
{
    TArray<int32> Parent;
    Parent.Init(INDEX_NONE, Graph.Points.Num());
    TArray<int32> Queue;
    if (Start != INDEX_NONE)
    {
        Parent[Start] = Start;
        Queue.Add(Start);
    }
    for (int32 Head = 0; Head < Queue.Num(); ++Head)
    {
        for (const int32 Next : Graph.Adjacency[Queue[Head]])
        {
            if (Parent[Next] == INDEX_NONE)
            {
                Parent[Next] = Queue[Head];
                Queue.Add(Next);
            }
        }
    }
    return Parent;
}

bool LayoutEnd(UWorld* World, const TSharedPtr<FJsonObject>& Payload, const TCHAR* ActorField, const TCHAR* PointField,
               FVector& Out, FString& OutName, FString& OutError)
{
    FString ActorName;
    if (Payload->TryGetStringField(ActorField, ActorName) && !ActorName.IsEmpty())
    {
        const AActor* Actor = FindActorByNameInWorldForMcp(World, ActorName, false);
        OutError = Actor ? FString() : FString::Printf(TEXT("%s '%s' names no actor in the level."), ActorField, *ActorName);
        Out = Actor ? Actor->GetActorLocation() : FVector::ZeroVector;
        OutName = ActorName;
        return Actor != nullptr;
    }
    if (PointField && Payload->HasField(PointField))
    {
        Out = ExtractVectorField(Payload, PointField, FVector::ZeroVector);
        OutName = PointField;
        return true;
    }
    return false;
}
} // namespace

bool Handle(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    const auto Refuse = [&](const FString& Message, const TCHAR* Code)
    {
        Bridge->SendAutomationResponse(Socket, RequestId, false, Message, nullptr, Code);
        return true;
    };
    if (!World)
    {
        return Refuse(TEXT("No editor level is open."), TEXT("NO_WORLD"));
    }
    FSampleSpec Spec;
    Spec.CellCm = FMath::Clamp(GetJsonNumberField(Payload, TEXT("cellCm"), 100.0), 25.0, 1000.0);
    Spec.MaxNodes = FMath::Clamp(static_cast<int32>(GetJsonNumberField(Payload, TEXT("maxNodes"), 6000.0)), 100, 20000);
    if (Payload->HasField(TEXT("center")) != Payload->HasField(TEXT("extent")))
    {
        return Refuse(TEXT("Give center and extent together (a box), or neither to read the whole navigable area."), TEXT("INVALID_ARGUMENT"));
    }
    const UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    const FBox Navigable = Nav ? Nav->GetNavigableWorldBounds() : FBox(ForceInit);
    const bool bBox = Payload->HasField(TEXT("center"));
    Spec.Area = Navigable;
    if (bBox)
    {
        // Only the navigable part of the box is read: that bounds the work however large the box (the editor waits on it).
        const FVector Extent = ExtractVectorField(Payload, TEXT("extent"), FVector::ZeroVector).GetAbs();
        const FBox Box = FBox::BuildAABB(ExtractVectorField(Payload, TEXT("center"), FVector::ZeroVector), Extent);
        Spec.Area = Navigable.IsValid ? Box.Overlap(Navigable) : Box;
    }
    FVector Start = FVector::ZeroVector;
    FVector Goal = FVector::ZeroVector;
    FString StartName;
    FString GoalName;
    FString Error;
    const bool bStart = LayoutEnd(World, Payload, TEXT("startActor"), nullptr, Start, StartName, Error);
    if (!Error.IsEmpty())
    {
        return Refuse(Error, TEXT("ACTOR_NOT_FOUND"));
    }
    const bool bGoal = LayoutEnd(World, Payload, TEXT("goalActor"), TEXT("goal"), Goal, GoalName, Error);
    if (!Error.IsEmpty())
    {
        return Refuse(Error, TEXT("ACTOR_NOT_FOUND"));
    }
    TActorIterator<APlayerStart> FirstStart(World);
    const APlayerStart* PlayerStart = !bStart && FirstStart ? *FirstStart : nullptr;
    const bool bHasStart = bStart || PlayerStart;
    if (PlayerStart)
    {
        Start = PlayerStart->GetActorLocation();
        StartName = PlayerStart->GetActorLabel();
    }

    FGraph Graph;
    FString Code;
    const TCHAR* const NoWalkable = TEXT("No walkable nav mesh lies in the area read: move or widen it (center, extent).");
    if (!Spec.Area.IsValid)
    {
        return bBox && Navigable.IsValid ? Refuse(NoWalkable, TEXT("NO_WALKABLE_AREA"))
                                         : Refuse(TEXT("The level has no navigable area: put a Nav Mesh Bounds Volume around the walkable "
                                                       "area and build it (manage_ai rebuild_navigation)."), TEXT("NAV_DATA_MISSING"));
    }
    if (!Sample(World, Spec, Graph, Code, Error))
    {
        return Refuse(Error, *Code);
    }
    if (Graph.Points.Num() < 2)
    {
        return Refuse(NoWalkable, TEXT("NO_WALKABLE_AREA"));
    }
    double Away = 0.0;
    const int32 StartNode = bHasStart ? LayoutEndNode(Graph, Start, Away) : INDEX_NONE;
    if (bStart && StartNode == INDEX_NONE)
    {
        return Refuse(FString::Printf(TEXT("startActor '%s' stands %.0f m from the nav mesh read: give a box (center, extent) that "
                                           "holds it, or a start on the nav mesh."), *StartName, Away / 100.0), TEXT("INVALID_ARGUMENT"));
    }
    const int32 GoalNode = bGoal ? LayoutEndNode(Graph, Goal, Away) : INDEX_NONE;
    if (bGoal && GoalNode == INDEX_NONE)
    {
        return Refuse(FString::Printf(TEXT("The goal (%s) is %.0f m from the nav mesh read: give a box (center, extent) that holds "
                                           "it, or a goal on the nav mesh."), *GoalName, Away / 100.0), TEXT("INVALID_ARGUMENT"));
    }
    const double RoomWidth = FMath::Clamp(GetJsonNumberField(Payload, TEXT("roomWidthCm"), 400.0), 100.0, 5000.0);
    FLayout Layout = Spaces(Graph, FMath::Max(1, FMath::RoundToInt(RoomWidth / Graph.CellCm)));
    const TArray<int32> Parent = LayoutWalk(Graph, StartNode);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString Summary = WriteReply(World, Graph, Layout, Parent, StartNode, GoalNode, StartName, GoalName, Result);
    if (PlayerStart && StartNode == INDEX_NONE)
    {
        Summary += FString::Printf(TEXT(" The first PlayerStart (%s) is outside the area read, so nothing is said about what can "
                                        "be reached: give startActor."), *StartName);
    }
    else if (bGoal && StartNode == INDEX_NONE)
    {
        Summary += TEXT(" No start was given and the level has no PlayerStart: give startActor for the walk to the goal.");
    }
    if (GetJsonBoolField(Payload, TEXT("image"), false))
    {
        const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(
            FPaths::ProjectSavedDir(), TEXT("EngineWire"), TEXT("Layout"), World->GetMapName() + TEXT("_layout.png")));
        if (WriteImage(Graph, Layout, StartNode, GoalNode, Path))
        {
            Result->SetStringField(TEXT("imagePath"), Path);
        }
    }
    Bridge->SendAutomationResponse(Socket, RequestId, true, Summary, Result);
    return true;
}
} // namespace McpLevelLayout
