#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {
namespace {

TSharedPtr<FJsonObject> MakeSkyRigPayload(
    const TSharedPtr<FJsonObject> &Payload, const FString &ActorName)
{
    TSharedPtr<FJsonObject> RigPayload = McpHandlerUtils::CreateResultObject();
    RigPayload->Values = Payload->Values;
    RigPayload->RemoveField(TEXT("targetActor"));
    RigPayload->RemoveField(TEXT("waterBodyName"));
    RigPayload->RemoveField(TEXT("name"));
    RigPayload->SetStringField(TEXT("actorName"), ActorName);
    return RigPayload;
}

bool ConfigureSkyRigActor(
    FEnvironmentBuildContext &Context, const FString &ActorClassPath,
    const FString &ComponentClassPath, const FString &ActorName,
    const TCHAR *ResponseName, const TCHAR *PathField,
    bool &bOutCreated, FString &OutMessage, FString &OutErrorCode)
{
    UClass *ActorClass = LoadClass<AActor>(nullptr, *ActorClassPath);
    const bool bExisted = ActorClass && McpFindActorByNameOrClass(ActorClass, ActorName);
    TSharedPtr<FJsonObject> ActorResponse = McpHandlerUtils::CreateResultObject();
    const TSharedPtr<FJsonObject> ActorPayload = MakeSkyRigPayload(Context.Payload, ActorName);
    bool bResult = McpConfigureActorAndComponent(
        ActorPayload, ActorClassPath, ActorName, ComponentClassPath,
        ActorResponse, OutMessage, OutErrorCode);
    FString ComponentPath;
    if (bResult &&
        (!ActorResponse->TryGetStringField(TEXT("componentPath"), ComponentPath) ||
         ComponentPath.IsEmpty()))
    {
        bResult = false;
        OutMessage = FString::Printf(
            TEXT("Required sky rig component could not be configured: %s"),
            *ComponentClassPath);
        OutErrorCode = TEXT("COMPONENT_CREATION_FAILED");
    }
    const TArray<TSharedPtr<FJsonValue>> *ConfigurationErrors = nullptr;
    if (bResult &&
        ActorResponse->TryGetArrayField(TEXT("configurationErrors"), ConfigurationErrors) &&
        ConfigurationErrors && !ConfigurationErrors->IsEmpty())
    {
        bResult = false;
        OutMessage = TEXT("One or more sky rig settings could not be applied");
        OutErrorCode = TEXT("CONFIGURATION_FAILED");
    }
    Context.Resp->SetObjectField(ResponseName, ActorResponse);
    FString ActorPath;
    if (ActorResponse->TryGetStringField(TEXT("actorPath"), ActorPath))
    {
        Context.Resp->SetStringField(PathField, ActorPath);
    }
    bOutCreated = !bExisted && !ActorPath.IsEmpty();
    return bResult;
}

bool CreateSkySphereRig(FEnvironmentBuildContext &Context)
{
    const FString BaseName = McpGetFirstStringField(Context.Payload, {TEXT("name"), TEXT("actorName")});
    const FString AtmosphereName = BaseName.IsEmpty() ? TEXT("SkyAtmosphere") : BaseName + TEXT("_Atmosphere");
    const FString DirectionalName = BaseName.IsEmpty() ? TEXT("DirectionalLight") : BaseName + TEXT("_Sun");
    const FString SkyLightName = BaseName.IsEmpty() ? TEXT("SkyLight") : BaseName + TEXT("_SkyLight");

    bool bAtmosphereCreated = false;
    bool bDirectionalCreated = false;
    bool bSkyLightCreated = false;
    FString AtmosphereMessage;
    FString AtmosphereError;
    FString DirectionalMessage;
    FString DirectionalError;
    FString SkyLightMessage;
    FString SkyLightError;
    const bool bAtmosphereConfigured = ConfigureSkyRigActor(
        Context, TEXT("/Script/Engine.SkyAtmosphere"),
        TEXT("/Script/Engine.SkyAtmosphereComponent"), AtmosphereName,
        TEXT("skyAtmosphere"), TEXT("skyAtmosphereActorPath"),
        bAtmosphereCreated, AtmosphereMessage, AtmosphereError);
    const bool bDirectionalConfigured = ConfigureSkyRigActor(
        Context, TEXT("/Script/Engine.DirectionalLight"),
        TEXT("/Script/Engine.DirectionalLightComponent"), DirectionalName,
        TEXT("directionalLight"), TEXT("directionalLightActorPath"),
        bDirectionalCreated, DirectionalMessage, DirectionalError);
    const bool bSkyLightConfigured = ConfigureSkyRigActor(
        Context, TEXT("/Script/Engine.SkyLight"),
        TEXT("/Script/Engine.SkyLightComponent"), SkyLightName,
        TEXT("skyLight"), TEXT("skyLightActorPath"),
        bSkyLightCreated, SkyLightMessage, SkyLightError);

    const int32 CreatedActorCount =
        (bAtmosphereCreated ? 1 : 0) +
        (bDirectionalCreated ? 1 : 0) +
        (bSkyLightCreated ? 1 : 0);
    const int32 ConfiguredActorCount =
        (bAtmosphereConfigured ? 1 : 0) +
        (bDirectionalConfigured ? 1 : 0) +
        (bSkyLightConfigured ? 1 : 0);
    Context.Resp->SetNumberField(TEXT("createdActorCount"), CreatedActorCount);
    Context.Resp->SetNumberField(TEXT("configuredActorCount"), ConfiguredActorCount);
    // Name the rig actors so callers can address them (dogfood #218).
    Context.Resp->SetStringField(TEXT("atmosphereActor"), AtmosphereName);
    Context.Resp->SetStringField(TEXT("directionalLightActor"), DirectionalName);
    Context.Resp->SetStringField(TEXT("skyLightActor"), SkyLightName);
    Context.bSuccess =
        bAtmosphereConfigured && bDirectionalConfigured && bSkyLightConfigured;
    if (Context.bSuccess)
    {
        Context.Message = TEXT("Sky atmosphere, directional light, and skylight configured");
        Context.ErrorCode.Empty();
    }
    else if (!bAtmosphereConfigured)
    {
        Context.Message = AtmosphereMessage;
        Context.ErrorCode = AtmosphereError;
    }
    else if (!bDirectionalConfigured)
    {
        Context.Message = DirectionalMessage;
        Context.ErrorCode = DirectionalError;
    }
    else
    {
        Context.Message = SkyLightMessage;
        Context.ErrorCode = SkyLightError;
    }
    return true;
}

}

