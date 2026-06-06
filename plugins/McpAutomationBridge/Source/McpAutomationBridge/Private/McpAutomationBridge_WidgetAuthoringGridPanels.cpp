#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "McpAutomationBridge_WidgetAuthoringGuidRegistry.h"
#include "McpAutomationBridge_WidgetAuthoringPayload.h"
#include "McpAutomationBridge_WidgetAuthoringTreeMutation.h"
#include "McpAutomationBridge_WidgetAuthoringValidation.h"

#include "Components/GridPanel.h"
#include "Components/UniformGridPanel.h"
#include "Components/WrapBox.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringGridPanels(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    // =========================================================================
    // 19.2 Layout Panels (continued)
    // =========================================================================

    if (SubAction.Equals(TEXT("add_grid_panel"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("GridPanel"));
        int32 ColumnCount = static_cast<int32>(GetJsonNumberField(Payload, TEXT("columnCount"), 2));
        int32 RowCount = static_cast<int32>(GetJsonNumberField(Payload, TEXT("rowCount"), 2));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UGridPanel* GridPanel = WidgetBP->WidgetTree->ConstructWidget<UGridPanel>(UGridPanel::StaticClass(), FName(*SlotName));
        if (!GridPanel)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create grid panel"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Register widget GUID to prevent ensure failures during compilation
        RegisterWidgetGuid(WidgetBP, GridPanel);

        // CRITICAL: Use SafeAddWidgetToTree to properly handle root replacement and GUID cleanup
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!SafeAddWidgetToTree(WidgetBP, GridPanel, ParentSlot))
        {
            UnregisterWidgetGuid(WidgetBP, GridPanel);
            WidgetBP->WidgetTree->RemoveWidget(GridPanel);
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add grid panel to widget tree"), TEXT("TREE_ERROR"));
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
        ResultJson->SetStringField(TEXT("message"), TEXT("Added grid panel"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added grid panel"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_uniform_grid"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("UniformGridPanel"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UUniformGridPanel* UniformGrid = WidgetBP->WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), FName(*SlotName));
        if (!UniformGrid)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create uniform grid panel"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Register widget GUID to prevent ensure failures during compilation
        RegisterWidgetGuid(WidgetBP, UniformGrid);

        // Set slot padding if provided
        if (Payload->HasField(TEXT("slotPadding")))
        {
            TSharedPtr<FJsonObject> PaddingObj = GetObjectField(Payload, TEXT("slotPadding"));
            if (PaddingObj.IsValid())
            {
                FMargin SlotPadding;
                SlotPadding.Left = GetJsonNumberField(PaddingObj, TEXT("left"), 0.0);
                SlotPadding.Top = GetJsonNumberField(PaddingObj, TEXT("top"), 0.0);
                SlotPadding.Right = GetJsonNumberField(PaddingObj, TEXT("right"), 0.0);
                SlotPadding.Bottom = GetJsonNumberField(PaddingObj, TEXT("bottom"), 0.0);
                UniformGrid->SetSlotPadding(SlotPadding);
            }
        }

        // Set min desired slot size
        if (Payload->HasField(TEXT("minDesiredSlotWidth")))
        {
            UniformGrid->SetMinDesiredSlotWidth(static_cast<float>(GetJsonNumberField(Payload, TEXT("minDesiredSlotWidth"), 0.0)));
        }
        if (Payload->HasField(TEXT("minDesiredSlotHeight")))
        {
            UniformGrid->SetMinDesiredSlotHeight(static_cast<float>(GetJsonNumberField(Payload, TEXT("minDesiredSlotHeight"), 0.0)));
        }

        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!SafeAddWidgetToTree(WidgetBP, UniformGrid, ParentSlot))
        {
            UnregisterWidgetGuid(WidgetBP, UniformGrid);
            WidgetBP->WidgetTree->RemoveWidget(UniformGrid);
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add uniform grid to widget tree"), TEXT("TREE_ERROR"));
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
        ResultJson->SetStringField(TEXT("message"), TEXT("Added uniform grid panel"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added uniform grid panel"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_wrap_box"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("WrapBox"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWrapBox* WrapBox = WidgetBP->WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), FName(*SlotName));
        if (!WrapBox)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create wrap box"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Register widget GUID to prevent ensure failures during compilation
        RegisterWidgetGuid(WidgetBP, WrapBox);

        // Set inner slot padding if provided
        if (Payload->HasField(TEXT("innerSlotPadding")))
        {
            TSharedPtr<FJsonObject> PaddingObj = GetObjectField(Payload, TEXT("innerSlotPadding"));
            if (PaddingObj.IsValid())
            {
                FVector2D InnerPadding;
                InnerPadding.X = GetJsonNumberField(PaddingObj, TEXT("x"), 0.0);
                InnerPadding.Y = GetJsonNumberField(PaddingObj, TEXT("y"), 0.0);
                WrapBox->SetInnerSlotPadding(InnerPadding);
            }
        }

        // Set explicit wrap size
        // Note: SetWrapSize was introduced in UE 5.1
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        if (Payload->HasField(TEXT("wrapSize")))
        {
            WrapBox->SetWrapSize(static_cast<float>(GetJsonNumberField(Payload, TEXT("wrapSize"), 0.0)));
        }
#endif

        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!SafeAddWidgetToTree(WidgetBP, WrapBox, ParentSlot))
        {
            UnregisterWidgetGuid(WidgetBP, WrapBox);
            WidgetBP->WidgetTree->RemoveWidget(WrapBox);
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add wrap box to widget tree"), TEXT("TREE_ERROR"));
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
        ResultJson->SetStringField(TEXT("message"), TEXT("Added wrap box"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added wrap box"), ResultJson);
        return true;
    }

    return false;
}
}
