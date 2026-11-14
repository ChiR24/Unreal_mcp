#include "Domains/Navigation/McpNavPrivate.h"

namespace McpNavigationHandlers
{
bool HandleRebuildNavigation(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    UWorld* World = McpHandlerUtils::GetEditorWorld();
    if (!World)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("No editor world available"), TEXT("NO_WORLD"));
        return true;
    }

    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!NavSys)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Navigation system not available"), TEXT("NO_NAV_SYS"));
        return true;
    }

    // Build() returns at once, with a log warning only, in both of these cases (NavigationSystem.cpp, 5.0 to 5.8), and
    // otherwise blocks until every nav data is built.
    if (!NavSys->IsThereAnywhereToBuildNavigation())
    {
        Self->SendAutomationError(Socket, RequestId,
            TEXT("There is nowhere to build navigation: the level has no NavMeshBoundsVolume. Add one (manage_level_structure create_nav_mesh_bounds_volume), then rebuild."),
            TEXT("NO_NAV_BOUNDS"));
        return true;
    }
    if (NavSys->IsNavigationBuildingLocked(static_cast<uint8>(~ENavigationBuildLock::NoUpdateInEditor)))
    {
        Self->SendAutomationError(Socket, RequestId,
            TEXT("Navigation building is locked (levels are still loading or streaming in); nothing was built. Try again once they have loaded."),
            TEXT("NAV_BUILD_LOCKED"));
        return true;
    }
    NavSys->Build();
    const bool bHasNavMesh = Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance()) != nullptr;

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("rebuilding"), NavSys->IsNavigationBuildInProgress());
    Result->SetBoolField(TEXT("hasNavMesh"), bHasNavMesh);

    Self->SendAutomationResponse(Socket, RequestId, true,
        bHasNavMesh ? TEXT("Navigation built") : TEXT("Navigation built, but no nav mesh came of it"), Result);
    return true;
}

bool HandleGetNavigationInfo(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    UWorld* World = McpHandlerUtils::GetEditorWorld();
    if (!World)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("No editor world available"), TEXT("NO_WORLD"));
        return true;
    }

    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    TSharedPtr<FJsonObject> NavInfo = McpHandlerUtils::CreateResultObject();

    if (NavSys)
    {
        ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance());
        if (NavMesh)
        {
            NavInfo->SetNumberField(TEXT("agentRadius"), NavMesh->AgentRadius);
            NavInfo->SetNumberField(TEXT("agentHeight"), NavMesh->AgentHeight);
            NavInfo->SetNumberField(TEXT("agentMaxSlope"), NavMesh->AgentMaxSlope);
            NavInfo->SetNumberField(TEXT("tileSizeUU"), NavMesh->TileSizeUU);
            const FNavMeshResolution Resolution = McpNavMeshResolution(NavMesh);
            NavInfo->SetNumberField(TEXT("cellSize"), Resolution.CellSize);
            NavInfo->SetNumberField(TEXT("cellHeight"), Resolution.CellHeight);
            NavInfo->SetNumberField(TEXT("agentStepHeight"), Resolution.AgentMaxStepHeight);
        }
        NavInfo->SetBoolField(TEXT("isNavigationBuildInProgress"), NavSys->IsNavigationBuildInProgress());
    }

    int32 NavLinkCount = 0;
    for (TActorIterator<ANavLinkProxy> It(World); It; ++It)
    {
        NavLinkCount++;
    }
    NavInfo->SetNumberField(TEXT("navLinkCount"), NavLinkCount);

    int32 BoundsVolumeCount = 0;
    for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
    {
        BoundsVolumeCount++;
    }
    NavInfo->SetNumberField(TEXT("boundsVolumes"), BoundsVolumeCount);
    Result->SetObjectField(TEXT("navMeshInfo"), NavInfo);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Navigation info retrieved"), Result);
    return true;
}
}
