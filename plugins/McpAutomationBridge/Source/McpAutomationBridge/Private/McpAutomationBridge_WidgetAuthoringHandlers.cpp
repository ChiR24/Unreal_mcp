#include "McpAutomationBridge_WidgetAuthoringActions.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"

bool UMcpAutomationBridgeSubsystem::HandleManageWidgetAuthoringAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_widget_authoring"))
    {
        return false;
    }

    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SubAction = GetJsonStringField(Payload, TEXT("action"));
    }

    TSharedPtr<FJsonObject> ResultJson = McpHandlerUtils::CreateResultObject();
    using namespace WidgetAuthoringHandlers;
    static constexpr FWidgetAuthoringActionHandler Handlers[] = {
        HandleWidgetAuthoringCreation,
        HandleWidgetAuthoringPanelBasics,
        HandleWidgetAuthoringBasicVisuals,
        HandleWidgetAuthoringValueWidgets,
        HandleWidgetAuthoringInfo,
        HandleWidgetAuthoringGridPanels,
        HandleWidgetAuthoringScrollScalePanels,
        HandleWidgetAuthoringBorderPanel,
        HandleWidgetAuthoringInputWidgets,
        HandleWidgetAuthoringCollectionWidgets,
        HandleWidgetAuthoringCanvasSlotGeometry,
        HandleWidgetAuthoringSlotAppearance,
        HandleWidgetAuthoringStyleClipping,
        HandleWidgetAuthoringPropertyBindings,
        HandleWidgetAuthoringEventBindings,
        HandleWidgetAuthoringAnimationCore,
        HandleWidgetAuthoringMenuTemplates,
        HandleWidgetAuthoringHudElements,
        HandleWidgetAuthoringPreview,
        HandleWidgetAuthoringGenericComponent,
        HandleWidgetAuthoringUnifiedBinding,
        HandleWidgetAuthoringStyleVariables,
        HandleWidgetAuthoringSettingsTemplate,
        HandleWidgetAuthoringLoadingMinimapTemplates,
        HandleWidgetAuthoringObjectiveDamageTemplates,
        HandleWidgetAuthoringInventoryTemplate,
        HandleWidgetAuthoringDialogRadialTemplates,
        HandleWidgetAuthoringManipulation,
        HandleWidgetAuthoringAdditionalPanels,
        HandleWidgetAuthoringAdvancedStyling,
        HandleWidgetAuthoringAnimationQueries,
        HandleWidgetAuthoringLocalization,
        HandleWidgetAuthoringCreditsTemplate,
        HandleWidgetAuthoringShopTemplate,
        HandleWidgetAuthoringQuestTemplate
    };

    for (FWidgetAuthoringActionHandler Handler : Handlers)
    {
        if (Handler(*this, RequestId, SubAction, Payload, RequestingSocket, ResultJson))
        {
            return true;
        }
    }

    return false;
}
