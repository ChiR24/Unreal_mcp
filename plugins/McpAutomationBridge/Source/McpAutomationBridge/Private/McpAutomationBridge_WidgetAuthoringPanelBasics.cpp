#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "McpAutomationBridge_WidgetAuthoringGuidRegistry.h"
#include "McpAutomationBridge_WidgetAuthoringTreeMutation.h"
#include "McpAutomationBridge_WidgetAuthoringValidation.h"

#include "Components/CanvasPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/VerticalBox.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringPanelBasics(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    // =========================================================================
    // 19.2 Layout Panels
    // =========================================================================

    if (SubAction.Equals(TEXT("add_canvas_panel"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("CanvasPanel"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Create canvas panel
        UCanvasPanel* CanvasPanel = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), FName(*SlotName));
        if (!CanvasPanel)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create canvas panel"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Register widget GUID to prevent ensure failures during compilation
        RegisterWidgetGuid(WidgetBP, CanvasPanel);

        // CRITICAL: Use SafeAddWidgetToTree to properly handle root replacement and GUID cleanup
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!SafeAddWidgetToTree(WidgetBP, CanvasPanel, ParentSlot))
        {
            UnregisterWidgetGuid(WidgetBP, CanvasPanel);
            WidgetBP->WidgetTree->RemoveWidget(CanvasPanel);
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add canvas panel to widget tree"), TEXT("TREE_ERROR"));
            return true;
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        // CRITICAL: Validate widget creation succeeded and check for engine errors
        FString ValidationError;
        if (!ValidateWidgetCreation(WidgetBP, SlotName, ValidationError))
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, ValidationError, TEXT("ENGINE_ERROR"));
            return true;
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added canvas panel"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added canvas panel"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_horizontal_box"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("HorizontalBox"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UHorizontalBox* HBox = WidgetBP->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*SlotName));
        if (!HBox)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create horizontal box"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Register widget GUID to prevent ensure failures during compilation
        RegisterWidgetGuid(WidgetBP, HBox);

        // CRITICAL: Use SafeAddWidgetToTree to properly handle root replacement and GUID cleanup
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!SafeAddWidgetToTree(WidgetBP, HBox, ParentSlot))
        {
            UnregisterWidgetGuid(WidgetBP, HBox);
            WidgetBP->WidgetTree->RemoveWidget(HBox);
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add horizontal box to widget tree"), TEXT("TREE_ERROR"));
            return true;
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        // CRITICAL: Validate widget creation succeeded and check for engine errors
        FString ValidationError;
        if (!ValidateWidgetCreation(WidgetBP, SlotName, ValidationError))
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, ValidationError, TEXT("ENGINE_ERROR"));
            return true;
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added horizontal box"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added horizontal box"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_vertical_box"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("VerticalBox"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UVerticalBox* VBox = WidgetBP->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName(*SlotName));
        if (!VBox)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create vertical box"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Register widget GUID to prevent ensure failures during compilation
        RegisterWidgetGuid(WidgetBP, VBox);

        // CRITICAL: Use SafeAddWidgetToTree to properly handle root replacement and GUID cleanup
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!SafeAddWidgetToTree(WidgetBP, VBox, ParentSlot))
        {
            UnregisterWidgetGuid(WidgetBP, VBox);
            WidgetBP->WidgetTree->RemoveWidget(VBox);
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add vertical box to widget tree"), TEXT("TREE_ERROR"));
            return true;
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        // CRITICAL: Validate widget creation succeeded and check for engine errors
        FString ValidationError;
        if (!ValidateWidgetCreation(WidgetBP, SlotName, ValidationError))
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, ValidationError, TEXT("ENGINE_ERROR"));
            return true;
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added vertical box"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added vertical box"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_overlay"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("Overlay"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UOverlay* OverlayWidget = WidgetBP->WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), FName(*SlotName));
        if (!OverlayWidget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create overlay"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Register widget GUID to prevent ensure failures during compilation
        RegisterWidgetGuid(WidgetBP, OverlayWidget);

        // CRITICAL: Use SafeAddWidgetToTree to properly handle root replacement and GUID cleanup
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!SafeAddWidgetToTree(WidgetBP, OverlayWidget, ParentSlot))
        {
            UnregisterWidgetGuid(WidgetBP, OverlayWidget);
            WidgetBP->WidgetTree->RemoveWidget(OverlayWidget);
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add overlay to widget tree"), TEXT("TREE_ERROR"));
            return true;
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        // CRITICAL: Validate widget creation succeeded and check for engine errors
        FString ValidationError;
        if (!ValidateWidgetCreation(WidgetBP, SlotName, ValidationError))
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, ValidationError, TEXT("ENGINE_ERROR"));
            return true;
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added overlay"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added overlay"), ResultJson);
        return true;
    }

    return false;
}
}
