#include "Domains/Environment/McpEnvShared.h"
#include "Core/Routing/McpConsolidatedActionRouting.h"
#include "Domains/Environment/Decals/McpDecalPlacement.h"
#include "Domains/Environment/Modules/McpModuleKit.h"
#include "Domains/Environment/Roads/McpRoadPlan.h"
#include "Domains/Landscape/Erosion/McpTerrainErosion.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpEnvironmentHandlers, Log, All);

using namespace McpEnvironmentHandlers;

bool UEngineWireSubsystem::HandleBuildEnvironmentAction(
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

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("build_environment payload missing."),
                            TEXT("INVALID_PAYLOAD"));
        return true;
    }

    const FString LowerSub = McpConsolidatedActions::GetPayloadSubAction(Payload);

    UE_LOG(LogMcpEnvironmentHandlers, Verbose,
           TEXT("HandleBuildEnvironmentAction: SubAction=%s"), *LowerSub);

    if (LowerSub == TEXT("add_foliage_instances"))
    {
        // A StaticMesh path in meshPath resolves to its auto foliage type, like foliageType does;
        // rebuilding the payload used to drop it.
        const FString FoliageTypePath = McpGetFirstStringField(Payload, {TEXT("foliageTypePath"), TEXT("foliageType"), TEXT("meshPath")});

        TSharedPtr<FJsonObject> FoliagePayload = MakeShared<FJsonObject>(*Payload);
        if (!FoliageTypePath.IsEmpty())
        {
            FoliagePayload->SetStringField(TEXT("foliageTypePath"), FoliageTypePath);
        }
        return HandleAddFoliageInstances(RequestId, TEXT("add_foliage_instances"),
                                         FoliagePayload, RequestingSocket);
    }
    else if (LowerSub == TEXT("get_foliage_instances"))
    {
        return HandleGetFoliageInstances(RequestId, TEXT("get_foliage_instances"), Payload, RequestingSocket);
    }
    else if (LowerSub == TEXT("remove_foliage") || LowerSub == TEXT("remove_foliage_instances"))
    {
        // Passed through whole: rebuilt, an empty `areas` was dropped before the handler's own refusal could see it,
        // and the whole-type branch removed every instance of the type.
        return HandleRemoveFoliage(RequestId, TEXT("remove_foliage"), Payload, RequestingSocket);
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
    else if (LowerSub == TEXT("add_foliage") &&
             (Payload->HasField(TEXT("foliageType")) || Payload->HasField(TEXT("foliageTypePath")) ||
              Payload->HasField(TEXT("locations")) || Payload->HasField(TEXT("location")) ||
              Payload->HasField(TEXT("position")) || Payload->HasField(TEXT("count")) || Payload->HasField(TEXT("radius"))))
    {
        // A type or a placement input means scatter instances; meshPath alone makes the type.
        // Every add_foliage used to build only the type asset and report success.
        TSharedPtr<FJsonObject> PaintPayload = MakeShared<FJsonObject>(*Payload);
        if (!PaintPayload->HasField(TEXT("foliageType")) && !PaintPayload->HasField(TEXT("foliageTypePath")))
        {
            PaintPayload->SetStringField(TEXT("foliageType"), GetJsonStringField(Payload, TEXT("meshPath")));
        }
        return HandlePaintFoliage(RequestId, TEXT("paint_foliage"), PaintPayload, RequestingSocket);
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
    else if (LowerSub == TEXT("assemble_modules"))
    {
        return McpModuleAssembly::HandleAssembleModules(*this, RequestId, Payload, RequestingSocket);
    }
    else if (LowerSub == TEXT("plan_road"))
    {
        return McpRoadPlanning::HandlePlanRoad(*this, RequestId, Payload, RequestingSocket);
    }
    else if (LowerSub == TEXT("place_decals"))
    {
        return McpDecalPlacement::HandlePlaceDecals(*this, RequestId, Payload, RequestingSocket);
    }
    else if (LowerSub == TEXT("erode_landscape"))
    {
        return McpLandscapeErosion::HandleErodeLandscape(*this, RequestId, Payload, RequestingSocket);
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
        return HandleGenerateLODs(RequestId, Payload, RequestingSocket);
    }
    else if (LowerSub == TEXT("bake_lightmap"))
    {
        return HandleBakeLightmap(RequestId, TEXT("bake_lightmap"), Payload,
                                  RequestingSocket);
    }

    return McpEnvironmentHandlers::HandleBuildEnvironmentEditorAction(
        *this, RequestId, LowerSub, Payload, RequestingSocket);
}
