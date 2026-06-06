#include "McpAutomationBridgeSubsystem.h"

#include "MCP/McpConsolidatedActionRouting.h"

#define MCP_REGISTER_DIRECT(ActionName, MethodName) RegisterHandler(TEXT(ActionName), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return MethodName(R, A, P, S); })

static bool ReadFirstNonEmptyMaterialAlias(
    const TSharedPtr<FJsonObject>& RoutedPayload,
    const TCHAR* First,
    const TCHAR* Second,
    const TCHAR* Third,
    FString& OutValue)
{
    FString Candidate;
    if (RoutedPayload->TryGetStringField(First, Candidate) && !Candidate.IsEmpty())
    {
        OutValue = Candidate;
        return true;
    }
    if (RoutedPayload->TryGetStringField(Second, Candidate) && !Candidate.IsEmpty())
    {
        OutValue = Candidate;
        return true;
    }
    if (RoutedPayload->TryGetStringField(Third, Candidate) && !Candidate.IsEmpty())
    {
        OutValue = Candidate;
        return true;
    }
    return false;
}

static void NormalizeMaterialConnectionAliases(const TSharedPtr<FJsonObject>& RoutedPayload)
{
    if (!RoutedPayload.IsValid())
    {
        return;
    }

    FString ExistingValue;
    FString AliasValue;
    if ((!RoutedPayload->TryGetStringField(TEXT("sourceNodeId"), ExistingValue) || ExistingValue.IsEmpty()) &&
        ReadFirstNonEmptyMaterialAlias(RoutedPayload, TEXT("fromNodeId"), TEXT("fromNode"), TEXT("sourceNode"), AliasValue))
    {
        RoutedPayload->SetStringField(TEXT("sourceNodeId"), AliasValue);
    }
    if ((!RoutedPayload->TryGetStringField(TEXT("targetNodeId"), ExistingValue) || ExistingValue.IsEmpty()) &&
        ReadFirstNonEmptyMaterialAlias(RoutedPayload, TEXT("toNodeId"), TEXT("toNode"), TEXT("targetNode"), AliasValue))
    {
        RoutedPayload->SetStringField(TEXT("targetNodeId"), AliasValue);
    }
    if ((!RoutedPayload->TryGetStringField(TEXT("sourcePin"), ExistingValue) || ExistingValue.IsEmpty()) &&
        ReadFirstNonEmptyMaterialAlias(RoutedPayload, TEXT("fromPin"), TEXT("outputPin"), TEXT("sourceOutputPin"), AliasValue))
    {
        RoutedPayload->SetStringField(TEXT("sourcePin"), AliasValue);
    }
    if ((!RoutedPayload->TryGetStringField(TEXT("inputName"), ExistingValue) || ExistingValue.IsEmpty()) &&
        ReadFirstNonEmptyMaterialAlias(RoutedPayload, TEXT("targetPin"), TEXT("toPin"), TEXT("inputPin"), AliasValue))
    {
        RoutedPayload->SetStringField(TEXT("inputName"), AliasValue);
    }
}

void UMcpAutomationBridgeSubsystem::RegisterAssetRoutingHandlers()
{
    RegisterHandler(
        TEXT("manage_asset"),
        [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S)
        {
            const FString SubAction = McpConsolidatedActions::GetPayloadSubAction(P);
            if (McpConsolidatedActions::IsMaterialAuthoringAction(SubAction))
            {
                FString RoutedSubAction = SubAction;
                if (RoutedSubAction == TEXT("connect_material_pins"))
                {
                    RoutedSubAction = TEXT("connect_nodes");
                }
                else if (RoutedSubAction == TEXT("break_material_connections"))
                {
                    RoutedSubAction = TEXT("disconnect_nodes");
                }
                else if (RoutedSubAction == TEXT("rebuild_material"))
                {
                    RoutedSubAction = TEXT("compile_material");
                }

                const TSharedPtr<FJsonObject> RoutedPayload =
                    McpConsolidatedActions::WithPayloadSubAction(P, RoutedSubAction);
                if (RoutedSubAction == TEXT("connect_nodes"))
                {
                    NormalizeMaterialConnectionAliases(RoutedPayload);
                }
                return HandleManageMaterialAuthoringAction(
                    R,
                    TEXT("manage_material_authoring"),
                    RoutedPayload,
                    S);
            }
            if (McpConsolidatedActions::IsTextureAction(SubAction))
            {
                const TSharedPtr<FJsonObject> RoutedPayload =
                    McpConsolidatedActions::WithPayloadSubAction(P, SubAction);
                return HandleManageTextureAction(R, TEXT("manage_texture"), RoutedPayload, S);
            }
            return HandleAssetAction(R, A, P, S);
        });

    MCP_REGISTER_DIRECT("asset_query", HandleAssetQueryAction);
    MCP_REGISTER_DIRECT("search_assets", HandleSearchAssets);
    MCP_REGISTER_DIRECT("find_by_tag", HandleFindByTag);
    MCP_REGISTER_DIRECT("generate_lods", HandleGenerateLODs);
    MCP_REGISTER_DIRECT("create_thumbnail", HandleGenerateThumbnail);
    MCP_REGISTER_DIRECT("get_source_control_state", HandleGetSourceControlState);
    MCP_REGISTER_DIRECT("manage_material_authoring", HandleManageMaterialAuthoringAction);
}

#undef MCP_REGISTER_DIRECT
