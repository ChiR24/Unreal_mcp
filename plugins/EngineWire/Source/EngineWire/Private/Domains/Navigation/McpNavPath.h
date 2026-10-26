#pragma once

#include "CoreMinimal.h"

class UWorld;

// The path a pawn would walk on a world's nav mesh (get_ai_info info:'path', and what drives a pawn in Play).
namespace McpNavigationHandlers
{
struct FMcpNavPath
{
    FString Reason; // ok, start_off_navmesh, goal_off_navmesh, no_path or partial
    bool bFromOnMesh = false;
    bool bToOnMesh = false;
    FVector FromOnMesh = FVector::ZeroVector;
    FVector ToOnMesh = FVector::ZeroVector;
    TArray<FVector> Points;
    double Length = 0.0;
    double Gap = 0.0; // partial: how far the path stops short of the goal
    FString NavData;
};

/** From to To on World's nav mesh, each end found within Extent; false with OutCode (NAV_DATA_MISSING,
 *  NAV_BUILD_IN_PROGRESS) and OutMessage when the nav mesh cannot answer. */
bool FindNavPath(UWorld* World, const FVector& From, const FVector& To, const FVector& Extent, FMcpNavPath& Out,
                 FString& OutCode, FString& OutMessage);
} // namespace McpNavigationHandlers
