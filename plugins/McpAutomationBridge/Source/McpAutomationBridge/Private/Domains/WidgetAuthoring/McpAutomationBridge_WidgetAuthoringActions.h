#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class FMcpBridgeWebSocket;
class UMcpAutomationBridgeSubsystem;
class UWidget;

namespace WidgetAuthoringHandlers
{
using FWidgetAuthoringActionHandler = bool (*)(UMcpAutomationBridgeSubsystem&, const FString&, const FString&, const TSharedPtr<FJsonObject>&, TSharedPtr<FMcpBridgeWebSocket>, TSharedPtr<FJsonObject>);

bool HandleWidgetAuthoringCreation(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringTypedComponents(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringTypedPanels(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);

// The one add_<widget> flow: construct WidgetClass in the widget blueprint at
// `widgetPath`, let Configure apply the payload's properties, insert it through
// SafeAddWidgetToTree (rolled back on failure), save, validate and reply.
bool AddConfiguredWidget(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson, UClass* WidgetClass,
    const TCHAR* DefaultSlotName, const TCHAR* Label, TFunctionRef<void(UWidget*)> Configure);
bool HandleWidgetAuthoringInfo(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringCanvasSlotGeometry(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringSlotAppearance(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringStyleClipping(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringEventBindings(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringAnimationCore(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringPreview(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringGenericComponent(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringManipulation(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringDuplicate(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringAdvancedStyling(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringAnimationQueries(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringLocalization(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringHudElements(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringTreeBuild(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringScreens(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringPropertyBindings(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
}
