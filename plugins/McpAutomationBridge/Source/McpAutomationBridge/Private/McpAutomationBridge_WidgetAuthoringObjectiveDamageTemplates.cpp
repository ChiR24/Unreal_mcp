#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "McpAutomationBridge_WidgetAuthoringTreeMutation.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringObjectiveDamageTemplates(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    if (SubAction.Equals(TEXT("add_interaction_prompt"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("InteractionPrompt"));
        FString DefaultText = GetJsonStringField(Payload, TEXT("text"), TEXT("Press E to Interact"));

        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // CRITICAL: Use CreateAndRegisterWidget to register GUIDs IMMEDIATELY after creation.
        // This prevents ensure failures if compilation is triggered during widget creation.
        // The compiler's ValidateAndFixUpVariableGuids() expects all widgets to be in the GUID map.

        // Create prompt container
        UHorizontalBox* PromptContainer = CreateAndRegisterWidget<UHorizontalBox>(WidgetBP, WidgetBP->WidgetTree, *SlotName);

        // Key icon
        UImage* KeyIcon = CreateAndRegisterWidget<UImage>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_KeyIcon")));
        PromptContainer->AddChild(KeyIcon);

        // Prompt text
        UTextBlock* PromptText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_Text")));
        PromptText->SetText(FText::FromString(DefaultText));
        PromptContainer->AddChild(PromptText);

        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (Parent)
        {
            Parent->AddChild(PromptContainer);
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(PromptContainer->Slot))
            {
                Slot->SetAnchors(FAnchors(0.5f, 0.7f, 0.5f, 0.7f)); // Center-bottom area
                Slot->SetAlignment(FVector2D(0.5f, 0.5f));
                Slot->SetAutoSize(true);
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added interaction prompt"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_objective_tracker"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("ObjectiveTracker"));

        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // CRITICAL: Use CreateAndRegisterWidget to register GUIDs IMMEDIATELY after creation.
        // This prevents ensure failures if compilation is triggered during widget creation.
        // The compiler's ValidateAndFixUpVariableGuids() expects all widgets to be in the GUID map.

        // Create objective container
        UVerticalBox* ObjectiveContainer = CreateAndRegisterWidget<UVerticalBox>(WidgetBP, WidgetBP->WidgetTree, *SlotName);

        // Objective title
        UTextBlock* ObjectiveTitle = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_Title")));
        ObjectiveTitle->SetText(FText::FromString(TEXT("Objectives")));
        ObjectiveContainer->AddChild(ObjectiveTitle);

        // Objective list (vertical box for dynamic entries)
        UVerticalBox* ObjectiveList = CreateAndRegisterWidget<UVerticalBox>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_List")));
        ObjectiveContainer->AddChild(ObjectiveList);

        // Sample objective item
        UHorizontalBox* SampleObjective = CreateAndRegisterWidget<UHorizontalBox>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_SampleItem")));
        UCheckBox* ObjectiveCheck = CreateAndRegisterWidget<UCheckBox>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_Check")));
        UTextBlock* ObjectiveText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_ItemText")));
        ObjectiveText->SetText(FText::FromString(TEXT("Sample Objective")));
        SampleObjective->AddChild(ObjectiveCheck);
        SampleObjective->AddChild(ObjectiveText);
        ObjectiveList->AddChild(SampleObjective);

        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (Parent)
        {
            Parent->AddChild(ObjectiveContainer);
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(ObjectiveContainer->Slot))
            {
                Slot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f)); // Top-right
                Slot->SetAlignment(FVector2D(1.0f, 0.0f));
                Slot->SetPosition(FVector2D(-20.0f, 100.0f));
                Slot->SetAutoSize(true);
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added objective tracker"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_damage_indicator"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("DamageIndicator"));

        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // CRITICAL: Use CreateAndRegisterWidget to register GUIDs IMMEDIATELY after creation.
        // This prevents ensure failures if compilation is triggered during widget creation.
        // The compiler's ValidateAndFixUpVariableGuids() expects all widgets to be in the GUID map.

        // Create damage indicator overlay (full screen)
        UOverlay* DamageOverlay = CreateAndRegisterWidget<UOverlay>(WidgetBP, WidgetBP->WidgetTree, *SlotName);

        // Blood vignette image (edge damage indicator)
        UImage* VignetteImage = CreateAndRegisterWidget<UImage>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_Vignette")));
        VignetteImage->SetVisibility(ESlateVisibility::Hidden);
        DamageOverlay->AddChild(VignetteImage);

        // Directional damage arrows container
        UCanvasPanel* DirectionalCanvas = CreateAndRegisterWidget<UCanvasPanel>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_Directional")));
        DamageOverlay->AddChild(DirectionalCanvas);

        // Add directional indicators (N, S, E, W)
        for (const FString& Dir : { TEXT("Top"), TEXT("Bottom"), TEXT("Left"), TEXT("Right") })
        {
            UImage* DirIndicator = CreateAndRegisterWidget<UImage>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_") + Dir));
            DirIndicator->SetVisibility(ESlateVisibility::Hidden);
            DirectionalCanvas->AddChild(DirIndicator);
        }

        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (Parent)
        {
            Parent->AddChild(DamageOverlay);
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(DamageOverlay->Slot))
            {
                Slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f)); // Full screen
                Slot->SetOffsets(FMargin(0.0f));
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added damage indicator"), ResultJson);
        return true;
    }

    return false;
}
}
