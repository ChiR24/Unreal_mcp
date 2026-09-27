#pragma once

#include "Domains/Render/McpAutomationBridge_RenderSupport.h"
#include "McpAutomationBridgeSubsystem.h"

#include "Engine/EngineTypes.h"

namespace McpRenderHandlers
{
inline bool IsLightChannelNumber(double Value, int32& OutChannel)
{
    const int32 Channel = FMath::RoundToInt(Value);
    if (FMath::IsNearlyEqual(Value, static_cast<double>(Channel)) && Channel >= 0 && Channel <= 2)
    {
        OutChannel = Channel;
        return true;
    }
    return false;
}

inline bool ReadLightChannels(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    TArray<int32>& OutChannels)
{
    OutChannels.Reset();
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (Payload.IsValid() && Payload->TryGetArrayField(TEXT("channels"), Values))
    {
        if (!Values || Values->Num() == 0)
        {
            Subsystem->SendAutomationError(
                Socket, RequestId, TEXT("channels must contain at least one channel index."), TEXT("INVALID_ARGUMENT"));
            return false;
        }
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            double Number = -1.0;
            int32 Channel = -1;
            if (!Value.IsValid() || !Value->TryGetNumber(Number) || !IsLightChannelNumber(Number, Channel))
            {
                Subsystem->SendAutomationError(
                    Socket, RequestId, TEXT("channels entries must be 0, 1, or 2."), TEXT("INVALID_ARGUMENT"));
                return false;
            }
            OutChannels.AddUnique(Channel);
        }
        return OutChannels.Num() > 0;
    }

    double ChannelNumber = -1.0;
    int32 Channel = -1;
    if (Payload.IsValid() && Payload->TryGetNumberField(TEXT("channel"), ChannelNumber) &&
        IsLightChannelNumber(ChannelNumber, Channel))
    {
        OutChannels.Add(Channel);
        return true;
    }
    Subsystem->SendAutomationError(
        Socket, RequestId, TEXT("channel must be 0, 1, or 2."), TEXT("INVALID_ARGUMENT"));
    return false;
}

inline void SetLightChannel(FLightingChannels& Channels, int32 Channel, bool bEnabled)
{
    if (Channel == 0)
    {
        Channels.bChannel0 = bEnabled;
    }
    else if (Channel == 1)
    {
        Channels.bChannel1 = bEnabled;
    }
    else
    {
        Channels.bChannel2 = bEnabled;
    }
}

inline void SetLightChannels(FLightingChannels& Channels, const TArray<int32>& ChannelList, bool bEnabled)
{
    for (int32 Channel : ChannelList)
    {
        SetLightChannel(Channels, Channel, bEnabled);
    }
}
}
