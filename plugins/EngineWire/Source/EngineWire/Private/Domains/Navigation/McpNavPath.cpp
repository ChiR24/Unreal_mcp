#include "Domains/Navigation/McpNavPath.h"

#include "Domains/Navigation/McpNavPrivate.h"
#include "NavigationPath.h"

// get_ai_info info:'path' (get_navigation_path): whether a pawn could walk from one point to another on the editor
// level's nav mesh, and why not: an end off the mesh, no path at all, or a partial one that stops short (how far
// short). Reads only.
namespace McpNavigationHandlers
{
namespace NavPathParts
{
constexpr int32 MaxPoints = 100;

TArray<TSharedPtr<FJsonValue>> Point(const FVector& Value)
{
    return {MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Value.X)), MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Value.Y)),
            MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Value.Z))};
}

// An end given as a point, or as an actor (where it stands).
bool ReadEnd(const TSharedPtr<FJsonObject>& Payload, const TCHAR* PointField, const TCHAR* ActorField, UWorld* World,
             FVector& Out, FString& OutError)
{
    FString ActorName;
    if (Payload->TryGetStringField(ActorField, ActorName) && !ActorName.IsEmpty())
    {
        const AActor* Actor = FindActorByNameInWorldForMcp(World, ActorName, false);
        if (!Actor)
        {
            OutError = FString::Printf(TEXT("%s '%s' names no actor in the level."), ActorField, *ActorName);
            return false;
        }
        Out = Actor->GetActorLocation();
        return true;
    }
    if (!Payload->HasField(PointField))
    {
        OutError = FString::Printf(TEXT("Give %s (a point) or %s (an actor)."), PointField, ActorField);
        return false;
    }
    Out = ExtractVectorField(Payload, PointField, FVector::ZeroVector);
    return true;
}
} // namespace NavPathParts

using namespace NavPathParts;

bool FindNavPath(UWorld* World, const FVector& From, const FVector& To, const FVector& Extent, FMcpNavPath& Out,
                 FString& OutCode, FString& OutMessage)
{
    UNavigationSystemV1* NavSys = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
    ANavigationData* NavData = NavSys ? NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
    if (!NavData)
    {
        OutCode = TEXT("NAV_DATA_MISSING");
        OutMessage = TEXT("The level has no navigation data: put a Nav Mesh Bounds Volume around the walkable area and build it "
                          "(manage_ai configure_navigation).");
        return false;
    }
    if (NavSys->IsNavigationBuildInProgress())
    {
        OutCode = TEXT("NAV_BUILD_IN_PROGRESS");
        OutMessage = TEXT("The nav mesh is still building, so a path read now could be stale: ask again once it is built.");
        return false;
    }
    Out.NavData = NavData->GetName();
    FNavLocation FromNav;
    FNavLocation ToNav;
    Out.bFromOnMesh = NavSys->ProjectPointToNavigation(From, FromNav, Extent, NavData);
    Out.bToOnMesh = NavSys->ProjectPointToNavigation(To, ToNav, Extent, NavData);
    Out.FromOnMesh = FromNav.Location;
    Out.ToOnMesh = ToNav.Location;
    if (!Out.bFromOnMesh || !Out.bToOnMesh)
    {
        Out.Reason = Out.bFromOnMesh ? TEXT("goal_off_navmesh") : TEXT("start_off_navmesh");
        return true;
    }
    const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, FromNav.Location, ToNav.Location);
    if (!Path || !Path->IsValid() || Path->PathPoints.Num() == 0)
    {
        Out.Reason = TEXT("no_path");
        return true;
    }
    Out.Points = Path->PathPoints;
    Out.Length = Path->GetPathLength(); // float in 5.0 and 5.1, double from 5.2
    Out.Gap = Path->IsPartial() ? FVector::Dist(Path->PathPoints.Last(), ToNav.Location) : 0.0;
    Out.Reason = Path->IsPartial() ? TEXT("partial") : TEXT("ok");
    return true;
}

bool HandleGetNavigationPath(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                             TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    FVector From = FVector::ZeroVector;
    FVector To = FVector::ZeroVector;
    FString Error;
    FString Code = TEXT("INVALID_ARGUMENT");
    FMcpNavPath Path;
    if (!ReadEnd(Payload, TEXT("from"), TEXT("fromActor"), World, From, Error) || !ReadEnd(Payload, TEXT("to"), TEXT("toActor"), World, To, Error) ||
        !FindNavPath(World, From, To, ExtractVectorField(Payload, TEXT("projectExtent"), FVector(100.0, 100.0, 250.0)), Path, Code, Error))
    {
        Self->SendAutomationResponse(Socket, RequestId, false, Error, nullptr, Code);
        return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("reachable"), Path.Reason == TEXT("ok"));
    Result->SetStringField(TEXT("reason"), Path.Reason);
    Result->SetStringField(TEXT("navData"), Path.NavData);
    if (Path.bFromOnMesh)
    {
        Result->SetArrayField(TEXT("fromOnMesh"), Point(Path.FromOnMesh));
    }
    if (Path.bToOnMesh)
    {
        Result->SetArrayField(TEXT("toOnMesh"), Point(Path.ToOnMesh));
    }
    if (Path.Points.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> Points;
        for (int32 Index = 0; Index < Path.Points.Num() && Points.Num() < MaxPoints; ++Index)
        {
            Points.Add(MakeShared<FJsonValueArray>(Point(Path.Points[Index])));
        }
        Result->SetArrayField(TEXT("points"), Points);
        Result->SetNumberField(TEXT("pathLength"), FMath::RoundToDouble(Path.Length));
    }
    if (Path.Reason == TEXT("partial"))
    {
        Result->SetNumberField(TEXT("gap"), FMath::RoundToDouble(Path.Gap));
    }
    const FString Message = Path.Reason == TEXT("ok") ? FString::Printf(TEXT("Reachable: a %.0f cm path."), Path.Length)
                          : Path.Reason == TEXT("partial") ? FString::Printf(TEXT("Partial path: it stops %.0f cm short of the goal."), Path.Gap)
                          : Path.Reason == TEXT("no_path") ? FString(TEXT("No path: both ends are on the nav mesh, but nothing connects them."))
                          : FString::Printf(TEXT("No path: the %s is not on the nav mesh within projectExtent."),
                                            Path.Reason == TEXT("goal_off_navmesh") ? TEXT("goal") : TEXT("start"));
    Self->SendAutomationResponse(Socket, RequestId, true, Message, Result);
    return true;
}
} // namespace McpNavigationHandlers
