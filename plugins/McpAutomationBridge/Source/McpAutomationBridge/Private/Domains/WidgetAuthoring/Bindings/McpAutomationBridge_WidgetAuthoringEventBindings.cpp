#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringPayload.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
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

        // Create a real UK2Node_ComponentBoundEvent bound to the button's OnClicked
        // multicast delegate — the exact node the Designer's "+ OnClicked" adds.
        // Track whether we actually mutate the blueprint so the reuse path stays
        // side-effect free (no dirty / recompile when nothing changed).
        bool bBlueprintChanged = false;
        if (!ButtonWidget->bIsVariable)
        {
            ButtonWidget->Modify();
            ButtonWidget->bIsVariable = true;
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
            bBlueprintChanged = true;
        }

        FMulticastDelegateProperty* DelegateProp =
            FindFProperty<FMulticastDelegateProperty>(UButton::StaticClass(), FName(TEXT("OnClicked")));
        if (!DelegateProp)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("OnClicked delegate not found on UButton"), TEXT("DELEGATE_NOT_FOUND"));
            return true;
        }

        FObjectProperty* CompProp =
            FindFProperty<FObjectProperty>(WidgetBP->SkeletonGeneratedClass, FName(*SlotName));
        if (!CompProp)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Component variable '%s' not found on widget skeleton class"), *SlotName),
                TEXT("COMPONENT_PROPERTY_NOT_FOUND"));
            return true;
        }

        UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(WidgetBP);
        if (!EventGraph)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Event graph not found on widget blueprint"), TEXT("EVENT_GRAPH_NOT_FOUND"));
            return true;
        }

        // Idempotent: reuse an existing bound event for this delegate+component.
        const UK2Node_ComponentBoundEvent* Existing =
            FKismetEditorUtilities::FindBoundEventForComponent(WidgetBP, DelegateProp->GetFName(), CompProp->GetFName());

        UK2Node_ComponentBoundEvent* BoundNode = const_cast<UK2Node_ComponentBoundEvent*>(Existing);
        bool bCreatedNew = false;
        if (!BoundNode)
        {
            EventGraph->Modify();
            FGraphNodeCreator<UK2Node_ComponentBoundEvent> Creator(*EventGraph);
            BoundNode = Creator.CreateNode(false);
            BoundNode->InitializeComponentBoundEventParams(CompProp, DelegateProp);
            Creator.Finalize();
            bCreatedNew = true;
            bBlueprintChanged = true;
        }

        // Only dirty + recompile when something actually changed — a repeat
        // bind_on_clicked that reuses the existing node leaves the asset untouched.
        bool bCompiled = true;
        if (bBlueprintChanged)
        {
            FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBP);
            bCompiled = McpSafeCompileBlueprint(WidgetBP);
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetStringField(TEXT("eventType"), TEXT("OnClicked"));
        // Response contract: functionName echoes the request input (default "OnButtonClicked").
        // The real handler is engine-generated -- returned as eventFunctionName
        // (UK2Node_ComponentBoundEvent::CustomFunctionName). Reference the handler via that.
        ResultJson->SetStringField(TEXT("functionName"), FunctionName);
        ResultJson->SetBoolField(TEXT("bound"), true);
        ResultJson->SetBoolField(TEXT("createdNew"), bCreatedNew);
        ResultJson->SetBoolField(TEXT("compileSucceeded"), bCompiled);
        ResultJson->SetStringField(TEXT("nodeId"), BoundNode->NodeGuid.ToString());
        ResultJson->SetStringField(TEXT("eventFunctionName"), BoundNode->CustomFunctionName.ToString());

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("OnClicked event bound"), ResultJson);
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
