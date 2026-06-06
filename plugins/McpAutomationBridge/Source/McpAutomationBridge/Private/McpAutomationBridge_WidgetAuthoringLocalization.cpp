#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringLocalization(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    // =========================================================================
    // 19.15 Localization Actions
    // =========================================================================

    if (SubAction.Equals(TEXT("set_localization_key"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
        FString Namespace = GetJsonStringField(Payload, TEXT("namespace"), TEXT("Game"));
        FString Key = GetJsonStringField(Payload, TEXT("key"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty() || Key.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, slotName, key"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!TargetWidget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("NOT_FOUND"));
            return true;
        }

        bool bApplied = false;
        if (UTextBlock* TextWidget = Cast<UTextBlock>(TargetWidget))
        {
            // Create localized text reference
            FText LocalizedText = FText::ChangeKey(FTextKey(Namespace), FTextKey(Key), TextWidget->GetText());
            TextWidget->SetText(LocalizedText);
            bApplied = true;
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), bApplied);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetStringField(TEXT("namespace"), Namespace);
        ResultJson->SetStringField(TEXT("key"), Key);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Set localization key"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("bind_localized_text"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
        FString StringTableId = GetJsonStringField(Payload, TEXT("stringTableId"));
        FString StringKey = GetJsonStringField(Payload, TEXT("stringKey"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty() || StringTableId.IsEmpty() || StringKey.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!TargetWidget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("NOT_FOUND"));
            return true;
        }

        bool bBound = false;
        if (UTextBlock* TextWidget = Cast<UTextBlock>(TargetWidget))
        {
            // Try to get text from string table
            FText LocalizedText = FText::FromStringTable(FName(*StringTableId), StringKey);
            if (!LocalizedText.IsEmpty())
            {
                TextWidget->SetText(LocalizedText);
                bBound = true;
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), bBound);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetStringField(TEXT("stringTableId"), StringTableId);
        ResultJson->SetStringField(TEXT("stringKey"), StringKey);
        if (!bBound)
        {
            ResultJson->SetStringField(TEXT("note"), TEXT("String table entry not found or widget is not a text widget"));
        }

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Bound localized text"), ResultJson);
        return true;
    }

    return false;
}
}
