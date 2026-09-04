#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringPayload.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Components/SpinBox.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_ComponentBoundEvent.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;
// create_property_binding: split out of the event-binding file to keep it
// within the per-file line budget.
bool HandleWidgetAuthoringPropertyBindings(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject> ResultJson)
{
    if (SubAction.Equals(TEXT("create_property_binding"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetSlotName(Payload);
        FString PropertyName = GetJsonStringField(Payload, TEXT("propertyName"));
        FString FunctionName = GetJsonStringField(Payload, TEXT("functionName"));
        // Published contract: {widgetPath, slotName, bindingSource}. bindingSource
        // names the function/variable that feeds the binding; propertyName is
        // optional and defaults per widget class below.
        if (FunctionName.IsEmpty())
        {
            FunctionName = GetJsonStringField(Payload, TEXT("bindingSource"));
        }

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

        if (PropertyName.IsEmpty())
        {
            if (TargetWidget->IsA<UTextBlock>())
            {
                PropertyName = TEXT("Text");
            }
            else if (TargetWidget->IsA<UProgressBar>())
            {
                PropertyName = TEXT("Percent");
            }
            else if (TargetWidget->IsA<UImage>())
            {
                PropertyName = TEXT("Brush");
            }
            else if (TargetWidget->IsA<UCheckBox>())
            {
                PropertyName = TEXT("CheckedState");
            }
            else if (TargetWidget->IsA<USlider>())
            {
                PropertyName = TEXT("Value");
            }
            else
            {
                Subsystem.SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("propertyName is required for a %s widget (no default bindable property)"), *TargetWidget->GetClass()->GetName()),
                    TEXT("MISSING_PARAMETER"));
                return true;
            }
        }

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

        WidgetAuthoringHelpers::MarkWidgetBlueprintModifiedAndSave(WidgetBP);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Property binding configured"), ResultJson);
        return true;
    }

    return false;
}
}
