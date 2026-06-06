#include "McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {

bool HandleBuildSkyWeatherAction(const FString &LowerSub, FEnvironmentBuildContext &Context)
{
    const TSharedPtr<FJsonObject> &Payload = Context.Payload;
    TSharedPtr<FJsonObject> &Resp = Context.Resp;
    bool &bSuccess = Context.bSuccess;
    FString &Message = Context.Message;
    FString &ErrorCode = Context.ErrorCode;

    if (LowerSub == TEXT("create_sky_sphere"))
    {
        // Initialize to false - only set true on successful creation
        bSuccess = false;

        if (!GEditor)
        {
            Message = TEXT("Editor not available");
            ErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
        }
        else
        {
            UClass *SkySphereClass = LoadClass<AActor>(
                nullptr, TEXT("/Script/Engine.Blueprint'/Engine/Maps/Templates/"
                              "SkySphere.SkySphere_C'"));
            if (!SkySphereClass)
            {
                FString RequestedName = TEXT("SkySphere");
                Payload->TryGetStringField(TEXT("name"), RequestedName);

                ADirectionalLight *SunLight = Cast<ADirectionalLight>(
                    SpawnActorInActiveWorld<AActor>(ADirectionalLight::StaticClass(),
                                                    FVector::ZeroVector,
                                                    FRotator(-45.0f, -35.0f, 0.0f),
                                                    TEXT("SkySunLight")));
                ASkyLight *SkyLight = Cast<ASkyLight>(
                    SpawnActorInActiveWorld<AActor>(ASkyLight::StaticClass(),
                                                    FVector::ZeroVector,
                                                    FRotator::ZeroRotator,
                                                    TEXT("SkyLight")));

                if (SunLight && SkyLight)
                {
                    SunLight->SetActorLabel(FString::Printf(TEXT("%s_Sun"), *RequestedName));
                    SkyLight->SetActorLabel(FString::Printf(TEXT("%s_SkyLight"), *RequestedName));

                    if (UDirectionalLightComponent *SunComp =
                            Cast<UDirectionalLightComponent>(SunLight->GetLightComponent()))
                    {
                        SunComp->SetIntensity(10.0f);
                        SunComp->MarkRenderStateDirty();
                    }
                    if (USkyLightComponent *SkyComp = SkyLight->GetLightComponent())
                    {
                        SkyComp->SetIntensity(1.0f);
                        SkyComp->MarkRenderStateDirty();
                    }

                    bSuccess = true;
                    Message = TEXT("Native sky lighting rig created");
                    Resp->SetBoolField(TEXT("fallbackUsed"), true);
                    Resp->SetStringField(TEXT("missingAsset"), TEXT("/Engine/Maps/Templates/SkySphere"));
                    Resp->SetStringField(TEXT("actorName"), RequestedName);
                    Resp->SetStringField(TEXT("sunActorName"), SunLight->GetActorLabel());
                    Resp->SetStringField(TEXT("skyLightActorName"), SkyLight->GetActorLabel());
                    McpHandlerUtils::AddVerification(Resp, SunLight);
                }
                else
                {
                    Message = TEXT("SkySphere class not found and native sky rig fallback failed");
                    ErrorCode = TEXT("SPAWN_FAILED");
                    Resp->SetStringField(TEXT("missingAsset"), TEXT("/Engine/Maps/Templates/SkySphere"));
                }
            }
            else
            {
                AActor *SkySphere = SpawnActorInActiveWorld<AActor>(
                    SkySphereClass, FVector::ZeroVector, FRotator::ZeroRotator,
                    TEXT("SkySphere"));
                if (SkySphere)
                {
                    bSuccess = true;
                    Message = TEXT("Sky sphere created");
                    Resp->SetStringField(TEXT("actorName"), SkySphere->GetActorLabel());
                }
                else
                {
                    Message = TEXT("Failed to spawn sky sphere actor");
                    ErrorCode = TEXT("SPAWN_FAILED");
                }
            }
        }
    }
    // -------------------------------------------------------------------------
    // set_time_of_day: Set time of day on sky sphere
    // -------------------------------------------------------------------------
    else if (LowerSub == TEXT("set_time_of_day"))
    {
        float TimeOfDay = 12.0f;
        if (!Payload->TryGetNumberField(TEXT("time"), TimeOfDay))
        {
            Payload->TryGetNumberField(TEXT("hour"), TimeOfDay);
        }

        if (GEditor)
        {
            UEditorActorSubsystem *ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
            if (ActorSS)
            {
                for (AActor *Actor : ActorSS->GetAllLevelActors())
                {
                    if (Actor->GetClass()->GetName().Contains(TEXT("SkySphere")))
                    {
                        UFunction *SetTimeFunction = Actor->FindFunction(TEXT("SetTimeOfDay"));
                        if (SetTimeFunction)
                        {
                            float TimeParam = TimeOfDay;
                            Actor->ProcessEvent(SetTimeFunction, &TimeParam);
                            bSuccess = true;
                            Message = FString::Printf(TEXT("Time of day set to %.2f"), TimeOfDay);
                            break;
                        }
                    }
                }
            }
        }
        if (!bSuccess)
        {
            bSuccess = false;
            Message = TEXT("Sky sphere not found or time function not available");
            ErrorCode = TEXT("SET_TIME_FAILED");
        }
    }
    // -------------------------------------------------------------------------
    // create_fog_volume: Create exponential height fog
    // -------------------------------------------------------------------------
    else if (LowerSub == TEXT("create_fog_volume"))
    {
        // Initialize to false - only set true on successful creation
        bSuccess = false;

        FVector Location(0, 0, 0);
        // Support both top-level x/y/z and location object
        const TSharedPtr<FJsonObject> *LocObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj)
        {
            (*LocObj)->TryGetNumberField(TEXT("x"), Location.X);
            (*LocObj)->TryGetNumberField(TEXT("y"), Location.Y);
            (*LocObj)->TryGetNumberField(TEXT("z"), Location.Z);
        }
        else
        {
            Payload->TryGetNumberField(TEXT("x"), Location.X);
            Payload->TryGetNumberField(TEXT("y"), Location.Y);
            Payload->TryGetNumberField(TEXT("z"), Location.Z);
        }

        if (!GEditor)
        {
            Message = TEXT("Editor not available");
            ErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
        }
        else
        {
            UClass *FogClass = LoadClass<AActor>(nullptr, TEXT("/Script/Engine.ExponentialHeightFog"));
            if (!FogClass)
            {
                Message = TEXT("ExponentialHeightFog class not found");
                ErrorCode = TEXT("CLASS_NOT_FOUND");
            }
            else
            {
                AActor *FogVolume = SpawnActorInActiveWorld<AActor>(
                    FogClass, Location, FRotator::ZeroRotator, TEXT("FogVolume"));
                if (FogVolume)
                {
                    bSuccess = true;
                    Message = TEXT("Fog volume created");
                    Resp->SetStringField(TEXT("actorName"), FogVolume->GetActorLabel());
                }
                else
                {
                    Message = TEXT("Failed to spawn fog volume actor");
                    ErrorCode = TEXT("SPAWN_FAILED");
                }
            }
        }
    }
    else if (LowerSub == TEXT("configure_sky_atmosphere"))
    {
        bSuccess = McpConfigureActorAndComponent(Payload, TEXT("/Script/Engine.SkyAtmosphere"),
                                                 TEXT("SkyAtmosphere"), TEXT("/Script/Engine.SkyAtmosphereComponent"),
                                                 Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("configure_sky_light"))
    {
        bSuccess = McpConfigureActorAndComponent(Payload, TEXT("/Script/Engine.SkyLight"),
                                                 TEXT("SkyLight"), TEXT("/Script/Engine.SkyLightComponent"),
                                                 Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("configure_directional_light_atmosphere"))
    {
        bSuccess = McpConfigureActorAndComponent(Payload, TEXT("/Script/Engine.DirectionalLight"),
                                                 TEXT("DirectionalLight"), TEXT("/Script/Engine.DirectionalLightComponent"),
                                                 Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("configure_exponential_height_fog"))
    {
        bSuccess = McpConfigureActorAndComponent(Payload, TEXT("/Script/Engine.ExponentialHeightFog"),
                                                 TEXT("ExponentialHeightFog"), TEXT("/Script/Engine.ExponentialHeightFogComponent"),
                                                 Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("configure_volumetric_cloud"))
    {
        bSuccess = McpConfigureActorAndComponent(Payload, TEXT("/Script/Engine.VolumetricCloud"),
                                                 TEXT("VolumetricCloud"), TEXT("/Script/Engine.VolumetricCloudComponent"),
                                                 Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("create_weather_system"))
    {
        bSuccess = McpConfigureParticleEmitter(Payload, TEXT("WeatherSystem"), Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("configure_rain_particles"))
    {
        bSuccess = McpConfigureParticleEmitter(Payload, TEXT("RainParticles"), Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("configure_snow_particles"))
    {
        bSuccess = McpConfigureParticleEmitter(Payload, TEXT("SnowParticles"), Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("configure_wind"))
    {
        bSuccess = McpConfigureActorAndComponent(Payload, TEXT("/Script/Engine.WindDirectionalSource"),
                                                 TEXT("WindDirectionalSource"), TEXT("/Script/Engine.WindDirectionalSourceComponent"),
                                                 Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("configure_lightning"))
    {
        bSuccess = McpConfigureParticleEmitter(Payload, TEXT("LightningSystem"), Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("create_time_of_day_system"))
    {
        bSuccess = McpCreateTimeOfDaySystem(Payload, Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("configure_sun_position"))
    {
        bSuccess = McpConfigureSunPosition(Payload, Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("configure_light_color_curve"))
    {
        bSuccess = McpCreateLinearColorCurve(Payload, TEXT("LightColorCurve"), Resp, Message, ErrorCode);
    }
    else if (LowerSub == TEXT("configure_sky_color_curve"))
    {
        bSuccess = McpCreateLinearColorCurve(Payload, TEXT("SkyColorCurve"), Resp, Message, ErrorCode);
    }
    else
    {
        return false;
    }

    return true;
}

} // namespace McpEnvironmentHandlers
#endif
