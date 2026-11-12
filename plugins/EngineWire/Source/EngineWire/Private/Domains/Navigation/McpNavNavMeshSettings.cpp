#include "Domains/Navigation/McpNavPrivate.h"
#include "Foundation/Reflection/McpPropertyReflection.h"

namespace McpNavigationHandlers
{
namespace
{
// configure_nav_mesh_settings (generation fields) and set_nav_agent_properties (agent fields) on the editor world's
// default RecastNavMesh; both take agentStepHeight, and only the fields sent change.
bool ConfigureNavMesh(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    bool bAgentProperties)
{
    UWorld* World = McpHandlerUtils::GetEditorWorld();
    UNavigationSystemV1* NavSys = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
    ARecastNavMesh* NavMesh = NavSys ? Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance()) : nullptr;
    if (!NavMesh)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            !World ? TEXT("No editor world available") : !NavSys ? TEXT("Navigation system not available") : TEXT("No RecastNavMesh found in level"),
            nullptr, !World ? TEXT("NO_WORLD") : !NavSys ? TEXT("NO_NAV_SYS") : TEXT("NO_NAVMESH"));
        return true;
    }

    // Two passes over the fields sent. Every value is first checked against the ClampMin and ClampMax of its property,
    // which the engine holds it to in the Details panel only: a cellSize of 0 divides by zero in the generator, and
    // 0.01 asks for a hundred thousand cells along each side of a tile. Only then is anything written.
    bool bModified = false;
    bool bCheckOnly = true;
    FString RangeError;
    const auto Apply = [&](const TCHAR* Field, const UStruct* Owner, const FName PropertyName, auto& Target)
    {
        double Value = 0.0;
        if (!Payload->TryGetNumberField(Field, Value))
        {
            return;
        }
        if (bCheckOnly)
        {
            const FProperty* Property = Owner->FindPropertyByName(PropertyName);
            if (Property && RangeError.IsEmpty())
            {
                McpPropertyReflection::CheckRange(Property, Value, Field, RangeError);
            }
            return;
        }
        Target = static_cast<std::remove_reference_t<decltype(Target)>>(Value);
        bModified = true;
    };
    const UStruct* MeshType = ARecastNavMesh::StaticClass();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
    FNavMeshResolutionParam& DefaultParams = NavMesh->NavMeshResolutionParams[(uint8)ENavigationDataResolution::Default];
    const UStruct* ParamType = FNavMeshResolutionParam::StaticStruct();
#endif
    const auto ApplyFields = [&]()
    {
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        Apply(TEXT("agentStepHeight"), ParamType, TEXT("AgentMaxStepHeight"), DefaultParams.AgentMaxStepHeight);
#else
        Apply(TEXT("agentStepHeight"), MeshType, TEXT("AgentMaxStepHeight"), NavMesh->AgentMaxStepHeight);
#endif
        if (bAgentProperties)
        {
            Apply(TEXT("agentRadius"), MeshType, TEXT("AgentRadius"), NavMesh->AgentRadius);
            Apply(TEXT("agentHeight"), MeshType, TEXT("AgentHeight"), NavMesh->AgentHeight);
            Apply(TEXT("agentMaxSlope"), MeshType, TEXT("AgentMaxSlope"), NavMesh->AgentMaxSlope);
        }
        else
        {
            Apply(TEXT("tileSizeUU"), MeshType, TEXT("TileSizeUU"), NavMesh->TileSizeUU);
            Apply(TEXT("minRegionArea"), MeshType, TEXT("MinRegionArea"), NavMesh->MinRegionArea);
            Apply(TEXT("mergeRegionSize"), MeshType, TEXT("MergeRegionSize"), NavMesh->MergeRegionSize);
            Apply(TEXT("maxSimplificationError"), MeshType, TEXT("MaxSimplificationError"), NavMesh->MaxSimplificationError);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
            Apply(TEXT("cellSize"), ParamType, TEXT("CellSize"), DefaultParams.CellSize);
            Apply(TEXT("cellHeight"), ParamType, TEXT("CellHeight"), DefaultParams.CellHeight);
#else
            Apply(TEXT("cellSize"), MeshType, TEXT("CellSize"), NavMesh->CellSize);
            Apply(TEXT("cellHeight"), MeshType, TEXT("CellHeight"), NavMesh->CellHeight);
#endif
        }
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
    };
    ApplyFields();
    if (!RangeError.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, RangeError + TEXT(". Nothing was changed."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    bCheckOnly = false;
    ApplyFields();
    if (bModified)
    {
        // What the editor does on its own edit of a generation property: the cell sizes are matched to the tile, the
        // nav data config is refilled, and the mesh rebuilds when auto-update is on. Without it the stored numbers and
        // the built mesh disagreed until someone rebuilt.
        if (!bAgentProperties)
        {
            FPropertyChangedEvent Changed(MeshType->FindPropertyByName(GET_MEMBER_NAME_CHECKED(ARecastNavMesh, TileSizeUU)), EPropertyChangeType::ValueSet);
            NavMesh->PostEditChangeProperty(Changed);
        }
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
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return ConfigureNavMesh(Self, RequestId, Payload, Socket, false);
}

bool HandleSetNavAgentProperties(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return ConfigureNavMesh(Self, RequestId, Payload, Socket, true);
}
}
