#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Components/SpinBox.h"
#include "Components/Widget.h"
#include "EdGraphSchema_K2.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ComponentBoundEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "WidgetBlueprint.h"

// bind_on_clicked, bind_on_hovered, bind_on_value_changed: the Designer's "+ OnClicked"
// bound-event node, wired to call the Blueprint function the caller names. The function
// is created with the delegate's inputs (a slider's Value, ...) when it does not exist,
// so the event's payload arrives in it. Repeat calls reuse the node and the call.
namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

namespace
{
struct FEventBinding
{
    FName Delegate;
    FName Handler;
};

UFunction* ResolveHandler(UWidgetBlueprint* WidgetBP, FName Name, UFunction* Signature, bool& bOutCreated, FString& OutError)
{
    UClass* Skeleton = WidgetBP->SkeletonGeneratedClass;
    if (UFunction* Existing = Skeleton ? Skeleton->FindFunctionByName(Name) : nullptr)
    {
        return Existing;
    }
    if (FKismetNameValidator(WidgetBP).IsValid(Name) != EValidatorResult::Ok)
    {
        OutError = FString::Printf(TEXT("'%s' is not free for a new function in '%s' (a variable, widget or other member uses it)"),
                                   *Name.ToString(), *WidgetBP->GetName());
        return nullptr;
    }
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(WidgetBP, Name, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddFunctionGraph<UFunction>(WidgetBP, Graph, true, Signature);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
    bOutCreated = true;
    return WidgetBP->SkeletonGeneratedClass ? WidgetBP->SkeletonGeneratedClass->FindFunctionByName(Name) : nullptr;
}

// The bound event node for Widget.Delegate, created when missing, then a call to the
// handler hung off its exec output with the event's outputs fed to same-named inputs.
FString BindAndWire(UWidgetBlueprint* WidgetBP, UWidget* Widget, const FEventBinding& Binding, const TSharedPtr<FJsonObject>& Out)
{
    FMulticastDelegateProperty* Delegate = FindFProperty<FMulticastDelegateProperty>(Widget->GetClass(), Binding.Delegate);
    FObjectProperty* WidgetProp = FindFProperty<FObjectProperty>(WidgetBP->SkeletonGeneratedClass, Widget->GetFName());
    UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(WidgetBP);
    if (!Delegate || !WidgetProp || !EventGraph)
    {
        return FString::Printf(TEXT("'%s' exposes no %s event to bind"), *Widget->GetName(), *Binding.Delegate.ToString());
    }
    bool bCreatedFunction = false;
    FString Error;
    UFunction* Handler = ResolveHandler(WidgetBP, Binding.Handler, Delegate->SignatureFunction, bCreatedFunction, Error);
    if (!Handler)
    {
        return Error.IsEmpty() ? FString::Printf(TEXT("could not create function '%s'"), *Binding.Handler.ToString()) : Error;
    }
    UK2Node_ComponentBoundEvent* Event = const_cast<UK2Node_ComponentBoundEvent*>(
        FKismetEditorUtilities::FindBoundEventForComponent(WidgetBP, Delegate->GetFName(), WidgetProp->GetFName()));
    const bool bCreatedEvent = Event == nullptr;
    if (!Event)
    {
        FGraphNodeCreator<UK2Node_ComponentBoundEvent> Creator(*EventGraph);
        Event = Creator.CreateNode(false);
        Event->InitializeComponentBoundEventParams(WidgetProp, Delegate);
        Creator.Finalize();
    }
    const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
    UEdGraphPin* Then = Event->FindPin(UEdGraphSchema_K2::PN_Then);
    UK2Node_CallFunction* Call = nullptr;
    for (UEdGraphPin* Linked : Then ? Then->LinkedTo : TArray<UEdGraphPin*>())
    {
        UK2Node_CallFunction* Existing = Cast<UK2Node_CallFunction>(Linked->GetOwningNode());
        Call = Existing && Existing->GetFunctionName() == Handler->GetFName() ? Existing : Call;
    }
    if (!Call && Then && Then->LinkedTo.Num() > 0)
    {
        return FString::Printf(TEXT("%s of '%s' already runs other nodes; add the call to '%s' with edit_graph"),
                               *Binding.Delegate.ToString(), *Widget->GetName(), *Handler->GetName());
    }
    if (!Call && Then)
    {
        FGraphNodeCreator<UK2Node_CallFunction> Creator(*EventGraph);
        Call = Creator.CreateNode(false);
        Call->SetFromFunction(Handler);
        Call->NodePosX = Event->NodePosX + 360;
        Call->NodePosY = Event->NodePosY;
        Creator.Finalize();
        Schema->TryCreateConnection(Then, Call->GetExecPin());
        for (UEdGraphPin* Output : Event->Pins)
        {
            UEdGraphPin* Input = Output->Direction == EGPD_Output && Output->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec
                ? Call->FindPin(Output->PinName, EGPD_Input) : nullptr;
            if (Input)
            {
                Schema->TryCreateConnection(Output, Input);
            }
        }
    }
    Out->SetStringField(TEXT("event"), Binding.Delegate.ToString());
    Out->SetStringField(TEXT("functionName"), Handler->GetName());
    Out->SetBoolField(TEXT("createdFunction"), bCreatedFunction);
    Out->SetBoolField(TEXT("createdEvent"), bCreatedEvent);
    Out->SetStringField(TEXT("nodeId"), Event->NodeGuid.ToString());
    Out->SetStringField(TEXT("callNodeId"), Call ? Call->NodeGuid.ToString() : FString());
    return FString();
}

FName ValueChangedDelegate(const UWidget* Widget)
{
    if (Widget->IsA<USlider>() || Widget->IsA<USpinBox>()) { return TEXT("OnValueChanged"); }
    if (Widget->IsA<UCheckBox>()) { return TEXT("OnCheckStateChanged"); }
    if (Widget->IsA<UComboBoxString>()) { return TEXT("OnSelectionChanged"); }
    return NAME_None;
}
}

bool HandleWidgetAuthoringEventBindings(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    const bool bClicked = SubAction.Equals(TEXT("bind_on_clicked"), ESearchCase::IgnoreCase);
    const bool bHovered = SubAction.Equals(TEXT("bind_on_hovered"), ESearchCase::IgnoreCase);
    if (!bClicked && !bHovered && !SubAction.Equals(TEXT("bind_on_value_changed"), ESearchCase::IgnoreCase))
    {
        return false;
    }
    const FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
    const FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
    const FString Handler = GetJsonStringField(Payload, bHovered ? TEXT("onHoveredFunction") : TEXT("bindingSource"));
    if (WidgetPath.IsEmpty() || SlotName.IsEmpty() || Handler.IsEmpty())
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, bHovered
            ? TEXT("widgetPath, slotName and onHoveredFunction (the function the hover calls) are required.")
            : TEXT("widgetPath, slotName and bindingSource (the function the event calls) are required."), TEXT("MISSING_PARAMETER"));
        return true;
    }
    UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
    UWidget* Widget = WidgetBP && WidgetBP->WidgetTree ? WidgetBP->WidgetTree->FindWidget(FName(*SlotName)) : nullptr;
    TArray<FEventBinding> Bindings;
    if (Widget && (bClicked || bHovered) && Widget->IsA<UButton>())
    {
        Bindings.Add({ bClicked ? FName(TEXT("OnClicked")) : FName(TEXT("OnHovered")), FName(*Handler) });
        const FString Unhover = GetJsonStringField(Payload, TEXT("onUnhoveredFunction"));
        if (bHovered && !Unhover.IsEmpty())
        {
            Bindings.Add({ TEXT("OnUnhovered"), FName(*Unhover) });
        }
    }
    else if (Widget && !bClicked && !bHovered && !ValueChangedDelegate(Widget).IsNone())
    {
        Bindings.Add({ ValueChangedDelegate(Widget), FName(*Handler) });
    }
    if (Bindings.Num() == 0)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, Widget
            ? FString::Printf(TEXT("'%s' is a %s; %s needs %s."), *SlotName, *Widget->GetClass()->GetName(), *SubAction,
                              bClicked || bHovered ? TEXT("a Button") : TEXT("a Slider, SpinBox, CheckBox or ComboBoxString"))
            : FString::Printf(TEXT("No widget '%s' in '%s'."), *SlotName, *WidgetPath), Widget ? TEXT("UNSUPPORTED_WIDGET") : TEXT("WIDGET_NOT_FOUND"));
        return true;
    }
    if (!Widget->bIsVariable)
    {
        Widget->Modify();
        Widget->bIsVariable = true;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
    }
    TArray<TSharedPtr<FJsonValue>> Results;
    for (const FEventBinding& Binding : Bindings)
    {
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        const FString Error = BindAndWire(WidgetBP, Widget, Binding, Entry);
        if (!Error.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("BIND_FAILED"));
            return true;
        }
        Results.Add(MakeShared<FJsonValueObject>(Entry));
    }
    FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBP);
    const bool bCompiled = McpSafeCompileBlueprint(WidgetBP);
    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetStringField(TEXT("slotName"), SlotName);
    ResultJson->SetStringField(TEXT("functionName"), Handler);
    ResultJson->SetArrayField(TEXT("bindings"), Results);
    ResultJson->SetBoolField(TEXT("compileSucceeded"), bCompiled);
    ResultJson->SetBoolField(TEXT("saved"), McpSafeAssetSave(WidgetBP));
    McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("%s of '%s' now calls %s"), *Bindings[0].Delegate.ToString(), *SlotName, *Handler), ResultJson);
    return true;
}
}
