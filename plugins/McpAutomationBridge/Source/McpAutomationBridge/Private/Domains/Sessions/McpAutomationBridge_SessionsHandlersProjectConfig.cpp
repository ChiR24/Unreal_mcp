#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sessions/McpAutomationBridge_SessionsHandlersPrivate.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsProjectConfig.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"

#include "HAL/IConsoleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Sound/AudioSettings.h"

// Online subsystem choice and VoIP knobs are project config the engine reads at startup (and packages
// with the game). Every write goes through WriteProjectConfigValue, which reads the file back.

bool HandleConfigureSessionInterface(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    static const FString Prefix = TEXT("OnlineSubsystem");
    FString Type = GetJsonStringField(Payload, TEXT("interfaceType"), TEXT("")).TrimStartAndEnd();
    Type.RemoveFromStart(Prefix);
    if (Type.Equals(TEXT("LAN"), ESearchCase::IgnoreCase))
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("LAN is not an online subsystem: LAN sessions run on the Null subsystem. Use interfaceType Null."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // DefaultPlatformService names a plugin OnlineSubsystem<Type>; a disabled one makes the engine
    // fall back silently at startup, so it is refused here with the subsystems that would work.
    TSharedPtr<IPlugin> Plugin;
    if (!Type.IsEmpty())
    {
        Plugin = IPluginManager::Get().FindPlugin(Prefix + Type);
    }
    if (!Plugin.IsValid() || !Plugin->IsEnabled())
    {
        TArray<FString> Enabled;
        for (const TSharedRef<IPlugin>& Candidate : IPluginManager::Get().GetEnabledPlugins())
        {
            const FString& Name = Candidate->GetName();
            if (Name.StartsWith(Prefix) && Name.Len() > Prefix.Len() && Name != TEXT("OnlineSubsystemUtils"))
            {
                Enabled.Add(Name.Mid(Prefix.Len()));
            }
        }
        Subsystem->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("%s; nothing was changed. Enabled online subsystems: %s. Enable the plugin OnlineSubsystem<Name> and restart the editor to use another."),
                Type.IsEmpty() ? TEXT("interfaceType is required, e.g. Null, Steam or EOS")
                    : *FString::Printf(TEXT("OnlineSubsystem%s is %s"), *Type, Plugin.IsValid() ? TEXT("installed but not enabled") : TEXT("not an installed plugin")),
                Enabled.Num() > 0 ? *FString::Join(Enabled, TEXT(", ")) : TEXT("none")),
            Type.IsEmpty() ? TEXT("INVALID_ARGUMENT") : TEXT("PLUGIN_NOT_ENABLED"));
        return true;
    }
    Type = Plugin->GetName().Mid(Prefix.Len());

    FString Previous;
    GConfig->GetString(TEXT("OnlineSubsystem"), TEXT("DefaultPlatformService"), Previous, GEngineIni);
    FString ConfigFile;
    FString Error;
    if (!McpHandlerUtils::WriteProjectConfigValue(TEXT("OnlineSubsystem"), TEXT("DefaultPlatformService"), Type, TEXT("Engine"), ConfigFile, Error))
    {
        Subsystem->SendAutomationError(Socket, RequestId, Error, TEXT("PERSIST_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetBoolField(TEXT("success"), true);
    ResponseJson->SetStringField(TEXT("interfaceType"), Type);
    ResponseJson->SetStringField(TEXT("previous"), Previous);
    ResponseJson->SetStringField(TEXT("configFile"), ConfigFile);
    ResponseJson->SetBoolField(TEXT("persisted"), true);
    ResponseJson->SetBoolField(TEXT("requiresRestart"), true);
    const FString Message = FString::Printf(
        TEXT("[OnlineSubsystem] DefaultPlatformService=%s written to %s. The online subsystem is chosen when its module starts, so the editor and packaged games use %s from their next launch; this session keeps %s."),
        *Type, *ConfigFile, *Type, Previous.IsEmpty() ? TEXT("the engine default") : *Previous);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

bool HandleConfigureVoiceSettings(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    struct FVoiceWrite
    {
        FString Section;
        FString Key;
        FString Value;
        bool bConsoleVariable;
    };
    TArray<FVoiceWrite> Writes;
    bool bRestartOnly = false;

    // Every field is checked before anything is written, so a bad value changes nothing.
    static const struct { const TCHAR* Field; const TCHAR* ConsoleVariable; double Max; } Knobs[] = {
        {TEXT("micInputGain"), TEXT("voice.MicInputGain"), 10.0},
        {TEXT("noiseGateThreshold"), TEXT("voice.MicNoiseGateThreshold"), 1.0},
        {TEXT("silenceDetectionThreshold"), TEXT("voice.SilenceDetectionThreshold"), 1.0},
    };
    for (const auto& Knob : Knobs)
    {
        double Value = 0.0;
        if (!Payload.IsValid() || !Payload->HasField(Knob.Field))
        {
            continue;
        }
        if (!Payload->TryGetNumberField(Knob.Field, Value) || Value < 0.0 || Value > Knob.Max)
        {
            Subsystem->SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("%s must be a number from 0 to %.0f; nothing was changed."), Knob.Field, Knob.Max),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }
        Writes.Add({TEXT("SystemSettings"), Knob.ConsoleVariable, FString::SanitizeFloat(Value), true});
    }
    if (Payload.IsValid() && Payload->HasField(TEXT("sampleRate")))
    {
        double Rate = 0.0;
        Payload->TryGetNumberField(TEXT("sampleRate"), Rate);
        if (Rate != 16000.0 && Rate != 24000.0)
        {
            Subsystem->SendAutomationError(Socket, RequestId,
                TEXT("sampleRate must be 16000 or 24000 (the two VoIP rates the engine supports); nothing was changed."),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }
        Writes.Add({TEXT("/Script/Engine.AudioSettings"), TEXT("VoiPSampleRate"), Rate == 16000.0 ? TEXT("Low16000Hz") : TEXT("Normal24000Hz"), false});
        bRestartOnly = true;
    }
    if (Payload.IsValid() && Payload->HasField(TEXT("voiceEnabled")))
    {
        const FString Flag = GetJsonBoolField(Payload, TEXT("voiceEnabled"), true) ? TEXT("True") : TEXT("False");
        Writes.Add({TEXT("Voice"), TEXT("bEnabled"), Flag, false});
        Writes.Add({TEXT("OnlineSubsystem"), TEXT("bHasVoiceEnabled"), Flag, false});
        bRestartOnly = true;
    }
    if (Writes.Num() == 0)
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("Pass at least one of voiceEnabled, micInputGain, noiseGateThreshold, silenceDetectionThreshold or sampleRate."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> Written;
    TArray<TSharedPtr<FJsonValue>> LiveApplied;
    FString ConfigFile;
    for (const FVoiceWrite& Write : Writes)
    {
        FString Error;
        if (!McpHandlerUtils::WriteProjectConfigValue(Write.Section, Write.Key, Write.Value, TEXT("Engine"), ConfigFile, Error))
        {
            // ponytail: entries written before a failed flush stay written and are listed; rolling the
            // ini back would need a second write to the file that just refused one.
            ResponseJson->SetArrayField(TEXT("written"), Written);
            Subsystem->SendAutomationResponse(Socket, RequestId, false, Error, ResponseJson, TEXT("PERSIST_FAILED"));
            return true;
        }
        Written.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("[%s] %s=%s"), *Write.Section, *Write.Key, *Write.Value)));
        IConsoleVariable* ConsoleVariable = Write.bConsoleVariable ? IConsoleManager::Get().FindConsoleVariable(*Write.Key) : nullptr;
        if (ConsoleVariable)
        {
            // Same priority the engine applies [SystemSettings] with at startup; a value set from the
            // console outranks it, and then liveApplied leaves this variable out.
            ConsoleVariable->Set(*Write.Value, ECVF_SetBySystemSettingsIni);
            if (FMath::IsNearlyEqual(ConsoleVariable->GetFloat(), FCString::Atof(*Write.Value)))
            {
                LiveApplied.Add(MakeShared<FJsonValueString>(Write.Key));
            }
        }
    }
    if (Payload->HasField(TEXT("sampleRate")))
    {
        GetMutableDefault<UAudioSettings>()->VoiPSampleRate =
            Payload->GetNumberField(TEXT("sampleRate")) == 16000.0 ? EVoiceSampleRate::Low16000Hz : EVoiceSampleRate::Normal24000Hz;
    }

    ResponseJson->SetBoolField(TEXT("success"), true);
    ResponseJson->SetArrayField(TEXT("written"), Written);
    ResponseJson->SetStringField(TEXT("configFile"), ConfigFile);
    ResponseJson->SetArrayField(TEXT("liveApplied"), LiveApplied);
    ResponseJson->SetBoolField(TEXT("requiresRestart"), bRestartOnly);
    const FString Message = FString::Printf(
        TEXT("Wrote %d voice settings to %s; packaged games read them at startup. %d console variables apply in this editor now%s."),
        Written.Num(), *ConfigFile, LiveApplied.Num(),
        bRestartOnly ? TEXT("; voiceEnabled and sampleRate are read once at startup, so they take effect in the editor after a restart") : TEXT(""));
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}
