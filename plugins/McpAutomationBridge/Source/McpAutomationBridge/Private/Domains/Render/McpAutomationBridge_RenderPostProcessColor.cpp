#include "Domains/Render/McpAutomationBridge_RenderHandlersPrivate.h"
#include "Domains/Render/McpAutomationBridge_RenderSupport.h"
#include "Domains/Render/McpAutomationBridge_RenderSupportSettings.h"

#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "Engine/PostProcessVolume.h"
#include "Engine/Scene.h"
#include "Engine/Texture.h"

namespace McpRenderHandlers
{
namespace
{
bool ApplyColorPostSettings(
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

bool ApplyColorPostValue(
    APostProcessVolume* Volume,
    const FString& Field,
    const TSharedPtr<FJsonValue>& Value,
    TArray<FString>& Applied,
    TArray<FString>& Unsupported,
    FString& Error)
{
    TSharedPtr<FJsonObject> Settings = MakeShared<FJsonObject>();
    Settings->SetField(Field, Value);
    return ApplyColorPostSettings(Volume, Settings, Applied, Unsupported, Error);
}
}

bool HandleRenderPostProcessColorAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    static const TSet<FString> Actions = {
        TEXT("configure_pp_blend"), TEXT("set_pp_white_balance"),
        TEXT("set_pp_color_grading"), TEXT("set_pp_lut"),
        TEXT("configure_tonemapper"), TEXT("set_tonemapper_type"),
        TEXT("configure_bloom"), TEXT("set_bloom_intensity"),
        TEXT("set_bloom_threshold")
    };
    if (!Actions.Contains(SubAction))
    {
        return false;
    }

    APostProcessVolume* Volume =
        RequirePostProcessVolume(Subsystem, RequestId, Payload, RequestingSocket);
    if (!Volume)
    {
        return true;
    }
    Volume->Modify();
    TArray<FString> Applied;
    TArray<FString> Unsupported;
    FString Error;

    if (SubAction == TEXT("configure_pp_blend"))
    {
        Volume->bUnbound = GetJsonBoolField(
            Payload, TEXT("infiniteUnbound"), GetJsonBoolField(Payload, TEXT("bUnbound"), Volume->bUnbound));
        Volume->BlendWeight = GetJsonNumberField(Payload, TEXT("blendWeight"), Volume->BlendWeight);
        Volume->bEnabled = GetJsonBoolField(Payload, TEXT("enabled"), Volume->bEnabled);
        Applied = {TEXT("bUnbound"), TEXT("BlendWeight"), TEXT("bEnabled")};
    }
    else if (SubAction == TEXT("set_pp_lut"))
    {
        const FString LutPath = GetJsonStringField(Payload, TEXT("lutPath"));
        UTexture* Texture = LoadObject<UTexture>(nullptr, *LutPath);
        if (!Texture)
        {
            Subsystem->SendAutomationError(
                RequestingSocket, RequestId, TEXT("Color grading LUT not found."), TEXT("ASSET_NOT_FOUND"));
            return true;
        }
        Volume->Settings.bOverride_ColorGradingLUT = true;
        Volume->Settings.ColorGradingLUT = Texture;
        Applied.Add(TEXT("ColorGradingLUT"));
    }
    else if (SubAction == TEXT("set_tonemapper_type"))
    {
        const FString Method = GetJsonStringField(Payload, TEXT("method"), TEXT("Filmic"));
        SetConsoleVariable(
            TEXT("r.TonemapperFilm"),
            Method.Equals(TEXT("Filmic"), ESearchCase::IgnoreCase) ? TEXT("1") : TEXT("0"),
            Applied,
            Unsupported);
    }
    else if (SubAction == TEXT("set_bloom_intensity"))
    {
        if (!ApplyColorPostValue(
                Volume, TEXT("BloomIntensity"),
                MakeShared<FJsonValueNumber>(GetJsonNumberField(Payload, TEXT("intensity"), 1.0)),
                Applied, Unsupported, Error))
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_SETTING"));
            return true;
        }
    }
    else if (SubAction == TEXT("configure_bloom"))
    {
        // configure_bloom used to have no branch at all, so it fell through to
        // the generic settings path and ignored `amount` and `threshold` -- the
        // two parameters its own contract declares. A caller who passed them got
        // "Post-process color settings applied." and an unchanged volume. The
        // sibling lens variants (vignette, grain, chromatic aberration) already
        // read their top-level number, so read ours the same way and still fold
        // in an explicit `settings` object for anything else on the struct.
        ApplyColorPostSettings(Volume, GetSettingsObject(Payload), Applied, Unsupported, Error);
        if (Payload->HasField(TEXT("amount")))
        {
            ApplyColorPostValue(
                Volume, TEXT("BloomIntensity"),
                MakeShared<FJsonValueNumber>(GetJsonNumberField(Payload, TEXT("amount"), 1.0)),
                Applied, Unsupported, Error);
        }
        if (Payload->HasField(TEXT("threshold")))
        {
            ApplyColorPostValue(
                Volume, TEXT("BloomThreshold"),
                MakeShared<FJsonValueNumber>(GetJsonNumberField(Payload, TEXT("threshold"), -1.0)),
                Applied, Unsupported, Error);
        }
        if (!Error.IsEmpty())
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_SETTING"));
            return true;
        }
    }
    else if (SubAction == TEXT("set_bloom_threshold"))
    {
        const TSharedPtr<FJsonObject> Settings = GetSettingsObject(Payload);
        const double Threshold = Settings.IsValid()
            ? GetJsonNumberField(Settings, TEXT("BloomThreshold"), GetJsonNumberField(Payload, TEXT("threshold"), -1.0))
            : GetJsonNumberField(Payload, TEXT("threshold"), -1.0);
        if (!ApplyColorPostValue(
                Volume, TEXT("BloomThreshold"), MakeShared<FJsonValueNumber>(Threshold),
                Applied, Unsupported, Error))
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_SETTING"));
            return true;
        }
    }
    else if (!ApplyColorPostSettings(
                 Volume, GetSettingsObject(Payload), Applied, Unsupported, Error))
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_SETTING"));
        return true;
    }

    // "Post-process color settings applied." over an empty Applied list is a
    // lie the caller cannot see past: the volume is untouched and the receipt
    // reads like a success. Refuse instead, and name the thing that was missing.
    if (Applied.Num() == 0 && Unsupported.Num() == 0)
    {
        Subsystem->SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(
                TEXT("%s changed nothing on '%s': no post-process value was supplied. Pass this "
                     "variant's own numeric parameter, or a `settings` object naming "
                     "FPostProcessSettings fields (e.g. {\"BloomIntensity\": 1.4})."),
                *SubAction, *Volume->GetActorNameOrLabel()),
            TEXT("NO_SETTING_SUPPLIED"));
        return true;
    }

    Volume->MarkComponentsRenderStateDirty();
    TSharedPtr<FJsonObject> Result = MakeRenderResult(SubAction);
    AddStringArray(Result, TEXT("appliedSettings"), Applied);
    AddStringArray(Result, TEXT("unsupportedSettings"), Unsupported);
    Result->SetNumberField(TEXT("blendWeight"), Volume->BlendWeight);
    Result->SetBoolField(TEXT("infiniteUnbound"), Volume->bUnbound);
    McpHandlerUtils::AddVerification(Result, Volume);
    Subsystem->SendAutomationResponse(
        RequestingSocket, RequestId, true, TEXT("Post-process color settings applied."), Result);
    return true;
}
}
#endif
