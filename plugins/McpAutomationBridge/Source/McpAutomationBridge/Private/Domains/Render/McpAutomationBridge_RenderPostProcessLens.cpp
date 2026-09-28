#include "Domains/Render/McpAutomationBridge_RenderHandlersPrivate.h"
#include "Domains/Render/McpAutomationBridge_RenderSupport.h"
#include "Domains/Render/McpAutomationBridge_RenderSupportEnums.h"
#include "Domains/Render/McpAutomationBridge_RenderSupportSettings.h"
#include "Foundation/Render/McpPostProcessVolumeResolution.h"

#include "McpAutomationBridgeSubsystem.h"

#include "Engine/PostProcessVolume.h"
#include "Engine/Scene.h"

namespace McpRenderHandlers
{
// The vignette/grain/chromatic/SSAO variants read their declared `amount`.
bool HandleRenderPostProcessLensAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (SubAction == TEXT("configure_screen_percentage"))
    {
        double ScreenPercentage = 100.0;
        FString Error;
        if (!ReadBoundedNumberField(Payload, TEXT("screenPercentage"), 100.0, 1.0, 200.0, ScreenPercentage, Error))
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
            return true;
        }
        TArray<FString> Applied;
        TArray<FString> Unsupported;
        SetConsoleVariable(
            TEXT("r.ScreenPercentage"),
            FString::SanitizeFloat(ScreenPercentage),
            Applied,
            Unsupported);
        TSharedPtr<FJsonObject> Result = MakeRenderResult(SubAction);
        Result->SetNumberField(TEXT("screenPercentage"), ScreenPercentage);
        AddStringArray(Result, TEXT("appliedCVars"), Applied);
        AddStringArray(Result, TEXT("unsupportedCVars"), Unsupported);
        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Screen percentage configured."), Result);
        return true;
    }

    static const TSet<FString> Actions = {
        TEXT("configure_lens_flare"), TEXT("configure_dof"), TEXT("set_dof_method"),
        TEXT("set_focal_distance"), TEXT("set_aperture"), TEXT("configure_bokeh"),
        TEXT("configure_motion_blur"), TEXT("set_motion_blur_amount"),
        TEXT("set_motion_blur_max"), TEXT("configure_exposure"),
        TEXT("set_exposure_method"), TEXT("set_exposure_compensation"),
        TEXT("set_exposure_min_max"), TEXT("configure_ssao"), TEXT("configure_gtao"),
        TEXT("configure_vignette"), TEXT("configure_chromatic_aberration"),
        TEXT("configure_grain")
    };
    if (!Actions.Contains(SubAction))
    {
        return false;
    }

    FString ResolveError;
    FString ResolveErrorCode;
    APostProcessVolume* Volume = McpResolvePostProcessVolume(
        GetRenderWorld(), Payload, false, ResolveError, ResolveErrorCode);
    if (!Volume)
    {
        if (!ResolveError.IsEmpty())
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, ResolveError, ResolveErrorCode);
        }
        return true;
    }
    Volume->Modify();
    TArray<FString> Applied;
    TArray<FString> Unsupported;
    FString Error;
    const TSharedPtr<FJsonObject> Settings = GetSettingsObject(Payload);

    if ((SubAction == TEXT("configure_lens_flare") ||
         SubAction == TEXT("configure_dof") ||
         SubAction == TEXT("configure_bokeh") ||
         SubAction == TEXT("configure_motion_blur") ||
         SubAction == TEXT("configure_exposure") ||
         SubAction == TEXT("configure_gtao")) &&
        !ApplyPostProcessSettings(Volume, Settings, Applied, Unsupported, Error))
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_SETTING"));
        return true;
    }

    ApplyVolumeBlendFields(Volume, Payload, Applied);
    if (SubAction == TEXT("configure_exposure") &&
        !ApplyDeclaredExposureFields(Volume, Payload, Applied, Unsupported, Error))
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_SETTING"));
        return true;
    }

    // Each single-value variant writes its value only when the caller passed it. Defaulting a
    // missing value reset the other exposure bound to 1.0, and SSAO, grain and chromatic
    // aberration overwrote what `settings` had just set with their default.
    const auto ApplyGiven = [&](const TCHAR* Key, const TCHAR* Field)
    {
        if (Payload->HasTypedField<EJson::Number>(Key))
        {
            ApplyPostProcessField(Volume, Field, MakeShared<FJsonValueNumber>(Payload->GetNumberField(Key)), Applied, Unsupported, Error);
        }
    };
    if (SubAction == TEXT("configure_lens_flare") &&
        Payload->HasTypedField<EJson::Boolean>(TEXT("enabled")) &&
        !GetJsonBoolField(Payload, TEXT("enabled"), true))
    {
        ApplyPostProcessField(Volume, TEXT("LensFlareIntensity"), MakeShared<FJsonValueNumber>(0.0), Applied, Unsupported, Error);
    }
    else if (SubAction == TEXT("set_dof_method"))
    {
        const FString Method = GetJsonStringField(Payload, TEXT("method"));
        FString Value;
        if (!ResolveEnumAlias(DepthOfFieldMethodMap(), Method, Value, Error))
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_SETTING"));
            return true;
        }
        ApplyPostProcessField(Volume, TEXT("DepthOfFieldMethod"), MakeShared<FJsonValueString>(Value), Applied, Unsupported, Error);
    }
    else if (SubAction == TEXT("set_focal_distance"))
    {
        ApplyGiven(TEXT("distance"), TEXT("DepthOfFieldFocalDistance"));
    }
    else if (SubAction == TEXT("set_aperture"))
    {
        ApplyGiven(TEXT("aperture"), TEXT("DepthOfFieldFstop"));
    }
    else if (SubAction == TEXT("set_motion_blur_amount"))
    {
        ApplyGiven(TEXT("amount"), TEXT("MotionBlurAmount"));
    }
    else if (SubAction == TEXT("set_motion_blur_max"))
    {
        ApplyGiven(TEXT("amount"), TEXT("MotionBlurMax"));
    }
    else if (SubAction == TEXT("set_exposure_method"))
    {
        const FString Method = GetJsonStringField(Payload, TEXT("method"));
        FString Value;
        if (!ResolveEnumAlias(AutoExposureMethodMap(), Method, Value, Error))
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_SETTING"));
            return true;
        }
        ApplyPostProcessField(Volume, TEXT("AutoExposureMethod"), MakeShared<FJsonValueString>(Value), Applied, Unsupported, Error);
    }
    else if (SubAction == TEXT("set_exposure_compensation"))
    {
        ApplyGiven(TEXT("compensationValue"), TEXT("AutoExposureBias"));
    }
    else if (SubAction == TEXT("set_exposure_min_max"))
    {
        ApplyGiven(TEXT("minBrightness"), TEXT("AutoExposureMinBrightness"));
        ApplyGiven(TEXT("maxBrightness"), TEXT("AutoExposureMaxBrightness"));
    }
    else if (SubAction == TEXT("configure_ssao") || SubAction == TEXT("configure_chromatic_aberration") ||
             SubAction == TEXT("configure_grain"))
    {
        ApplyPostProcessSettings(Volume, Settings, Applied, Unsupported, Error);
        ApplyGiven(TEXT("amount"), SubAction == TEXT("configure_ssao") ? TEXT("AmbientOcclusionIntensity")
                                   : SubAction == TEXT("configure_grain") ? TEXT("FilmGrainIntensity")
                                                                           : TEXT("SceneFringeIntensity"));
    }
    else if (SubAction == TEXT("configure_vignette"))
    {
        ApplyGiven(TEXT("amount"), TEXT("VignetteIntensity"));
    }

    if (!Error.IsEmpty())
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_SETTING"));
        return true;
    }
    // Answering success over an untouched volume hid a call that changed nothing.
    if (Applied.Num() == 0 && Unsupported.Num() == 0)
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("%s changed nothing: pass this variant's own parameter, or `settings` naming "
                                 "FPostProcessSettings fields."), *SubAction),
            TEXT("NO_SETTING_SUPPLIED"));
        return true;
    }
    Volume->MarkComponentsRenderStateDirty();
    TSharedPtr<FJsonObject> Result = MakeRenderResult(SubAction);
    AddStringArray(Result, TEXT("appliedSettings"), Applied);
    AddStringArray(Result, TEXT("unsupportedSettings"), Unsupported);
    McpHandlerUtils::AddVerification(Result, Volume);
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Post-process lens settings applied."), Result);
    return true;
}
}
