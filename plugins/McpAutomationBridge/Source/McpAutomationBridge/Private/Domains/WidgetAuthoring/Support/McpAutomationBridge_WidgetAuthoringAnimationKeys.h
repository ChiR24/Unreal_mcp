// McpAutomationBridge_WidgetAuthoringAnimationKeys.h — MovieScene key authoring for widget animations.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpBridgeWebSocket;
class UMcpAutomationBridgeSubsystem;
class UWidget;
class UWidgetAnimation;
class UWidgetBlueprint;

namespace WidgetAuthoringHelpers
{
struct FMcpWidgetKeyResult
{
    FString TrackType;
    FString PropertyName;
    FString TrackClass;
    FString BindingGuid;
    int32 ChannelCount = 0;
    int32 KeyCount = 0;
    int32 FrameNumber = 0;
    bool bCreatedTrack = false;
    bool bCreatedBinding = false;
};

// Finds or creates the widget binding and property track, then writes channel keys.
// trackType: opacity | color | translation | scale | angle | shear | transform.
bool McpAuthorWidgetAnimationKey(UWidgetBlueprint* WidgetBP, UWidgetAnimation* Animation, UWidget* Target,
                                 const TSharedPtr<FJsonObject>& Payload, FMcpWidgetKeyResult& Out,
                                 FString& OutError, FString& OutErrorCode);
}

namespace WidgetAuthoringHandlers
{
bool HandleWidgetAuthoringAnimationKeyframe(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId,
                                            const TSharedPtr<FJsonObject>& Payload,
                                            TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
                                            TSharedPtr<FJsonObject> ResultJson);
}
