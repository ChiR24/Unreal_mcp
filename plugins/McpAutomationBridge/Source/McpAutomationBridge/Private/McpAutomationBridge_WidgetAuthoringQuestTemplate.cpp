#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "McpAutomationBridge_WidgetAuthoringTreeMutation.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
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

bool HandleWidgetAuthoringQuestTemplate(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    if (SubAction.Equals(TEXT("add_quest_tracker"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("QuestTracker"));

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

        // Quest tracker container
        UVerticalBox* QuestContainer = CreateAndRegisterWidget<UVerticalBox>(WidgetBP, WidgetBP->WidgetTree, *SlotName);

        // Quest header
        UTextBlock* QuestHeader = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_Header")));
        QuestHeader->SetText(FText::FromString(TEXT("Active Quest")));
        QuestContainer->AddChild(QuestHeader);

        // Quest title
        UTextBlock* QuestTitle = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_Title")));
        QuestTitle->SetText(FText::FromString(TEXT("Quest Name")));
        QuestContainer->AddChild(QuestTitle);

        // Quest objectives list
        UVerticalBox* ObjectivesList = CreateAndRegisterWidget<UVerticalBox>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_Objectives")));
        QuestContainer->AddChild(ObjectivesList);

        // Sample objectives
        for (int32 i = 1; i <= 3; ++i)
        {
            UHorizontalBox* ObjRow = CreateAndRegisterWidget<UHorizontalBox>(WidgetBP, WidgetBP->WidgetTree, *FString::Printf(TEXT("%s_Objective_%d"), *SlotName, i));

            UCheckBox* ObjCheck = CreateAndRegisterWidget<UCheckBox>(WidgetBP, WidgetBP->WidgetTree, *FString::Printf(TEXT("%s_ObjCheck_%d"), *SlotName, i));
            ObjRow->AddChild(ObjCheck);

            UTextBlock* ObjText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, *FString::Printf(TEXT("%s_ObjText_%d"), *SlotName, i));
            ObjText->SetText(FText::FromString(FString::Printf(TEXT("Objective %d (0/1)"), i)));
            ObjRow->AddChild(ObjText);

            ObjectivesList->AddChild(ObjRow);
        }

        // Quest rewards preview
        UHorizontalBox* RewardsRow = CreateAndRegisterWidget<UHorizontalBox>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_Rewards")));
        QuestContainer->AddChild(RewardsRow);

        UTextBlock* RewardsLabel = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_RewardsLabel")));
        RewardsLabel->SetText(FText::FromString(TEXT("Rewards: ")));
        RewardsRow->AddChild(RewardsLabel);

        UImage* RewardIcon = CreateAndRegisterWidget<UImage>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_RewardIcon")));
        RewardsRow->AddChild(RewardIcon);

        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (Parent)
        {
            Parent->AddChild(QuestContainer);
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(QuestContainer->Slot))
            {
                Slot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f)); // Top-left
                Slot->SetAlignment(FVector2D(0.0f, 0.0f));
                Slot->SetPosition(FVector2D(20.0f, 100.0f));
                Slot->SetAutoSize(true);
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added quest tracker"), ResultJson);
        return true;
    }

    return false;
}
}
