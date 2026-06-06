#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SessionsHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
bool HandleConfigureVoiceSettings(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace SessionsHelpers;

    if (!Payload.IsValid() || !Payload->HasField(TEXT("voiceSettings")))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("voiceSettings is required with at least one setting (volume, noiseGateThreshold, noiseSuppression, echoCancellation, or sampleRate)"), nullptr);
        return true;
    }

    TSharedPtr<FJsonObject> VoiceSettings = GetObjectField(Payload, TEXT("voiceSettings"));
    if (!VoiceSettings.IsValid())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("voiceSettings must be a valid object"), nullptr);
        return true;
    }

    double Volume = FMath::Clamp(GetJsonNumberField(VoiceSettings, TEXT("volume"), 1.0), 0.0, 1.0);
    double NoiseGateThreshold = GetJsonNumberField(VoiceSettings, TEXT("noiseGateThreshold"), 0.01);
    bool bNoiseSuppression = GetJsonBoolField(VoiceSettings, TEXT("noiseSuppression"), true);
    bool bEchoCancellation = GetJsonBoolField(VoiceSettings, TEXT("echoCancellation"), true);
    int32 SampleRate = static_cast<int32>(GetJsonNumberField(VoiceSettings, TEXT("sampleRate"), 16000));

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    TSharedPtr<FJsonObject> ConfiguredSettings = McpHandlerUtils::CreateResultObject();
    ConfiguredSettings->SetNumberField(TEXT("volume"), Volume);
    ConfiguredSettings->SetNumberField(TEXT("noiseGateThreshold"), NoiseGateThreshold);
    ConfiguredSettings->SetBoolField(TEXT("noiseSuppression"), bNoiseSuppression);
    ConfiguredSettings->SetBoolField(TEXT("echoCancellation"), bEchoCancellation);
    ConfiguredSettings->SetNumberField(TEXT("sampleRate"), SampleRate);
    ResponseJson->SetObjectField(TEXT("voiceSettings"), ConfiguredSettings);

    FString Message = TEXT("Voice chat settings configured successfully");
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

bool HandleSetVoiceChannel(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (!Payload.IsValid() || !Payload->HasField(TEXT("channelName")))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("channelName is required. Optionally provide channelType (Team, Global, Proximity, Party)"), nullptr);
        return true;
    }

    FString ChannelName = GetJsonStringField(Payload, TEXT("channelName"), TEXT("Default"));
    FString ChannelType = GetJsonStringField(Payload, TEXT("channelType"), TEXT("Global"));
    TArray<FString> ValidTypes = { TEXT("Team"), TEXT("Global"), TEXT("Proximity"), TEXT("Party") };
    if (!ValidTypes.Contains(ChannelType))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Invalid voice channel type: %s. Valid types: Team, Global, Proximity, Party"), *ChannelType), nullptr);
        return true;
    }

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("channelName"), ChannelName);
    ResponseJson->SetStringField(TEXT("channelType"), ChannelType);

    FString Message = FString::Printf(TEXT("Voice channel '%s' set with type: %s"), *ChannelName, *ChannelType);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

bool HandleSetVoiceAttenuation(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (!Payload.IsValid() || !Payload->HasField(TEXT("attenuationRadius")))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("attenuationRadius is required. Optionally provide attenuationFalloff (0.1-10.0)"), nullptr);
        return true;
    }

    double AttenuationRadius = GetJsonNumberField(Payload, TEXT("attenuationRadius"), 2000.0);
    double AttenuationFalloff = GetJsonNumberField(Payload, TEXT("attenuationFalloff"), 1.0);
    AttenuationRadius = FMath::Max(AttenuationRadius, 0.0);
    AttenuationFalloff = FMath::Clamp(AttenuationFalloff, 0.1, 10.0);

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetNumberField(TEXT("attenuationRadius"), AttenuationRadius);
    ResponseJson->SetNumberField(TEXT("attenuationFalloff"), AttenuationFalloff);

    FString Message = FString::Printf(TEXT("Voice attenuation configured: radius=%.0f, falloff=%.2f"),
        AttenuationRadius, AttenuationFalloff);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

bool HandleConfigurePushToTalk(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (!Payload.IsValid() || !Payload->HasField(TEXT("pushToTalkEnabled")))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("pushToTalkEnabled is required. Optionally provide pushToTalkKey (e.g., 'V', 'Space', 'LeftShift')"), nullptr);
        return true;
    }

    bool bPushToTalkEnabled = GetJsonBoolField(Payload, TEXT("pushToTalkEnabled"), false);
    FString PushToTalkKey = GetJsonStringField(Payload, TEXT("pushToTalkKey"), TEXT("V"));

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetBoolField(TEXT("pushToTalkEnabled"), bPushToTalkEnabled);
    ResponseJson->SetStringField(TEXT("pushToTalkKey"), PushToTalkKey);

    FString Message = FString::Printf(TEXT("Push-to-talk %s%s"),
        bPushToTalkEnabled ? TEXT("enabled") : TEXT("disabled"),
        bPushToTalkEnabled ? *FString::Printf(TEXT(" (key: %s)"), *PushToTalkKey) : TEXT(""));

    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}
#endif
