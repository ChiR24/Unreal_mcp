#include "McpAutomationBridge_NavigationHandlersPrivate.h"

#if WITH_EDITOR
namespace McpNavigationHandlers
{
bool HandleConfigureNavMeshSettings(
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
    if (!NavMesh)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("No RecastNavMesh found in level"), nullptr, TEXT("NO_NAVMESH"));
        return true;
    }

    bool bModified = false;
    if (Payload->HasField(TEXT("tileSizeUU")))
    {
        NavMesh->TileSizeUU = GetJsonNumberFieldNav(Payload, TEXT("tileSizeUU"), 1000.0f);
        bModified = true;
    }
    if (Payload->HasField(TEXT("minRegionArea")))
    {
        NavMesh->MinRegionArea = GetJsonNumberFieldNav(Payload, TEXT("minRegionArea"), 0.0f);
        bModified = true;
    }
    if (Payload->HasField(TEXT("mergeRegionSize")))
    {
        NavMesh->MergeRegionSize = GetJsonNumberFieldNav(Payload, TEXT("mergeRegionSize"), 400.0f);
        bModified = true;
    }
    if (Payload->HasField(TEXT("maxSimplificationError")))
    {
        NavMesh->MaxSimplificationError = GetJsonNumberFieldNav(Payload, TEXT("maxSimplificationError"), 1.3f);
        bModified = true;
    }

    if (Payload->HasField(TEXT("cellSize")) || Payload->HasField(TEXT("cellHeight")))
    {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
        FNavMeshResolutionParam& DefaultParams = NavMesh->NavMeshResolutionParams[(uint8)ENavigationDataResolution::Default];
        if (Payload->HasField(TEXT("cellSize")))
        {
            DefaultParams.CellSize = GetJsonNumberFieldNav(Payload, TEXT("cellSize"), 19.0f);
            bModified = true;
        }
        if (Payload->HasField(TEXT("cellHeight")))
        {
            DefaultParams.CellHeight = GetJsonNumberFieldNav(Payload, TEXT("cellHeight"), 10.0f);
            bModified = true;
        }
#else
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        if (Payload->HasField(TEXT("cellSize")))
        {
            NavMesh->CellSize = GetJsonNumberFieldNav(Payload, TEXT("cellSize"), 19.0f);
            bModified = true;
        }
        if (Payload->HasField(TEXT("cellHeight")))
        {
            NavMesh->CellHeight = GetJsonNumberFieldNav(Payload, TEXT("cellHeight"), 10.0f);
            bModified = true;
        }
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
    }

    if (Payload->HasField(TEXT("agentStepHeight")))
    {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        FNavMeshResolutionParam& DefaultParams = NavMesh->NavMeshResolutionParams[(uint8)ENavigationDataResolution::Default];
        DefaultParams.AgentMaxStepHeight = GetJsonNumberFieldNav(Payload, TEXT("agentStepHeight"), 35.0f);
#else
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        NavMesh->AgentMaxStepHeight = GetJsonNumberFieldNav(Payload, TEXT("agentStepHeight"), 35.0f);
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
        bModified = true;
    }

    if (bModified)
    {
        NavMesh->MarkPackageDirty();
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("navMeshName"), NavMesh->GetName());
    Result->SetNumberField(TEXT("tileSizeUU"), NavMesh->TileSizeUU);
    Result->SetBoolField(TEXT("modified"), bModified);
    Result->SetBoolField(TEXT("navMeshPresent"), true);
    Result->SetStringField(TEXT("navMeshPath"), NavMesh->GetPathName());
    Result->SetStringField(TEXT("navMeshClass"), NavMesh->GetClass()->GetName());
    Result->SetBoolField(TEXT("existsAfter"), true);

    Self->SendAutomationResponse(Socket, RequestId, true,
        bModified ? TEXT("NavMesh settings configured") : TEXT("No settings modified"), Result);
    return true;
}

bool HandleSetNavAgentProperties(
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
    if (!NavMesh)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("No RecastNavMesh found in level"), nullptr, TEXT("NO_NAVMESH"));
        return true;
    }

    bool bModified = false;
    if (Payload->HasField(TEXT("agentRadius")))
    {
        NavMesh->AgentRadius = GetJsonNumberFieldNav(Payload, TEXT("agentRadius"), 35.0f);
        bModified = true;
    }
    if (Payload->HasField(TEXT("agentHeight")))
    {
        NavMesh->AgentHeight = GetJsonNumberFieldNav(Payload, TEXT("agentHeight"), 144.0f);
        bModified = true;
    }
    if (Payload->HasField(TEXT("agentMaxSlope")))
    {
        NavMesh->AgentMaxSlope = GetJsonNumberFieldNav(Payload, TEXT("agentMaxSlope"), 44.0f);
        bModified = true;
    }
    if (Payload->HasField(TEXT("agentStepHeight")))
    {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        FNavMeshResolutionParam& DefaultParams = NavMesh->NavMeshResolutionParams[(uint8)ENavigationDataResolution::Default];
        DefaultParams.AgentMaxStepHeight = GetJsonNumberFieldNav(Payload, TEXT("agentStepHeight"), 35.0f);
#else
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        NavMesh->AgentMaxStepHeight = GetJsonNumberFieldNav(Payload, TEXT("agentStepHeight"), 35.0f);
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
        bModified = true;
    }

    if (bModified)
    {
        NavMesh->MarkPackageDirty();
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("agentRadius"), NavMesh->AgentRadius);
    Result->SetNumberField(TEXT("agentHeight"), NavMesh->AgentHeight);
    Result->SetNumberField(TEXT("agentMaxSlope"), NavMesh->AgentMaxSlope);
    Result->SetBoolField(TEXT("navMeshPresent"), true);
    Result->SetStringField(TEXT("navMeshPath"), NavMesh->GetPathName());
    Result->SetBoolField(TEXT("existsAfter"), true);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Nav agent properties set"), Result);
    return true;
}
}
#endif
