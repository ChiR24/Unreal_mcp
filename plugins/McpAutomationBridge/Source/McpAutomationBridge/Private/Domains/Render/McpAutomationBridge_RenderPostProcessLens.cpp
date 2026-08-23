#include "Domains/Render/McpAutomationBridge_RenderHandlersPrivate.h"
#include "Domains/Render/McpAutomationBridge_RenderSupport.h"
#include "Domains/Render/McpAutomationBridge_RenderSupportEnums.h"
#include "Domains/Render/McpAutomationBridge_RenderSupportSettings.h"
#include "Foundation/Render/McpPostProcessVolumeResolution.h"

#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "Engine/PostProcessVolume.h"
#include "Engine/Scene.h"

namespace McpRenderHandlers
{
namespace
{
bool ApplyLensPostSettings(
    APostProcessVolume* Volume,
    const TSharedPtr<FJsonObject>& Settings,
    TArray<FString>& Applied,
    TArray<FString>& Unsupported,
    FString& Error)
{
    return ApplyJsonSettings(
        &Volume->Settings,
        FPostProcessSettings::StaticStruct(),
        Settings,
        true,
        Applied,
        Unsupported,
        Error);
}

bool ApplyLensPostNumber(
    APostProcessVolume* Volume,
    const FString& Field,
    double Value,
    TArray<FString>& Applied,
    TArray<FString>& Unsupported,
    FString& Error)
{
    TSharedPtr<FJsonObject> Settings = MakeShared<FJsonObject>();
    Settings->SetNumberField(Field, Value);
    return ApplyLensPostSettings(Volume, Settings, Applied, Unsupported, Error);
}

bool ApplyLensPostEnum(
    APostProcessVolume* Volume,
    const FString& Field,
    const FString& Value,
    TArray<FString>& Applied,
    TArray<FString>& Unsupported,
    FString& Error)
{
    TSharedPtr<FJsonObject> Settings = MakeShared<FJsonObject>();
    Settings->SetStringField(Field, Value);
    return ApplyLensPostSettings(Volume, Settings, Applied, Unsupported, Error);
}
}

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
        !ApplyLensPostSettings(Volume, Settings, Applied, Unsupported, Error))
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_SETTING"));
        return true;
    }

    if (SubAction == TEXT("configure_lens_flare") &&
        Payload->HasTypedField<EJson::Boolean>(TEXT("enabled")) &&
        !GetJsonBoolField(Payload, TEXT("enabled"), true))
    {
        ApplyLensPostNumber(Volume, TEXT("LensFlareIntensity"), 0.0, Applied, Unsupported, Error);
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
        ApplyLensPostEnum(Volume, TEXT("DepthOfFieldMethod"), Value, Applied, Unsupported, Error);
    }
    else if (SubAction == TEXT("set_focal_distance"))
    {
        ApplyLensPostNumber(Volume, TEXT("DepthOfFieldFocalDistance"),
            GetJsonNumberField(Settings, TEXT("DepthOfFieldFocalDistance"), GetJsonNumberField(Payload, TEXT("distance"), 0.0)),
            Applied, Unsupported, Error);
    }
    else if (SubAction == TEXT("set_aperture"))
    {
        ApplyLensPostNumber(Volume, TEXT("DepthOfFieldFstop"),
            GetJsonNumberField(Settings, TEXT("DepthOfFieldFstop"), GetJsonNumberField(Payload, TEXT("aperture"), 4.0)),
            Applied, Unsupported, Error);
    }
    else if (SubAction == TEXT("set_motion_blur_amount"))
    {
        ApplyLensPostNumber(Volume, TEXT("MotionBlurAmount"),
            GetJsonNumberField(Settings, TEXT("MotionBlurAmount"), GetJsonNumberField(Payload, TEXT("amount"), 0.0)),
            Applied, Unsupported, Error);
    }
    else if (SubAction == TEXT("set_motion_blur_max"))
    {
        ApplyLensPostNumber(Volume, TEXT("MotionBlurMax"),
            GetJsonNumberField(Settings, TEXT("MotionBlurMax"), GetJsonNumberField(Payload, TEXT("amount"), GetJsonNumberField(Payload, TEXT("max"), 0.0))),
            Applied, Unsupported, Error);
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
        ApplyLensPostEnum(Volume, TEXT("AutoExposureMethod"), Value, Applied, Unsupported, Error);
    }
    else if (SubAction == TEXT("set_exposure_compensation"))
    {
        ApplyLensPostNumber(Volume, TEXT("AutoExposureBias"),
            GetJsonNumberField(Payload, TEXT("compensationValue"), 0.0),
            Applied, Unsupported, Error);
    }
    else if (SubAction == TEXT("set_exposure_min_max"))
    {
        ApplyLensPostNumber(Volume, TEXT("AutoExposureMinBrightness"),
            GetJsonNumberField(Payload, TEXT("minBrightness"), 1.0), Applied, Unsupported, Error);
        ApplyLensPostNumber(Volume, TEXT("AutoExposureMaxBrightness"),
            GetJsonNumberField(Payload, TEXT("maxBrightness"), 1.0), Applied, Unsupported, Error);
    }
    else if (SubAction == TEXT("configure_ssao"))
    {
        ApplyLensPostSettings(Volume, Settings, Applied, Unsupported, Error);
        ApplyLensPostNumber(Volume, TEXT("AmbientOcclusionIntensity"),
            GetJsonNumberField(Payload, TEXT("intensity"), 0.5), Applied, Unsupported, Error);
    }
    else if (SubAction == TEXT("configure_vignette"))
    {
        ApplyLensPostNumber(Volume, TEXT("VignetteIntensity"),
            GetJsonNumberField(Payload, TEXT("intensity"), 0.4), Applied, Unsupported, Error);
    }
    else if (SubAction == TEXT("configure_chromatic_aberration"))
    {
        ApplyLensPostSettings(Volume, Settings, Applied, Unsupported, Error);
        ApplyLensPostNumber(Volume, TEXT("SceneFringeIntensity"),
            GetJsonNumberField(Payload, TEXT("intensity"), 0.0), Applied, Unsupported, Error);
    }
    else if (SubAction == TEXT("configure_grain"))
    {
        ApplyLensPostSettings(Volume, Settings, Applied, Unsupported, Error);
        ApplyLensPostNumber(Volume, TEXT("FilmGrainIntensity"),
            GetJsonNumberField(Payload, TEXT("intensity"), 0.0), Applied, Unsupported, Error);
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
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Post-process lens settings applied."), Result);
    return true;
}
}
#endif
