#include "McpAutomationBridge_NavigationHandlersPrivate.h"

#if WITH_EDITOR
namespace McpNavigationHandlers
{
bool HandleRebuildNavigation(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString BlueprintPath = GetJsonStringFieldNav(Payload, TEXT("blueprintPath"));
    if (!BlueprintPath.IsEmpty())
    {
        if (!IsValidNavigationPath(BlueprintPath))
        {
            Self->SendAutomationResponse(Socket, RequestId, false,
                TEXT("Invalid blueprintPath: must not contain path traversal (..) or invalid format"), nullptr, TEXT("SECURITY_VIOLATION"));
            return true;
        }
        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            Self->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), nullptr, TEXT("NOT_FOUND"));
            return true;
        }
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!NavSys)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Navigation system not available"), nullptr, TEXT("NO_NAV_SYS"));
        return true;
    }

    ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance());
    const bool bHasNavMesh = (NavMesh != nullptr);
    NavSys->Build();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("rebuilding"), NavSys->IsNavigationBuildInProgress());
    Result->SetBoolField(TEXT("hasNavMesh"), bHasNavMesh);
    Result->SetBoolField(TEXT("navMeshPresent"), bHasNavMesh);
    Result->SetBoolField(TEXT("bHasNavMesh"), bHasNavMesh);
    Result->SetStringField(TEXT("navigationSystemPath"), NavSys->GetPathName());
    Result->SetBoolField(TEXT("existsAfter"), true);

    Self->SendAutomationResponse(Socket, RequestId, true,
        bHasNavMesh ? TEXT("Navigation rebuild initiated") : TEXT("Navigation rebuild initiated (no existing NavMesh - ensure NavMeshBoundsVolume is present)"), Result);
    return true;
}

bool HandleGetNavigationInfo(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString BlueprintPath = GetJsonStringFieldNav(Payload, TEXT("blueprintPath"));
    if (!BlueprintPath.IsEmpty())
    {
        if (!IsValidNavigationPath(BlueprintPath))
        {
            Self->SendAutomationResponse(Socket, RequestId, false,
                TEXT("Invalid blueprintPath: must not contain path traversal (..) or invalid format"), nullptr, TEXT("SECURITY_VIOLATION"));
            return true;
        }
        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            Self->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), nullptr, TEXT("NOT_FOUND"));
            return true;
        }
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
            const FNavMeshResolutionParam& DefaultParams = NavMesh->NavMeshResolutionParams[(uint8)ENavigationDataResolution::Default];
            NavInfo->SetNumberField(TEXT("cellSize"), DefaultParams.CellSize);
            NavInfo->SetNumberField(TEXT("cellHeight"), DefaultParams.CellHeight);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
            NavInfo->SetNumberField(TEXT("agentStepHeight"), DefaultParams.AgentMaxStepHeight);
#else
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            NavInfo->SetNumberField(TEXT("agentStepHeight"), NavMesh->AgentMaxStepHeight);
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
#else
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            NavInfo->SetNumberField(TEXT("cellSize"), NavMesh->CellSize);
            NavInfo->SetNumberField(TEXT("cellHeight"), NavMesh->CellHeight);
            NavInfo->SetNumberField(TEXT("agentStepHeight"), NavMesh->AgentMaxStepHeight);
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
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
#endif
