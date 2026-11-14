#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Core/Routing/McpConsolidatedActionRouting.h"

#include "Domains/Lighting/McpLightingPrivate.h"
#include "EngineWireSubsystem.h"

#include "Dom/JsonObject.h"

bool UEngineWireSubsystem::HandleLightingAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString EffectiveAction = Action;
    if (Action.Equals(TEXT("manage_lighting"), ESearchCase::IgnoreCase))
    {
        // subAction first, then `action` as the fallback - the same priority the
        // pre-queue gate's NormalizeAction uses, so a split payload can never
        // route this dispatcher away from what the gate authorized.
        const FString PayloadAction = McpConsolidatedActions::GetPayloadSubAction(Payload);
        if (!PayloadAction.IsEmpty())
        {
            EffectiveAction = PayloadAction;
        }
    }

    // The handler table sends the names of the shared Lighting() set, HandleBakeLightmap none.
    using namespace McpLightingHandlers;
    using FHandler = bool (*)(UEngineWireSubsystem&, const FString&, const TSharedPtr<FJsonObject>&, TSharedPtr<FMcpBridgeWebSocket>);
    static const TMap<FString, FHandler> Handlers = {
        {TEXT("list_light_types"), &HandleListLightTypes},
        {TEXT("spawn_light"), &HandleSpawnLight},
        {TEXT("create_light"), &HandleSpawnLight},
        {TEXT("create_dynamic_light"), &HandleSpawnLight},
        {TEXT("spawn_sky_light"), &HandleSpawnSkyLight},
        {TEXT("create_sky_light"), &HandleSpawnSkyLight},
        {TEXT("build_lighting"), &HandleBuildLighting},
        {TEXT("ensure_single_sky_light"), &HandleEnsureSingleSkyLight},
        {TEXT("create_lightmass_volume"), &HandleCreateLightmassVolume},
        {TEXT("setup_volumetric_fog"), &HandleSetupVolumetricFog},
        {TEXT("setup_global_illumination"), &HandleSetupGlobalIllumination},
        {TEXT("configure_shadows"), &HandleConfigureShadows},
        {TEXT("set_exposure"), &HandleSetExposure},
        {TEXT("set_ambient_occlusion"), &HandleSetAmbientOcclusion},
        {TEXT("create_lighting_enabled_level"), &HandleCreateLightingEnabledLevel}};
    const FHandler* Handler = Handlers.Find(EffectiveAction);
    return Handler && (*Handler)(*this, RequestId, Payload, RequestingSocket);
}

bool UEngineWireSubsystem::HandleBakeLightmap(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
    if (Payload.IsValid()) { P->Values = Payload->Values; }
    if (!P->HasField(TEXT("quality"))) { P->SetStringField(TEXT("quality"), TEXT("Preview")); }
    return McpLightingHandlers::HandleBuildLighting(*this, RequestId, P, RequestingSocket);
}
