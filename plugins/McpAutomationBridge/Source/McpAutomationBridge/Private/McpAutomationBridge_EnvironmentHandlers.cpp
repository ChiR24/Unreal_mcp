#include "McpAutomationBridge_EnvironmentHandlersShared.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpEnvironmentHandlers, Log, All);

using namespace McpEnvironmentHandlers;

bool UMcpAutomationBridgeSubsystem::HandleBuildEnvironmentAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("build_environment"), ESearchCase::IgnoreCase) &&
        !Lower.StartsWith(TEXT("build_environment")))
    {
        return false;
    }

    // Validate payload
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("build_environment payload missing."),
                            TEXT("INVALID_PAYLOAD"));
        return true;
    }

    // Extract sub-action
    FString SubAction;
    Payload->TryGetStringField(TEXT("action"), SubAction);
    const FString LowerSub = SubAction.ToLower();

    UE_LOG(LogMcpEnvironmentHandlers, Verbose,
           TEXT("HandleBuildEnvironmentAction: SubAction=%s"), *LowerSub);

    // =========================================================================
    // Foliage Sub-actions (dispatch to dedicated handlers)
    // =========================================================================
    if (LowerSub == TEXT("add_foliage_instances"))
    {
        FString FoliageTypePath;
        if (!Payload->TryGetStringField(TEXT("foliageTypePath"), FoliageTypePath) ||
            FoliageTypePath.IsEmpty())
        {
            Payload->TryGetStringField(TEXT("foliageType"), FoliageTypePath);
        }

        TSharedPtr<FJsonObject> FoliagePayload = McpHandlerUtils::CreateResultObject();
        if (!FoliageTypePath.IsEmpty())
        {
            FoliagePayload->SetStringField(TEXT("foliageTypePath"), FoliageTypePath);
        }

        // Preserve full transform data so callers can specify rotation and scale.
        const TArray<TSharedPtr<FJsonValue>> *Transforms = nullptr;
        Payload->TryGetArrayField(TEXT("transforms"), Transforms);
        if (Transforms)
        {
            FoliagePayload->SetArrayField(TEXT("transforms"), *Transforms);
        }

        const TArray<TSharedPtr<FJsonValue>> *Locations = nullptr;
        if (Payload->TryGetArrayField(TEXT("locations"), Locations) && Locations)
        {
            FoliagePayload->SetArrayField(TEXT("locations"), *Locations);
        }

        return HandleAddFoliageInstances(RequestId, TEXT("add_foliage_instances"),
                                         FoliagePayload, RequestingSocket);
    }
    else if (LowerSub == TEXT("get_foliage_instances"))
    {
        FString FoliageTypePath;
        Payload->TryGetStringField(TEXT("foliageType"), FoliageTypePath);
        TSharedPtr<FJsonObject> FoliagePayload = McpHandlerUtils::CreateResultObject();
        if (!FoliageTypePath.IsEmpty())
        {
            FoliagePayload->SetStringField(TEXT("foliageTypePath"), FoliageTypePath);
        }
        return HandleGetFoliageInstances(RequestId, TEXT("get_foliage_instances"),
                                         FoliagePayload, RequestingSocket);
    }
    else if (LowerSub == TEXT("remove_foliage") || LowerSub == TEXT("remove_foliage_instances"))
    {
        FString FoliageTypePath = McpGetFirstStringField(Payload, {TEXT("foliageTypePath"), TEXT("foliageType")});
        bool bRemoveAll = false;
        Payload->TryGetBoolField(TEXT("removeAll"), bRemoveAll);

        TSharedPtr<FJsonObject> FoliagePayload = McpHandlerUtils::CreateResultObject();
        if (!FoliageTypePath.IsEmpty())
        {
            FoliagePayload->SetStringField(TEXT("foliageTypePath"), FoliageTypePath);
        }
        if (LowerSub == TEXT("remove_foliage_instances") && FoliageTypePath.IsEmpty() && !bRemoveAll)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false,
                                   TEXT("remove_foliage_instances requires foliageTypePath/foliageType or removeAll=true"),
                                   FoliagePayload, TEXT("INVALID_ARGUMENT"));
            return true;
        }
        FoliagePayload->SetBoolField(TEXT("removeAll"), bRemoveAll);
        return HandleRemoveFoliage(RequestId, TEXT("remove_foliage"),
                                   FoliagePayload, RequestingSocket);
    }
    else if (LowerSub == TEXT("paint_foliage") || LowerSub == TEXT("paint_foliage_instances"))
    {
        return HandlePaintFoliage(RequestId, TEXT("paint_foliage"), Payload,
                                  RequestingSocket);
    }
    else if (LowerSub == TEXT("create_procedural_foliage"))
    {
        return HandleCreateProceduralFoliage(RequestId,
                                             TEXT("create_procedural_foliage"),
                                             Payload, RequestingSocket);
    }
    else if (LowerSub == TEXT("create_procedural_terrain"))
    {
        return HandleCreateProceduralTerrain(RequestId,
                                             TEXT("create_procedural_terrain"),
                                             Payload, RequestingSocket);
    }
    else if (LowerSub == TEXT("add_foliage_type") || LowerSub == TEXT("add_foliage") || LowerSub == TEXT("create_foliage_type"))
    {
        return HandleAddFoliageType(RequestId, TEXT("add_foliage_type"),
                                    Payload, RequestingSocket);
    }
    else if (LowerSub == TEXT("create_landscape"))
    {
        return HandleCreateLandscape(RequestId, TEXT("create_landscape"),
                                     Payload, RequestingSocket);
    }

    // =========================================================================
    // Landscape Operations (dispatch to dedicated handlers)
    // =========================================================================
    else if (LowerSub == TEXT("paint_landscape") ||
             LowerSub == TEXT("paint_landscape_layer"))
    {
        return HandlePaintLandscapeLayer(RequestId, TEXT("paint_landscape_layer"),
                                         Payload, RequestingSocket);
    }
    else if (LowerSub == TEXT("sculpt_landscape") || LowerSub == TEXT("sculpt"))
    {
        return HandleSculptLandscape(RequestId, TEXT("sculpt_landscape"), Payload,
                                     RequestingSocket);
    }
    else if (LowerSub == TEXT("modify_heightmap"))
    {
        return HandleModifyHeightmap(RequestId, TEXT("modify_heightmap"), Payload,
                                     RequestingSocket);
    }
    else if (LowerSub == TEXT("set_landscape_material") || LowerSub == TEXT("configure_landscape_material"))
    {
        return HandleSetLandscapeMaterial(RequestId, TEXT("set_landscape_material"),
                                          Payload, RequestingSocket);
    }
    else if (LowerSub == TEXT("create_landscape_grass_type"))
    {
        return HandleCreateLandscapeGrassType(RequestId,
                                              TEXT("create_landscape_grass_type"),
                                              Payload, RequestingSocket);
    }
    else if (LowerSub == TEXT("generate_lods"))
    {
        return HandleGenerateLODs(RequestId, TEXT("generate_lods"), Payload,
                                  RequestingSocket);
    }
    else if (LowerSub == TEXT("bake_lightmap"))
    {
        return HandleBakeLightmap(RequestId, TEXT("bake_lightmap"), Payload,
                                  RequestingSocket);
    }

#if WITH_EDITOR
    return McpEnvironmentHandlers::HandleBuildEnvironmentEditorAction(
        *this, RequestId, LowerSub, Payload, RequestingSocket);
#else
    SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("Environment building actions require editor build."), nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
