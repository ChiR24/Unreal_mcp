#include "Domains/Render/McpRenderPrivate.h"
#include "Domains/Render/McpRenderSupport.h"
#include "Domains/Render/McpRenderSupportSettings.h"

#include "EngineWireSubsystem.h"

#include "Engine/PostProcessVolume.h"
#include "Engine/Scene.h"
#include "Engine/Texture.h"

namespace McpRenderHandlers
{
bool HandleRenderPostProcessColorAction(
    UEngineWireSubsystem* Subsystem,
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
    // set_tonemapper_type set r.TonemapperFilm, gone from the engine before 5.0, and answered applied.
    const FString ToneMethod = GetJsonStringField(Payload, TEXT("method"), TEXT("Filmic"));
    const bool bFilmic = ToneMethod.Equals(TEXT("Filmic"), ESearchCase::IgnoreCase);
    if (SubAction == TEXT("set_tonemapper_type") && !bFilmic && !ToneMethod.Equals(TEXT("Linear"), ESearchCase::IgnoreCase))
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("method '%s' is not a tonemapper: use "
            "Filmic (the tone curve, the default) or Linear (no tone curve). Nothing was changed."), *ToneMethod), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    APostProcessVolume* Volume =
        RequirePostProcessVolume(Subsystem, RequestId, Payload, RequestingSocket);
    if (!Volume)
    {
        return true;
    }
    // Every value goes to a copy first: the volume changes, in one undo step, only once all of them are taken.
    FPostProcessSettings Staged = Volume->Settings;
    TArray<FString> Applied;
    TArray<FString> Unsupported;
    FString Error;
    const bool bBlend = SubAction == TEXT("configure_pp_blend");

    if (bBlend)
    {
        // Only the volume's own fields (enabled, blendWeight, infiniteUnbound), which the commit below writes.
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
        Staged.bOverride_ColorGradingLUT = true;
        Staged.ColorGradingLUT = Texture;
        Applied.Add(TEXT("ColorGradingLUT"));
    }
    else if (SubAction == TEXT("set_tonemapper_type"))
    {
        // The filmic tone curve is the engine's one tonemapper; Linear switches the curve off the way the engine
        // documents it, ToneCurveAmount and ExpandGamut at 0.
        for (const TCHAR* Field : {TEXT("ToneCurveAmount"), TEXT("ExpandGamut")})
        {
            if (!ApplyPostProcessField(Staged, Field, MakeShared<FJsonValueNumber>(bFilmic ? 1.0 : 0.0), Applied, Unsupported, Error))
            {
                Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_SETTING"));
                return true;
            }
        }
    }
    else if (SubAction == TEXT("set_bloom_intensity"))
    {
        // amount is the declared name; this read the undeclared intensity, so bloom stayed at 1.0.
        if (Payload->HasTypedField<EJson::Number>(TEXT("amount")) &&
            !ApplyPostProcessField(
                Staged, TEXT("BloomIntensity"),
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
        // amount and threshold go on as sent, so a value that is no number is refused: it was taken as 1 and -1.
        if (ApplyPostProcessSettings(Staged, GetSettingsObject(Payload), Applied, Unsupported, Error) &&
            Payload->HasField(TEXT("amount")))
        {
            ApplyPostProcessField(Staged, TEXT("BloomIntensity"), Payload->TryGetField(TEXT("amount")), Applied, Unsupported, Error);
        }
        if (Error.IsEmpty() && Payload->HasField(TEXT("threshold")))
        {
            ApplyPostProcessField(Staged, TEXT("BloomThreshold"), Payload->TryGetField(TEXT("threshold")), Applied, Unsupported, Error);
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
                Staged, TEXT("BloomThreshold"), MakeShared<FJsonValueNumber>(Payload->GetNumberField(TEXT("threshold"))),
                Applied, Unsupported, Error))
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_SETTING"));
            return true;
        }
    }
    else if (!ApplyPostProcessSettings(
                 Staged, GetSettingsObject(Payload), Applied, Unsupported, Error))
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_SETTING"));
        return true;
    }

    // "Post-process color settings applied." over an empty Applied list is a
    // lie the caller cannot see past: the volume is untouched and the receipt
    // reads like a success. Refuse instead, and name the thing that was missing.
    // blendWeight or infiniteUnbound alone answered success for any variant while its own value was never sent.
    const bool bVolumeFields = bBlend && (Payload->HasTypedField<EJson::Boolean>(TEXT("enabled")) ||
        Payload->HasTypedField<EJson::Number>(TEXT("blendWeight")) || Payload->HasTypedField<EJson::Boolean>(TEXT("infiniteUnbound")));
    if (Applied.Num() == 0 && Unsupported.Num() == 0 && !bVolumeFields)
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

    CommitPostProcessVolume(Volume, Staged, Payload, bBlend, Applied);
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
