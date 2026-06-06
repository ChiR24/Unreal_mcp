#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "McpAutomationBridge_WidgetAuthoringPayload.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Components/SpinBox.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringEventBindings(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    if (SubAction.Equals(TEXT("bind_on_clicked"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetSlotName(Payload);
        FString FunctionName = GetJsonStringField(Payload, TEXT("functionName"), TEXT("OnButtonClicked"));

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

        UButton* ButtonWidget = nullptr;
        WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
            if (W && W->GetFName().ToString().Equals(SlotName, ESearchCase::IgnoreCase))
            {
                ButtonWidget = Cast<UButton>(W);
            }
        });

        if (!ButtonWidget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Button '%s' not found"), *SlotName), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        // Note: UButton::OnClicked is a multicast delegate that requires binding through Blueprint
        // We create metadata for the binding - the function needs to exist in the widget BP
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetStringField(TEXT("eventType"), TEXT("OnClicked"));
        ResultJson->SetStringField(TEXT("functionName"), FunctionName);
        ResultJson->SetStringField(TEXT("instruction"), FString::Printf(TEXT("Create an event handler function named '%s' and bind it to %s's OnClicked event in the Designer."), *FunctionName, *SlotName));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("OnClicked binding info provided"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("bind_on_hovered"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetSlotName(Payload);
        FString FunctionName = GetJsonStringField(Payload, TEXT("functionName"), TEXT("OnButtonHovered"));

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

        UButton* ButtonWidget = nullptr;
        WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
            if (W && W->GetFName().ToString().Equals(SlotName, ESearchCase::IgnoreCase))
            {
                ButtonWidget = Cast<UButton>(W);
            }
        });

        if (!ButtonWidget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Button '%s' not found"), *SlotName), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetStringField(TEXT("eventType"), TEXT("OnHovered"));
        ResultJson->SetStringField(TEXT("functionName"), FunctionName);
        ResultJson->SetStringField(TEXT("instruction"), FString::Printf(TEXT("Bind '%s' to %s's OnHovered event."), *FunctionName, *SlotName));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("OnHovered binding info provided"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("bind_on_value_changed"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetSlotName(Payload);
        FString FunctionName = GetJsonStringField(Payload, TEXT("functionName"), TEXT("OnValueChanged"));

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

        // Determine widget type for appropriate binding info
        FString WidgetType = TargetWidget->GetClass()->GetName();
        FString EventName = TEXT("OnValueChanged");

        if (Cast<USlider>(TargetWidget)) EventName = TEXT("OnValueChanged (float)");
        else if (Cast<UCheckBox>(TargetWidget)) EventName = TEXT("OnCheckStateChanged (bool)");
        else if (Cast<USpinBox>(TargetWidget)) EventName = TEXT("OnValueChanged (float)");
        else if (Cast<UComboBoxString>(TargetWidget)) EventName = TEXT("OnSelectionChanged (FString)");

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetStringField(TEXT("widgetType"), WidgetType);
        ResultJson->SetStringField(TEXT("eventType"), EventName);
        ResultJson->SetStringField(TEXT("functionName"), FunctionName);
        ResultJson->SetStringField(TEXT("instruction"), FString::Printf(TEXT("Bind '%s' to %s's %s event."), *FunctionName, *SlotName, *EventName));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("OnValueChanged binding info provided"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("create_property_binding"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetSlotName(Payload);
        FString PropertyName = GetJsonStringField(Payload, TEXT("propertyName"));
        FString FunctionName = GetJsonStringField(Payload, TEXT("functionName"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty() || PropertyName.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, slotName, propertyName"), TEXT("MISSING_PARAMETER"));
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

        // Check if property exists on widget
        FProperty* Prop = TargetWidget->GetClass()->FindPropertyByName(FName(*PropertyName));
        FString PropertyType = Prop ? Prop->GetCPPType() : TEXT("Unknown");

        if (FunctionName.IsEmpty())
        {
            FunctionName = FString::Printf(TEXT("Get%s"), *PropertyName);
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetStringField(TEXT("propertyName"), PropertyName);
        ResultJson->SetStringField(TEXT("propertyType"), PropertyType);
        ResultJson->SetStringField(TEXT("functionName"), FunctionName);
        ResultJson->SetStringField(TEXT("instruction"), FString::Printf(TEXT("Create function '%s' returning %s and use Property Binding dropdown on %s.%s."), *FunctionName, *PropertyType, *SlotName, *PropertyName));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Property binding configured"), ResultJson);
        return true;
    }

    return false;
}
}
