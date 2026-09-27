#include "Domains/Navigation/McpAutomationBridge_NavigationHandlersPrivate.h"

namespace McpNavigationHandlers
{
namespace
{
// configure_nav_mesh_settings (generation fields) and set_nav_agent_properties (agent fields) on the editor world's
// default RecastNavMesh; both take agentStepHeight, and only the fields sent change.
bool ConfigureNavMesh(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    bool bAgentProperties)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    UNavigationSystemV1* NavSys = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
    ARecastNavMesh* NavMesh = NavSys ? Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance()) : nullptr;
    if (!NavMesh)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            !World ? TEXT("No editor world available") : !NavSys ? TEXT("Navigation system not available") : TEXT("No RecastNavMesh found in level"),
            nullptr, !World ? TEXT("NO_WORLD") : !NavSys ? TEXT("NO_NAV_SYS") : TEXT("NO_NAVMESH"));
        return true;
    }

    bool bModified = false;
    const auto Apply = [&](const TCHAR* Field, auto& Target)
    {
        double Value = 0.0;
        if (Payload->TryGetNumberField(Field, Value))
        {
            Target = static_cast<std::remove_reference_t<decltype(Target)>>(Value);
            bModified = true;
        }
    };
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
    FNavMeshResolutionParam& DefaultParams = NavMesh->NavMeshResolutionParams[(uint8)ENavigationDataResolution::Default];
#endif
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    Apply(TEXT("agentStepHeight"), DefaultParams.AgentMaxStepHeight);
#else
    Apply(TEXT("agentStepHeight"), NavMesh->AgentMaxStepHeight);
#endif
    if (bAgentProperties)
    {
        Apply(TEXT("agentRadius"), NavMesh->AgentRadius);
        Apply(TEXT("agentHeight"), NavMesh->AgentHeight);
        Apply(TEXT("agentMaxSlope"), NavMesh->AgentMaxSlope);
    }
    else
    {
        Apply(TEXT("tileSizeUU"), NavMesh->TileSizeUU);
        Apply(TEXT("minRegionArea"), NavMesh->MinRegionArea);
        Apply(TEXT("mergeRegionSize"), NavMesh->MergeRegionSize);
        Apply(TEXT("maxSimplificationError"), NavMesh->MaxSimplificationError);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
        Apply(TEXT("cellSize"), DefaultParams.CellSize);
        Apply(TEXT("cellHeight"), DefaultParams.CellHeight);
#else
        Apply(TEXT("cellSize"), NavMesh->CellSize);
        Apply(TEXT("cellHeight"), NavMesh->CellHeight);
#endif
    }
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
    if (bModified)
    {
        NavMesh->MarkPackageDirty();
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("navMeshPath"), NavMesh->GetPathName());
    Result->SetBoolField(TEXT("modified"), bModified);
    if (bAgentProperties)
    {
        Result->SetNumberField(TEXT("agentRadius"), NavMesh->AgentRadius);
        Result->SetNumberField(TEXT("agentHeight"), NavMesh->AgentHeight);
        Result->SetNumberField(TEXT("agentMaxSlope"), NavMesh->AgentMaxSlope);
    }
    else
    {
        Result->SetStringField(TEXT("navMeshName"), NavMesh->GetName());
        Result->SetStringField(TEXT("navMeshClass"), NavMesh->GetClass()->GetName());
        Result->SetNumberField(TEXT("tileSizeUU"), NavMesh->TileSizeUU);
    }
    Self->SendAutomationResponse(Socket, RequestId, true,
        bAgentProperties ? TEXT("Nav agent properties set")
        : bModified ? TEXT("NavMesh settings configured") : TEXT("No settings modified"), Result);
    return true;
}
}

bool HandleConfigureNavMeshSettings(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return ConfigureNavMesh(Self, RequestId, Payload, Socket, false);
}

bool HandleSetNavAgentProperties(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return ConfigureNavMesh(Self, RequestId, Payload, Socket, true);
}
}
