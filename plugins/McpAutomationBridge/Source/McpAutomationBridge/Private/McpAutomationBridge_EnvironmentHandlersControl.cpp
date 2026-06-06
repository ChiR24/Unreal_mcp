#include "McpAutomationBridge_EnvironmentHandlersShared.h"

using namespace McpEnvironmentHandlers;

bool UMcpAutomationBridgeSubsystem::HandleControlEnvironmentAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("control_environment"), ESearchCase::IgnoreCase) &&
        !Lower.StartsWith(TEXT("control_environment")))
    {
        return false;
    }

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("control_environment payload missing."),
                            TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction;
    Payload->TryGetStringField(TEXT("action"), SubAction);
    const FString LowerSub = SubAction.ToLower();

#if WITH_EDITOR
    // -------------------------------------------------------------------------
    // Helper lambda for sending results
    // -------------------------------------------------------------------------
    auto SendResult = [&](bool bSuccess, const TCHAR *Message,
                          const FString &ErrorCode,
                          const TSharedPtr<FJsonObject> &Result)
    {
        if (bSuccess)
        {
            SendAutomationResponse(RequestingSocket, RequestId, true,
                                   Message ? Message : TEXT("Environment control succeeded."),
                                   Result, FString());
        }
        else
        {
            SendAutomationResponse(RequestingSocket, RequestId, false,
                                   Message ? Message : TEXT("Environment control failed."),
                                   Result, ErrorCode);
        }
    };

    // Get editor world
    UWorld *World = nullptr;
    if (GEditor)
    {
        World = GEditor->GetEditorWorldContext().World();
    }

    if (!World)
    {
        SendResult(false, TEXT("Editor world is unavailable"),
                   TEXT("WORLD_NOT_AVAILABLE"), nullptr);
        return true;
    }

    // -------------------------------------------------------------------------
    // Helper lambdas for finding lights
    // -------------------------------------------------------------------------
    auto FindFirstDirectionalLight = [&]() -> ADirectionalLight *
    {
        for (TActorIterator<ADirectionalLight> It(World); It; ++It)
        {
            if (ADirectionalLight *Light = *It)
            {
                if (IsValid(Light))
                {
                    return Light;
                }
            }
        }
        return nullptr;
    };

    auto FindFirstSkyLight = [&]() -> ASkyLight *
    {
        for (TActorIterator<ASkyLight> It(World); It; ++It)
        {
            if (ASkyLight *Sky = *It)
            {
                if (IsValid(Sky))
                {
                    return Sky;
                }
            }
        }
        return nullptr;
    };

    // -------------------------------------------------------------------------
    // set_time_of_day: Adjust sun rotation based on hour
    // -------------------------------------------------------------------------
    if (LowerSub == TEXT("set_time_of_day"))
    {
        double Hour = 0.0;
        const bool bHasHour = Payload->TryGetNumberField(TEXT("hour"), Hour);
        if (!bHasHour)
        {
            SendResult(false, TEXT("Missing hour parameter"),
                       TEXT("INVALID_ARGUMENT"), nullptr);
            return true;
        }

        ADirectionalLight *SunLight = FindFirstDirectionalLight();
        if (!SunLight)
        {
            SendResult(false, TEXT("No directional light found"),
                       TEXT("SUN_NOT_FOUND"), nullptr);
            return true;
        }

        const float ClampedHour = FMath::Clamp(static_cast<float>(Hour), 0.0f, 24.0f);
        const float SolarPitch = (ClampedHour / 24.0f) * 360.0f - 90.0f;

        SunLight->Modify();
        FRotator NewRotation = SunLight->GetActorRotation();
        NewRotation.Pitch = SolarPitch;
        SunLight->SetActorRotation(NewRotation);

        if (UDirectionalLightComponent *LightComp =
                Cast<UDirectionalLightComponent>(SunLight->GetLightComponent()))
        {
            LightComp->MarkRenderStateDirty();
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetNumberField(TEXT("hour"), ClampedHour);
        Result->SetNumberField(TEXT("pitch"), SolarPitch);
        Result->SetStringField(TEXT("actor"), SunLight->GetPathName());

        // Add verification data
        McpHandlerUtils::AddVerification(Result, SunLight);

        SendResult(true, TEXT("Time of day updated"), FString(), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // set_sun_intensity: Set directional light intensity
    // -------------------------------------------------------------------------
    if (LowerSub == TEXT("set_sun_intensity"))
    {
        double Intensity = 0.0;
        if (!Payload->TryGetNumberField(TEXT("intensity"), Intensity))
        {
            SendResult(false, TEXT("Missing intensity parameter"),
                       TEXT("INVALID_ARGUMENT"), nullptr);
            return true;
        }

        ADirectionalLight *SunLight = FindFirstDirectionalLight();
        if (!SunLight)
        {
            SendResult(false, TEXT("No directional light found"),
                       TEXT("SUN_NOT_FOUND"), nullptr);
            return true;
        }

        if (UDirectionalLightComponent *LightComp =
                Cast<UDirectionalLightComponent>(SunLight->GetLightComponent()))
        {
            LightComp->SetIntensity(static_cast<float>(Intensity));
            LightComp->MarkRenderStateDirty();
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetNumberField(TEXT("intensity"), Intensity);
        Result->SetStringField(TEXT("actor"), SunLight->GetPathName());
        SendResult(true, TEXT("Sun intensity updated"), FString(), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // set_skylight_intensity: Set sky light intensity
    // -------------------------------------------------------------------------
    if (LowerSub == TEXT("set_skylight_intensity"))
    {
        double Intensity = 0.0;
        if (!Payload->TryGetNumberField(TEXT("intensity"), Intensity))
        {
            SendResult(false, TEXT("Missing intensity parameter"),
                       TEXT("INVALID_ARGUMENT"), nullptr);
            return true;
        }

        ASkyLight *SkyActor = FindFirstSkyLight();
        if (!SkyActor)
        {
            SendResult(false, TEXT("No skylight found"), TEXT("SKYLIGHT_NOT_FOUND"),
                       nullptr);
            return true;
        }

        if (USkyLightComponent *SkyComp = SkyActor->GetLightComponent())
        {
            SkyComp->SetIntensity(static_cast<float>(Intensity));
            SkyComp->MarkRenderStateDirty();
            SkyActor->MarkComponentsRenderStateDirty();
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetNumberField(TEXT("intensity"), Intensity);
        Result->SetStringField(TEXT("actor"), SkyActor->GetPathName());
        SendResult(true, TEXT("Skylight intensity updated"), FString(), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // Unknown action
    // -------------------------------------------------------------------------
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("action"), LowerSub);
    SendResult(false, TEXT("Unsupported environment control action"),
               TEXT("UNSUPPORTED_ACTION"), Result);
    return true;

#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Environment control requires editor build"),
                           nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
