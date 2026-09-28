#include "Domains/Render/McpAutomationBridge_RenderHandlersPrivate.h"
#include "Domains/Render/McpAutomationBridge_RenderSupport.h"
#include "Domains/Render/McpAutomationBridge_RenderSupportSettings.h"

#include "McpAutomationBridgeSubsystem.h"

#include "Engine/PostProcessVolume.h"
#include "Engine/Scene.h"
#include "Engine/Texture.h"

namespace McpRenderHandlers
{
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
    // Every color variant takes the volume's infiniteUnbound and blendWeight.
    ApplyVolumeBlendFields(Volume, Payload, Applied);

    if (SubAction == TEXT("configure_pp_blend"))
    {
        // Only what was passed is written and reported: listing all three as applied
        // on an empty call slipped past the NO_SETTING_SUPPLIED guard below.
        if (Payload->HasTypedField<EJson::Boolean>(TEXT("enabled")))
        {
            Volume->bEnabled = Payload->GetBoolField(TEXT("enabled"));
            Applied.Add(TEXT("bEnabled"));
        }
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
        // amount is the declared name; this read the undeclared intensity, so bloom stayed at 1.0.
        if (Payload->HasTypedField<EJson::Number>(TEXT("amount")) &&
            !ApplyPostProcessField(
                Volume, TEXT("BloomIntensity"),
                MakeShared<FJsonValueNumber>(Payload->GetNumberField(TEXT("amount"))),
                Applied, Unsupported, Error))
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_SETTING"));
            return true;
        }
    }
    else if (SubAction == TEXT("configure_bloom"))
    {
        // configure_bloom used to have no branch at all, so it fell through to
        // the generic settings path and ignored `amount` and `threshold`. A
        // caller who passed them got "Post-process color settings applied." and
        // an unchanged volume. Read them as the lens variants read their number,
        // and still fold in `settings` for anything else on the struct.
        ApplyPostProcessSettings(Volume, GetSettingsObject(Payload), Applied, Unsupported, Error);
        if (Payload->HasField(TEXT("amount")))
        {
            ApplyPostProcessField(
                Volume, TEXT("BloomIntensity"),
                MakeShared<FJsonValueNumber>(GetJsonNumberField(Payload, TEXT("amount"), 1.0)),
                Applied, Unsupported, Error);
        }
        if (Payload->HasField(TEXT("threshold")))
        {
            ApplyPostProcessField(
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
        // The declared threshold alone: a settings.BloomThreshold used to win over it.
        if (Payload->HasTypedField<EJson::Number>(TEXT("threshold")) &&
            !ApplyPostProcessField(
                Volume, TEXT("BloomThreshold"), MakeShared<FJsonValueNumber>(Payload->GetNumberField(TEXT("threshold"))),
                Applied, Unsupported, Error))
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_SETTING"));
            return true;
        }
    }
    else if (!ApplyPostProcessSettings(
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
