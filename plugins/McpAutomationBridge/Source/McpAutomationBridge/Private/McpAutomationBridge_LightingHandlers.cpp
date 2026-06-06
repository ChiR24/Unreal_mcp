#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_LightingHandlersPrivate.h"
#include "McpAutomationBridgeSubsystem.h"

#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Subsystems/EditorActorSubsystem.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleLightingAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString EffectiveAction = Action;
    if (Action.Equals(TEXT("manage_lighting"), ESearchCase::IgnoreCase) && Payload.IsValid())
    {
        FString PayloadAction;
        if (Payload->TryGetStringField(TEXT("action"), PayloadAction) && !PayloadAction.IsEmpty())
        {
            EffectiveAction = PayloadAction;
        }
    }

    const FString Lower = EffectiveAction.ToLower();
    const bool bKnownLightingAction =
        Lower.StartsWith(TEXT("spawn_light")) ||
        Lower.StartsWith(TEXT("spawn_sky_light")) ||
        Lower.StartsWith(TEXT("create_sky_light")) ||
        Lower.StartsWith(TEXT("create_light")) ||
        Lower.StartsWith(TEXT("build_lighting")) ||
        Lower.StartsWith(TEXT("bake_lightmap")) ||
        Lower.StartsWith(TEXT("ensure_single_sky_light")) ||
        Lower.StartsWith(TEXT("create_lighting_enabled_level")) ||
        Lower.StartsWith(TEXT("create_lightmass_volume")) ||
        Lower.StartsWith(TEXT("create_dynamic_light")) ||
        Lower.StartsWith(TEXT("setup_volumetric_fog")) ||
        Lower.StartsWith(TEXT("setup_global_illumination")) ||
        Lower.StartsWith(TEXT("configure_shadows")) ||
        Lower.StartsWith(TEXT("set_exposure")) ||
        Lower.StartsWith(TEXT("list_light_types")) ||
        Lower.StartsWith(TEXT("set_ambient_occlusion"));

    if (!bKnownLightingAction)
    {
        if (Action.Equals(TEXT("manage_lighting"), ESearchCase::IgnoreCase))
        {
            const bool bMissingSubAction =
                EffectiveAction.Equals(TEXT("manage_lighting"), ESearchCase::IgnoreCase);
            SendAutomationError(
                RequestingSocket,
                RequestId,
                bMissingSubAction
                    ? TEXT("manage_lighting requires a non-empty 'action' field in payload")
                    : FString::Printf(TEXT("Unknown manage_lighting action: %s"), *EffectiveAction),
                bMissingSubAction ? TEXT("INVALID_ARGUMENT") : TEXT("UNKNOWN_ACTION"));
            return true;
        }

        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Lighting payload missing"), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS)
    {
        SendAutomationError(
            RequestingSocket,
            RequestId,
            TEXT("EditorActorSubsystem not available"),
            TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
    }

    using namespace McpLightingHandlers;
    if (Lower == TEXT("list_light_types"))
    {
        return HandleListLightTypes(*this, RequestId, RequestingSocket);
    }
    if (Lower == TEXT("spawn_light") || Lower == TEXT("create_light") ||
        Lower == TEXT("create_dynamic_light"))
    {
        return HandleSpawnLight(*this, RequestId, Payload, RequestingSocket);
    }
    if (Lower == TEXT("spawn_sky_light") || Lower == TEXT("create_sky_light"))
    {
        return HandleSpawnSkyLight(*this, RequestId, Payload, RequestingSocket);
    }
    if (Lower == TEXT("build_lighting") || Lower == TEXT("bake_lightmap"))
    {
        return HandleBuildLighting(*this, RequestId, Payload, RequestingSocket);
    }
    if (Lower == TEXT("ensure_single_sky_light"))
    {
        return HandleEnsureSingleSkyLight(*this, RequestId, Payload, RequestingSocket, ActorSS);
    }
    if (Lower == TEXT("create_lightmass_volume"))
    {
        return HandleCreateLightmassVolume(*this, RequestId, Payload, RequestingSocket);
    }
    if (Lower == TEXT("setup_volumetric_fog"))
    {
        return HandleSetupVolumetricFog(*this, RequestId, Payload, RequestingSocket, ActorSS);
    }
    if (Lower == TEXT("setup_global_illumination"))
    {
        return HandleSetupGlobalIllumination(*this, RequestId, Payload, RequestingSocket);
    }
    if (Lower == TEXT("configure_shadows"))
    {
        return HandleConfigureShadows(*this, RequestId, Payload, RequestingSocket);
    }
    if (Lower == TEXT("set_exposure"))
    {
        return HandleSetExposure(*this, RequestId, Payload, RequestingSocket, ActorSS);
    }
    if (Lower == TEXT("set_ambient_occlusion"))
    {
        return HandleSetAmbientOcclusion(*this, RequestId, Payload, RequestingSocket, ActorSS);
    }
    if (Lower == TEXT("create_lighting_enabled_level"))
    {
        return HandleCreateLightingEnabledLevel(*this, RequestId, Payload, RequestingSocket);
    }

    if (Action.Equals(TEXT("manage_lighting"), ESearchCase::IgnoreCase))
    {
        SendAutomationError(
            RequestingSocket,
            RequestId,
            FString::Printf(TEXT("Unknown manage_lighting action: %s"), *EffectiveAction),
            TEXT("UNKNOWN_ACTION"));
        return true;
    }

    return false;
#else
    SendAutomationResponse(
        RequestingSocket,
        RequestId,
        false,
        TEXT("Lighting actions require editor build"),
        nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
