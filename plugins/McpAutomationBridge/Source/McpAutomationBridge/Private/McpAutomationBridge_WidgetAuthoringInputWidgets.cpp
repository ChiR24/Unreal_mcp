#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "McpAutomationBridge_WidgetAuthoringGuidRegistry.h"
#include "McpAutomationBridge_WidgetAuthoringPayload.h"
#include "McpAutomationBridge_WidgetAuthoringTreeMutation.h"
#include "McpAutomationBridge_WidgetAuthoringValidation.h"

#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/RichTextBlock.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringInputWidgets(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    // =========================================================================
    // 19.3 Common Widgets (continued)
    // =========================================================================

    if (SubAction.Equals(TEXT("add_rich_text_block"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("RichTextBlock"));
        FString Text = GetJsonStringField(Payload, TEXT("text"), TEXT("Rich Text"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        URichTextBlock* RichTextBlock = WidgetBP->WidgetTree->ConstructWidget<URichTextBlock>(URichTextBlock::StaticClass(), FName(*SlotName));
        if (!RichTextBlock)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create rich text block"), TEXT("CREATION_ERROR"));
            return true;
        }

        RichTextBlock->SetText(FText::FromString(Text));

        // CRITICAL: Register widget GUID to prevent ensure failures during compilation
        RegisterWidgetGuid(WidgetBP, RichTextBlock);

        // CRITICAL: Use SafeAddWidgetToTree to properly handle root replacement and GUID cleanup
        // This prevents "Variable was deleted but still has a GUID" ensure failures
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!SafeAddWidgetToTree(WidgetBP, RichTextBlock, ParentSlot))
        {
            UnregisterWidgetGuid(WidgetBP, RichTextBlock);
            WidgetBP->WidgetTree->RemoveWidget(RichTextBlock);
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add rich text block to widget tree"), TEXT("TREE_ERROR"));
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
        ResultJson->SetStringField(TEXT("message"), TEXT("Added rich text block"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added rich text block"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_check_box"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("CheckBox"));
        bool bIsChecked = GetJsonBoolField(Payload, TEXT("isChecked"), false);

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UCheckBox* CheckBox = WidgetBP->WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), FName(*SlotName));
        if (!CheckBox)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create check box"), TEXT("CREATION_ERROR"));
            return true;
        }

        CheckBox->SetIsChecked(bIsChecked);

        // CRITICAL: Register widget GUID to prevent ensure failures during compilation
        RegisterWidgetGuid(WidgetBP, CheckBox);

        // CRITICAL: Use SafeAddWidgetToTree to properly handle root replacement and GUID cleanup
        // This prevents "Variable was deleted but still has a GUID" ensure failures
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!SafeAddWidgetToTree(WidgetBP, CheckBox, ParentSlot))
        {
            UnregisterWidgetGuid(WidgetBP, CheckBox);
            WidgetBP->WidgetTree->RemoveWidget(CheckBox);
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add check box to widget tree"), TEXT("TREE_ERROR"));
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
        ResultJson->SetStringField(TEXT("message"), TEXT("Added check box"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added check box"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_text_input"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("TextInput"));
        FString HintText = GetJsonStringField(Payload, TEXT("hintText"), TEXT(""));
        bool bMultiLine = GetJsonBoolField(Payload, TEXT("multiLine"), false);

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TextInput = nullptr;
        if (bMultiLine)
        {
            UMultiLineEditableTextBox* MultiLineText = WidgetBP->WidgetTree->ConstructWidget<UMultiLineEditableTextBox>(UMultiLineEditableTextBox::StaticClass(), FName(*SlotName));
            if (MultiLineText)
            {
                MultiLineText->SetHintText(FText::FromString(HintText));
                TextInput = MultiLineText;
                // CRITICAL: Register widget GUID to prevent ensure failures during compilation
                RegisterWidgetGuid(WidgetBP, MultiLineText);
            }
        }
        else
        {
            UEditableTextBox* SingleLineText = WidgetBP->WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), FName(*SlotName));
            if (SingleLineText)
            {
                SingleLineText->SetHintText(FText::FromString(HintText));
                TextInput = SingleLineText;
                // CRITICAL: Register widget GUID to prevent ensure failures during compilation
                RegisterWidgetGuid(WidgetBP, SingleLineText);
            }
        }

        if (!TextInput)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create text input"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Use SafeAddWidgetToTree to properly handle root replacement and GUID cleanup
        // This prevents "Variable was deleted but still has a GUID" ensure failures
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!SafeAddWidgetToTree(WidgetBP, TextInput, ParentSlot))
        {
            UnregisterWidgetGuid(WidgetBP, TextInput);
            WidgetBP->WidgetTree->RemoveWidget(TextInput);
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add text input to widget tree"), TEXT("TREE_ERROR"));
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
        ResultJson->SetStringField(TEXT("message"), TEXT("Added text input"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added text input"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_combo_box"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("ComboBox"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UComboBoxString* ComboBox = WidgetBP->WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), FName(*SlotName));
        if (!ComboBox)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create combo box"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Register widget GUID to prevent ensure failures during compilation
        RegisterWidgetGuid(WidgetBP, ComboBox);

        // Add options if provided
        const TArray<TSharedPtr<FJsonValue>>* Options = GetArrayField(Payload, TEXT("options"));
        if (Options)
        {
            for (const TSharedPtr<FJsonValue>& Option : *Options)
            {
                ComboBox->AddOption(Option->AsString());
            }
        }

        // Set selected option
        FString SelectedOption = GetJsonStringField(Payload, TEXT("selectedOption"));
        if (!SelectedOption.IsEmpty())
        {
            ComboBox->SetSelectedOption(SelectedOption);
        }

        // CRITICAL: Use SafeAddWidgetToTree to properly handle root replacement and GUID cleanup
        // This prevents "Variable was deleted but still has a GUID" ensure failures
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!SafeAddWidgetToTree(WidgetBP, ComboBox, ParentSlot))
        {
            UnregisterWidgetGuid(WidgetBP, ComboBox);
            WidgetBP->WidgetTree->RemoveWidget(ComboBox);
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add combo box to widget tree"), TEXT("TREE_ERROR"));
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
        ResultJson->SetStringField(TEXT("message"), TEXT("Added combo box"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added combo box"), ResultJson);
        return true;
    }

    return false;
}
}
