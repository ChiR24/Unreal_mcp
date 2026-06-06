#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "McpAutomationBridge_WidgetAuthoringGuidRegistry.h"
#include "McpAutomationBridge_WidgetAuthoringTreeMutation.h"
#include "McpAutomationBridge_WidgetAuthoringValidation.h"

#include "Components/ListView.h"
#include "Components/SpinBox.h"
#include "Components/TreeView.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringCollectionWidgets(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    if (SubAction.Equals(TEXT("add_spin_box"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("SpinBox"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        USpinBox* SpinBox = WidgetBP->WidgetTree->ConstructWidget<USpinBox>(USpinBox::StaticClass(), FName(*SlotName));
        if (!SpinBox)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create spin box"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Register widget GUID to prevent ensure failures during compilation
        RegisterWidgetGuid(WidgetBP, SpinBox);

        // Set value
        if (Payload->HasField(TEXT("value")))
        {
            SpinBox->SetValue(static_cast<float>(GetJsonNumberField(Payload, TEXT("value"), 0.0)));
        }
        // Set min/max
        if (Payload->HasField(TEXT("minValue")))
        {
            SpinBox->SetMinValue(static_cast<float>(GetJsonNumberField(Payload, TEXT("minValue"), 0.0)));
        }
        if (Payload->HasField(TEXT("maxValue")))
        {
            SpinBox->SetMaxValue(static_cast<float>(GetJsonNumberField(Payload, TEXT("maxValue"), 100.0)));
        }
        // Set delta
        if (Payload->HasField(TEXT("delta")))
        {
            SpinBox->SetDelta(static_cast<float>(GetJsonNumberField(Payload, TEXT("delta"), 1.0)));
        }

        // CRITICAL: Use SafeAddWidgetToTree to properly handle root replacement and GUID cleanup
        // This prevents "Variable was deleted but still has a GUID" ensure failures
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!SafeAddWidgetToTree(WidgetBP, SpinBox, ParentSlot))
        {
            UnregisterWidgetGuid(WidgetBP, SpinBox);
            WidgetBP->WidgetTree->RemoveWidget(SpinBox);
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add spin box to widget tree"), TEXT("TREE_ERROR"));
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
        ResultJson->SetStringField(TEXT("message"), TEXT("Added spin box"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added spin box"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_list_view"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("ListView"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UListView* ListView = WidgetBP->WidgetTree->ConstructWidget<UListView>(UListView::StaticClass(), FName(*SlotName));
        if (!ListView)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create list view"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Register widget GUID to prevent ensure failures during compilation
        RegisterWidgetGuid(WidgetBP, ListView);

        // CRITICAL: Use SafeAddWidgetToTree to properly handle root replacement and GUID cleanup
        // This prevents "Variable was deleted but still has a GUID" ensure failures
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!SafeAddWidgetToTree(WidgetBP, ListView, ParentSlot))
        {
            UnregisterWidgetGuid(WidgetBP, ListView);
            WidgetBP->WidgetTree->RemoveWidget(ListView);
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add list view to widget tree"), TEXT("TREE_ERROR"));
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
        ResultJson->SetStringField(TEXT("message"), TEXT("Added list view"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added list view"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_tree_view"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("TreeView"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UTreeView* TreeView = WidgetBP->WidgetTree->ConstructWidget<UTreeView>(UTreeView::StaticClass(), FName(*SlotName));
        if (!TreeView)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create tree view"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Register widget GUID to prevent ensure failures during compilation
        RegisterWidgetGuid(WidgetBP, TreeView);

        // CRITICAL: Use SafeAddWidgetToTree to properly handle root replacement and GUID cleanup
        // This prevents "Variable was deleted but still has a GUID" ensure failures
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!SafeAddWidgetToTree(WidgetBP, TreeView, ParentSlot))
        {
            UnregisterWidgetGuid(WidgetBP, TreeView);
            WidgetBP->WidgetTree->RemoveWidget(TreeView);
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add tree view to widget tree"), TEXT("TREE_ERROR"));
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
        ResultJson->SetStringField(TEXT("message"), TEXT("Added tree view"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added tree view"), ResultJson);
        return true;
    }

    return false;
}
}
