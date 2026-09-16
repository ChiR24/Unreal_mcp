#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringInfo(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    if (SubAction.Equals(TEXT("get_widget_info"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> WidgetInfo = McpHandlerUtils::CreateResultObject();

        WidgetInfo->SetStringField(TEXT("widgetClass"), WidgetBP->GetName());
        if (WidgetBP->ParentClass)
        {
            WidgetInfo->SetStringField(TEXT("parentClass"), WidgetBP->ParentClass->GetName());
        }

        // slots[] was a list of bare names: no class, no hierarchy, no text, so
        // identifying which entry was the title - and what it said - cost one
        // get_widget_slot_info call per widget. Names stay for callers that only
        // need them; widgets[] carries what the tree actually holds.
        TArray<TSharedPtr<FJsonValue>> SlotsArray;
        TArray<TSharedPtr<FJsonValue>> WidgetsArray;
        if (WidgetBP->WidgetTree)
        {
            WidgetBP->WidgetTree->ForEachWidget([&](UWidget* Widget) {
                if (!Widget) { return; }
                SlotsArray.Add(MakeShared<FJsonValueString>(Widget->GetName()));
                TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
                Entry->SetStringField(TEXT("name"), Widget->GetName());
                Entry->SetStringField(TEXT("widgetClass"), Widget->GetClass()->GetName());
                if (UPanelWidget* ParentPanel = Widget->GetParent())
                {
                    Entry->SetStringField(TEXT("parentName"), ParentPanel->GetName());
                }
                if (Widget->Slot)
                {
                    Entry->SetStringField(TEXT("slotClass"), Widget->Slot->GetClass()->GetName());
                }
                Entry->SetBoolField(TEXT("isVariable"), Widget->bIsVariable);
                if (UTextBlock* TextWidget = Cast<UTextBlock>(Widget))
                {
                    Entry->SetStringField(TEXT("text"), TextWidget->GetText().ToString());
                }
                WidgetsArray.Add(MakeShared<FJsonValueObject>(Entry));
            });
            if (WidgetBP->WidgetTree->RootWidget)
            {
                WidgetInfo->SetStringField(TEXT("rootWidget"), WidgetBP->WidgetTree->RootWidget->GetName());
            }
        }
        WidgetInfo->SetArrayField(TEXT("slots"), SlotsArray);
        WidgetInfo->SetArrayField(TEXT("widgets"), WidgetsArray);

        TArray<TSharedPtr<FJsonValue>> AnimsArray;
        for (UWidgetAnimation* Anim : WidgetBP->Animations)
        {
            if (Anim)
            {
                TSharedPtr<FJsonValue> AnimValue = MakeShared<FJsonValueString>(Anim->GetName());
                AnimsArray.Add(AnimValue);
            }
        }
        WidgetInfo->SetArrayField(TEXT("animations"), AnimsArray);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetObjectField(TEXT("widgetInfo"), WidgetInfo);

        McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Retrieved widget info"), ResultJson);
        return true;
    }

    return false;
}
}
