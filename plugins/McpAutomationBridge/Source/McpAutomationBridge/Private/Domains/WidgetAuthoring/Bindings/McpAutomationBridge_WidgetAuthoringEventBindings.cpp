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

// Create a real UK2Node_ComponentBoundEvent for an arbitrary multicast delegate on a
// widget component — the same path the Designer's "+ <Event>" button uses. Shared by
// bind_on_clicked / bind_on_hovered / bind_on_value_changed so all three author genuine
// nodes (nodeId + compileSucceeded) through one code path. Idempotent: a repeat call
// reuses the existing bound event and leaves the asset untouched (no dirty / recompile).
// Response contract: functionName echoes the request input; the real handler is
// engine-generated and returned as eventFunctionName (CustomFunctionName).
// Sends the response itself and always returns true (request handled).
static bool BindComponentDelegateEvent(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson,
    UWidgetBlueprint* WidgetBP,
    UWidget* TargetWidget,
    const FString& SlotName,
    UClass* DelegateOwnerClass,
    const FName DelegateName,
    const FString& EventTypeLabel,
    const FString& FunctionName)
{
    bool bBlueprintChanged = false;
    if (!TargetWidget->bIsVariable)
    {
        TargetWidget->Modify();
        TargetWidget->bIsVariable = true;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        bBlueprintChanged = true;
    }

    FMulticastDelegateProperty* DelegateProp =
        FindFProperty<FMulticastDelegateProperty>(DelegateOwnerClass, DelegateName);
    if (!DelegateProp)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("%s delegate not found on %s"), *DelegateName.ToString(), *DelegateOwnerClass->GetName()),
            TEXT("DELEGATE_NOT_FOUND"));
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
        // Adding a bound-event node is a structural change (new function entry on the class) — same call the
        // Designer's FKismetEditorUtilities::CreateNewBoundEventForComponent path makes after node creation.
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        bCreatedNew = true;
        bBlueprintChanged = true;
    }

    bool bCompiled = true;
    if (bBlueprintChanged)
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBP);
        bCompiled = McpSafeCompileBlueprint(WidgetBP);
    }

    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetStringField(TEXT("slotName"), SlotName);
    ResultJson->SetStringField(TEXT("eventType"), EventTypeLabel);
    ResultJson->SetStringField(TEXT("functionName"), FunctionName);
    ResultJson->SetBoolField(TEXT("bound"), true);
    ResultJson->SetBoolField(TEXT("createdNew"), bCreatedNew);
    ResultJson->SetBoolField(TEXT("compileSucceeded"), bCompiled);
    ResultJson->SetStringField(TEXT("nodeId"), BoundNode->NodeGuid.ToString());
    ResultJson->SetStringField(TEXT("eventFunctionName"), BoundNode->CustomFunctionName.ToString());

    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("%s event bound"), *EventTypeLabel), ResultJson);
    return true;
}

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

        return BindComponentDelegateEvent(Subsystem, RequestId, RequestingSocket, ResultJson,
            WidgetBP, ButtonWidget, SlotName, UButton::StaticClass(), FName(TEXT("OnClicked")),
            TEXT("OnClicked"), FunctionName);
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

        return BindComponentDelegateEvent(Subsystem, RequestId, RequestingSocket, ResultJson,
            WidgetBP, ButtonWidget, SlotName, UButton::StaticClass(), FName(TEXT("OnHovered")),
            TEXT("OnHovered"), FunctionName);
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

        // Resolve the value-changed multicast delegate per widget type.
        const FString WidgetType = TargetWidget->GetClass()->GetName();
        FName DelegateName = NAME_None;
        if (Cast<USlider>(TargetWidget) || Cast<USpinBox>(TargetWidget)) DelegateName = FName(TEXT("OnValueChanged"));
        else if (Cast<UCheckBox>(TargetWidget)) DelegateName = FName(TEXT("OnCheckStateChanged"));
        else if (Cast<UComboBoxString>(TargetWidget)) DelegateName = FName(TEXT("OnSelectionChanged"));

        if (DelegateName.IsNone())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Widget '%s' (%s) has no supported value-changed delegate. Supported: Slider, SpinBox, CheckBox, ComboBoxString."), *SlotName, *WidgetType),
                TEXT("UNSUPPORTED_WIDGET"));
            return true;
        }

        ResultJson->SetStringField(TEXT("widgetType"), WidgetType);
        return BindComponentDelegateEvent(Subsystem, RequestId, RequestingSocket, ResultJson,
            WidgetBP, TargetWidget, SlotName, TargetWidget->GetClass(), DelegateName,
            DelegateName.ToString(), FunctionName);
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
