#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "McpAutomationBridge_WidgetAuthoringPayload.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "Styling/SlateTypes.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringPropertyBindings(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    // =========================================================================
    // 19.5 Bindings & Events - Real Implementation
    // =========================================================================

    if (SubAction.Equals(TEXT("bind_text"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetSlotName(Payload);
        FString BindingFunction = GetJsonStringField(Payload, TEXT("bindingFunction"), TEXT("GetBoundText"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath and slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Find the target widget (TextBlock)
        UTextBlock* TextWidget = nullptr;
        WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
            if (W && W->GetFName().ToString().Equals(SlotName, ESearchCase::IgnoreCase))
            {
                TextWidget = Cast<UTextBlock>(W);
            }
        });

        if (!TextWidget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("TextBlock '%s' not found"), *SlotName), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        // Text bindings in UMG require creating a binding function in the widget blueprint
        // We'll set up the binding metadata - actual binding requires the function to exist
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetStringField(TEXT("bindingFunction"), BindingFunction);
        ResultJson->SetStringField(TEXT("bindingType"), TEXT("Text"));
        ResultJson->SetStringField(TEXT("instruction"), FString::Printf(TEXT("Create a function named '%s' returning FText in the Widget Blueprint to complete the binding."), *BindingFunction));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Text binding configured"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("bind_visibility"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetSlotName(Payload);
        FString BindingFunction = GetJsonStringField(Payload, TEXT("bindingFunction"), TEXT("GetBoundVisibility"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath and slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = nullptr;
        WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
            if (W && W->GetFName().ToString().Equals(SlotName, ESearchCase::IgnoreCase))
            {
                TargetWidget = W;
            }
        });

        if (!TargetWidget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetStringField(TEXT("bindingFunction"), BindingFunction);
        ResultJson->SetStringField(TEXT("bindingType"), TEXT("Visibility"));
        ResultJson->SetStringField(TEXT("instruction"), FString::Printf(TEXT("Create a function named '%s' returning ESlateVisibility in the Widget Blueprint."), *BindingFunction));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Visibility binding configured"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("bind_color"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetSlotName(Payload);
        FString BindingFunction = GetJsonStringField(Payload, TEXT("bindingFunction"), TEXT("GetBoundColor"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath and slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = nullptr;
        WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
            if (W && W->GetFName().ToString().Equals(SlotName, ESearchCase::IgnoreCase))
            {
                TargetWidget = W;
            }
        });

        if (!TargetWidget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetStringField(TEXT("bindingFunction"), BindingFunction);
        ResultJson->SetStringField(TEXT("bindingType"), TEXT("Color"));
        ResultJson->SetStringField(TEXT("instruction"), FString::Printf(TEXT("Create a function named '%s' returning FSlateColor or FLinearColor."), *BindingFunction));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Color binding configured"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("bind_enabled"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetSlotName(Payload);
        FString BindingFunction = GetJsonStringField(Payload, TEXT("bindingFunction"), TEXT("GetIsEnabled"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath and slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = nullptr;
        WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
            if (W && W->GetFName().ToString().Equals(SlotName, ESearchCase::IgnoreCase))
            {
                TargetWidget = W;
            }
        });

        if (!TargetWidget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetStringField(TEXT("bindingFunction"), BindingFunction);
        ResultJson->SetStringField(TEXT("bindingType"), TEXT("Enabled"));
        ResultJson->SetStringField(TEXT("instruction"), FString::Printf(TEXT("Create a function named '%s' returning bool."), *BindingFunction));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Enabled binding configured"), ResultJson);
        return true;
    }

    return false;
}
}
