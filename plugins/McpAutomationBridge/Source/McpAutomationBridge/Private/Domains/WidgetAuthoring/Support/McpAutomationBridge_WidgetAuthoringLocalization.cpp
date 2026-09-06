#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
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
            FText LocalizedText = FText::ChangeKey(FTextKey(Namespace), FTextKey(Key), TextWidget->GetText());
            TextWidget->SetText(LocalizedText);
            bApplied = true;
        }

        if (!bApplied)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Widget '%s' is a %s; set_localization_key applies to TextBlock widgets"),
                    *SlotName, *TargetWidget->GetClass()->GetName()),
                TEXT("UNSUPPORTED_WIDGET"));
            return true;
        }
        WidgetAuthoringHelpers::MarkWidgetBlueprintModifiedAndSave(WidgetBP);

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

        // Dogfood #188: a missing table/key used to answer success with data.success=false.
        UStringTable* StringTableAsset = LoadObject<UStringTable>(nullptr, *StringTableId);
        if (!StringTableAsset)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("String table not found: %s"), *StringTableId), TEXT("STRING_TABLE_NOT_FOUND"));
            return true;
        }
        if (!StringTableAsset->GetStringTable()->FindEntry(FTextKey(StringKey)).IsValid())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("String table %s has no entry '%s'"), *StringTableId, *StringKey), TEXT("STRING_KEY_NOT_FOUND"));
            return true;
        }
        if (!Cast<UTextBlock>(TargetWidget))
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' is a %s; bind_localized_text supports TextBlock widgets"), *SlotName, *TargetWidget->GetClass()->GetName()), TEXT("UNSUPPORTED_WIDGET"));
            return true;
        }
        bool bBound = false;
        if (UTextBlock* TextWidget = Cast<UTextBlock>(TargetWidget))
        {
            FText LocalizedText = FText::FromStringTable(FName(*StringTableId), StringKey);
            if (!LocalizedText.IsEmpty())
            {
                TextWidget->SetText(LocalizedText);
                bBound = true;
            }
        }

        WidgetAuthoringHelpers::MarkWidgetBlueprintModifiedAndSave(WidgetBP);

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
