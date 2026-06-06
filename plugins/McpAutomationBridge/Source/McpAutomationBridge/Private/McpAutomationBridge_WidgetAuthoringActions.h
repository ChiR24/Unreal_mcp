#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class FMcpBridgeWebSocket;
class UMcpAutomationBridgeSubsystem;

namespace WidgetAuthoringHandlers
{
using FWidgetAuthoringActionHandler = bool (*)(UMcpAutomationBridgeSubsystem&, const FString&, const FString&, const TSharedPtr<FJsonObject>&, TSharedPtr<FMcpBridgeWebSocket>, TSharedPtr<FJsonObject>);

bool HandleWidgetAuthoringCreation(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringPanelBasics(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringBasicVisuals(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringValueWidgets(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringInfo(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringGridPanels(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringScrollScalePanels(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringBorderPanel(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringInputWidgets(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringCollectionWidgets(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringCanvasSlotGeometry(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringSlotAppearance(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringStyleClipping(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringPropertyBindings(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringEventBindings(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringAnimationCore(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringMenuTemplates(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringHudElements(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringPreview(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringGenericComponent(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringUnifiedBinding(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringStyleVariables(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringSettingsTemplate(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringLoadingMinimapTemplates(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringObjectiveDamageTemplates(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringInventoryTemplate(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringDialogRadialTemplates(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringManipulation(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringAdditionalPanels(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringAdvancedStyling(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringAnimationQueries(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringLocalization(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringCreditsTemplate(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringShopTemplate(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
bool HandleWidgetAuthoringQuestTemplate(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson);
}