bool HandleBuildSkyWeatherAction(const FString &LowerSub, FEnvironmentBuildContext &Context)
{
    FString Message;
    FString ErrorCode;

    if (LowerSub == TEXT("create_sky_sphere"))
    {
        return CreateSkySphereRig(Context);
    }
    if (LowerSub == TEXT("configure_sky_atmosphere"))
    {
        return ConfigureEnvironmentActor(Context, TEXT("/Script/Engine.SkyAtmosphere"),
            TEXT("SkyAtmosphere"), TEXT("/Script/Engine.SkyAtmosphereComponent"));
    }
    if (LowerSub == TEXT("configure_sky_light"))
    {
        return ConfigureEnvironmentActor(Context, TEXT("/Script/Engine.SkyLight"),
            TEXT("SkyLight"), TEXT("/Script/Engine.SkyLightComponent"));
    }
    if (LowerSub == TEXT("configure_directional_light_atmosphere"))
    {
        return ConfigureEnvironmentActor(Context, TEXT("/Script/Engine.DirectionalLight"),
            TEXT("DirectionalLight"), TEXT("/Script/Engine.DirectionalLightComponent"));
    }
    if (LowerSub == TEXT("create_fog_volume") || LowerSub == TEXT("configure_exponential_height_fog"))
    {
        return ConfigureEnvironmentActor(Context, TEXT("/Script/Engine.ExponentialHeightFog"),
            TEXT("ExponentialHeightFog"), TEXT("/Script/Engine.ExponentialHeightFogComponent"));
    }
    if (LowerSub == TEXT("configure_volumetric_cloud"))
    {
        return ConfigureEnvironmentActor(Context, TEXT("/Script/Engine.VolumetricCloud"),
            TEXT("VolumetricCloud"), TEXT("/Script/Engine.VolumetricCloudComponent"));
    }
    if (LowerSub == TEXT("create_weather_system") || LowerSub == TEXT("configure_rain_particles"))
    {
        const bool bResult = McpConfigureParticleEmitter(
            Context.Payload, TEXT("WeatherEmitter"), Context.Resp, Message, ErrorCode);
        MarkActorConfigurationResult(Context, bResult, Message, ErrorCode);
        return true;
    }
    if (LowerSub == TEXT("configure_snow_particles"))
    {
        const bool bResult = McpConfigureParticleEmitter(
            Context.Payload, TEXT("SnowEmitter"), Context.Resp, Message, ErrorCode);
        MarkActorConfigurationResult(Context, bResult, Message, ErrorCode);
        return true;
    }
    if (LowerSub == TEXT("configure_lightning"))
    {
        const bool bResult = McpConfigureParticleEmitter(
            Context.Payload, TEXT("LightningEmitter"), Context.Resp, Message, ErrorCode);
        MarkActorConfigurationResult(Context, bResult, Message, ErrorCode);
        return true;
    }
    if (LowerSub == TEXT("configure_wind"))
    {
        return ConfigureEnvironmentActor(Context, TEXT("/Script/Engine.WindDirectionalSource"),
            TEXT("WindDirectionalSource"), TEXT("/Script/Engine.WindDirectionalSourceComponent"));
    }
    if (LowerSub == TEXT("create_time_of_day_system"))
    {
        const bool bResult = McpCreateTimeOfDaySystem(Context.Payload, Context.Resp, Message, ErrorCode);
        MarkActorConfigurationResult(Context, bResult, Message, ErrorCode);
        return true;
    }
    if (LowerSub == TEXT("set_time_of_day") || LowerSub == TEXT("configure_sun_position"))
    {
        const bool bResult = McpConfigureSunPosition(Context.Payload, Context.Resp, Message, ErrorCode);
        MarkActorConfigurationResult(Context, bResult, Message, ErrorCode);
        return true;
    }
    if (LowerSub == TEXT("configure_light_color_curve"))
    {
        const bool bResult = McpCreateLinearColorCurve(
            Context.Payload, TEXT("MCP_LightColorCurve"), Context.Resp, Message, ErrorCode);
        MarkActorConfigurationResult(Context, bResult, Message, ErrorCode);
        return true;
    }
    if (LowerSub == TEXT("configure_sky_color_curve"))
    {
        const bool bResult = McpCreateLinearColorCurve(
            Context.Payload, TEXT("MCP_SkyColorCurve"), Context.Resp, Message, ErrorCode);
        MarkActorConfigurationResult(Context, bResult, Message, ErrorCode);
        return true;
    }
    return false;
}

}
#endif
