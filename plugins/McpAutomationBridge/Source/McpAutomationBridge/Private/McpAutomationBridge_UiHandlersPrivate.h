#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Templates/Function.h"

#if WITH_EDITOR
namespace McpUiHandlers {

using FUiScreenshotFallback = TFunction<bool()>;

bool HandleWidgetAuthoringAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &LowerSub,
    const TSharedPtr<FJsonObject> &Payload,
    const TSharedPtr<FJsonObject> &Resp, bool &bSuccess, FString &Message,
    FString &ErrorCode);

bool HandleScreenshotAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    bool bIsSystemControl, const FString &LowerSub,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    const TSharedPtr<FJsonObject> &Resp, bool &bSuccess, FString &Message,
    FString &ErrorCode, bool &bResponseSent,
    const FUiScreenshotFallback &ScreenshotFallback);

bool HandleEditorControlAction(const FString &LowerSub,
                               const TSharedPtr<FJsonObject> &Payload,
                               const TSharedPtr<FJsonObject> &Resp,
                               bool &bSuccess, FString &Message,
                               FString &ErrorCode);

bool HandleRuntimeWidgetAction(const FString &LowerSub,
                               const TSharedPtr<FJsonObject> &Payload,
                               const TSharedPtr<FJsonObject> &Resp,
                               bool &bSuccess, FString &Message,
                               FString &ErrorCode);

bool HandleProjectSettingsAction(const FString &LowerSub,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 const TSharedPtr<FJsonObject> &Resp,
                                 bool &bSuccess, FString &Message,
                                 FString &ErrorCode);

}
#endif
