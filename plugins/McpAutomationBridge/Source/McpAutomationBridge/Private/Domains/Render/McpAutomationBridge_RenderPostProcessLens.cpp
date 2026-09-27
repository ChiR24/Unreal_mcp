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
        ApplyPostProcessField(Volume, TEXT("DepthOfFieldFocalDistance"), MakeShared<FJsonValueNumber>(GetJsonNumberField(Settings, TEXT("DepthOfFieldFocalDistance"), GetJsonNumberField(Payload, TEXT("distance"), 0.0))), Applied, Unsupported, Error);
    }
    else if (SubAction == TEXT("set_aperture"))
    {
        ApplyPostProcessField(Volume, TEXT("DepthOfFieldFstop"), MakeShared<FJsonValueNumber>(GetJsonNumberField(Settings, TEXT("DepthOfFieldFstop"), GetJsonNumberField(Payload, TEXT("aperture"), 4.0))), Applied, Unsupported, Error);
    }
    else if (SubAction == TEXT("set_motion_blur_amount"))
    {
        ApplyPostProcessField(Volume, TEXT("MotionBlurAmount"), MakeShared<FJsonValueNumber>(GetJsonNumberField(Settings, TEXT("MotionBlurAmount"), GetJsonNumberField(Payload, TEXT("amount"), 0.0))), Applied, Unsupported, Error);
    }
    else if (SubAction == TEXT("set_motion_blur_max"))
    {
        ApplyPostProcessField(Volume, TEXT("MotionBlurMax"), MakeShared<FJsonValueNumber>(GetJsonNumberField(Settings, TEXT("MotionBlurMax"), GetJsonNumberField(Payload, TEXT("amount"), GetJsonNumberField(Payload, TEXT("max"), 0.0)))), Applied, Unsupported, Error);
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
        ApplyPostProcessField(Volume, TEXT("AutoExposureBias"), MakeShared<FJsonValueNumber>(GetJsonNumberField(Payload, TEXT("compensationValue"), 0.0)), Applied, Unsupported, Error);
    }
    else if (SubAction == TEXT("set_exposure_min_max"))
    {
        ApplyPostProcessField(Volume, TEXT("AutoExposureMinBrightness"), MakeShared<FJsonValueNumber>(GetJsonNumberField(Payload, TEXT("minBrightness"), 1.0)), Applied, Unsupported, Error);
        ApplyPostProcessField(Volume, TEXT("AutoExposureMaxBrightness"), MakeShared<FJsonValueNumber>(GetJsonNumberField(Payload, TEXT("maxBrightness"), 1.0)), Applied, Unsupported, Error);
    }
    else if (SubAction == TEXT("configure_ssao"))
    {
        ApplyPostProcessSettings(Volume, Settings, Applied, Unsupported, Error);
        ApplyPostProcessField(Volume, TEXT("AmbientOcclusionIntensity"), MakeShared<FJsonValueNumber>(GetJsonNumberField(Payload, TEXT("amount"), 0.5)), Applied, Unsupported, Error);
    }
    else if (SubAction == TEXT("configure_vignette"))
    {
        ApplyPostProcessField(Volume, TEXT("VignetteIntensity"), MakeShared<FJsonValueNumber>(GetJsonNumberField(Payload, TEXT("amount"), 0.4)), Applied, Unsupported, Error);
    }
    else if (SubAction == TEXT("configure_chromatic_aberration"))
    {
        ApplyPostProcessSettings(Volume, Settings, Applied, Unsupported, Error);
        ApplyPostProcessField(Volume, TEXT("SceneFringeIntensity"), MakeShared<FJsonValueNumber>(GetJsonNumberField(Payload, TEXT("amount"), 0.0)), Applied, Unsupported, Error);
    }
    else if (SubAction == TEXT("configure_grain"))
    {
        ApplyPostProcessSettings(Volume, Settings, Applied, Unsupported, Error);
        ApplyPostProcessField(Volume, TEXT("FilmGrainIntensity"), MakeShared<FJsonValueNumber>(GetJsonNumberField(Payload, TEXT("amount"), 0.0)), Applied, Unsupported, Error);
    }

    if (!Error.IsEmpty())
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_SETTING"));
        return true;
    }
    Volume->MarkComponentsRenderStateDirty();
    TSharedPtr<FJsonObject> Result = MakeRenderResult(SubAction);
    AddStringArray(Result, TEXT("appliedSettings"), Applied);
    AddStringArray(Result, TEXT("unsupportedSettings"), Unsupported);
    McpHandlerUtils::AddVerification(Result, Volume);
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
        Applied.Num() > 0
            ? FString(TEXT("Post-process lens settings applied."))
            : FString(TEXT("No post-process setting was applied: pass `settings` (FPostProcessSettings "
                           "field names) or this variant's own parameters.")),
        Result);
    return true;
}
}
