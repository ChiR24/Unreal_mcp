#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"

#include "Blueprint/WidgetTree.h"
#include "Components/SlateWrapperTypes.h"
#include "Components/Widget.h"
#include "EdGraphSchema_K2.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintDiagnostics.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Select.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "WidgetBlueprint.h"

// bind_text, bind_visibility, bind_color, bind_enabled, bind_percent: a real UMG property binding, the
// same thing the Designer's "Bind" dropdown writes. These four were published with no
// handler at all, so every call died as UNKNOWN_ACTION.
namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

namespace
{
// The widget property each action binds; bind_color takes whichever tint the widget has.
TArray<FName> BindableProperties(const FString& SubAction)
{
    if (SubAction.Equals(TEXT("bind_text"), ESearchCase::IgnoreCase)) { return { TEXT("Text") }; }
    if (SubAction.Equals(TEXT("bind_visibility"), ESearchCase::IgnoreCase)) { return { TEXT("Visibility") }; }
    if (SubAction.Equals(TEXT("bind_enabled"), ESearchCase::IgnoreCase)) { return { TEXT("bIsEnabled") }; }
    // A Progress Bar fills by Percent; a Slider or SpinBox shows Value.
    if (SubAction.Equals(TEXT("bind_percent"), ESearchCase::IgnoreCase)) { return { TEXT("Percent"), TEXT("Value") }; }
    if (SubAction.Equals(TEXT("bind_color"), ESearchCase::IgnoreCase))
    {
        return { TEXT("ColorAndOpacity"), TEXT("FillColorAndOpacity"), TEXT("BrushColor"), TEXT("ContentColorAndOpacity") };
    }
    return {};
}

FString ListBindable(const UClass* Class)
{
    TArray<FString> Names;
    for (TFieldIterator<FDelegateProperty> It(Class); It; ++It)
    {
        FString Name = It->GetName();
        if (Name.RemoveFromEnd(TEXT("Delegate")))
        {
            Names.Add(Name);
        }
    }
    return Names.Num() > 0 ? FString::Join(Names, TEXT(", ")) : TEXT("nothing");
}

// Get_<Widget>_<Property>: a pure getter returning Source, converted to the delegate's
// return type (the K2 schema inserts the conversion node; a bool drives a Select node
// for visibility, true = Visible, false = Collapsed).
UEdGraph* BuildGetter(UWidgetBlueprint* WidgetBP, const FString& WidgetName, FName Property, UFunction* Signature,
                      FName Source, FString& OutError)
{
    const FName GraphName = FBlueprintEditorUtils::FindUniqueKismetName(
        WidgetBP, FString::Printf(TEXT("Get_%s_%s"), *WidgetName, *Property.ToString()));
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(WidgetBP, GraphName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddFunctionGraph<UFunction>(WidgetBP, Graph, true, Signature);
    TArray<UK2Node_FunctionEntry*> Entries;
    TArray<UK2Node_FunctionResult*> Results;
    Graph->GetNodesOfClass(Entries);
    Graph->GetNodesOfClass(Results);
    UEdGraphPin* ReturnPin = Results.Num() > 0 ? Results[0]->FindPin(UEdGraphSchema_K2::PN_ReturnValue) : nullptr;
    if (Entries.Num() == 0 || !ReturnPin)
    {
        OutError = TEXT("the binding signature has no return value to fill");
        FBlueprintEditorUtils::RemoveGraph(WidgetBP, Graph);
        return nullptr;
    }
    Entries[0]->AddExtraFlags(FUNC_BlueprintPure | FUNC_Const);
    FGraphNodeCreator<UK2Node_VariableGet> GetCreator(*Graph);
    UK2Node_VariableGet* Get = GetCreator.CreateNode();
    Get->VariableReference.SetSelfMember(Source);
    Get->NodePosX = Results[0]->NodePosX - 420;
    Get->NodePosY = Results[0]->NodePosY + 140;
    GetCreator.Finalize();
    const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
    UEdGraphPin* ValuePin = Get->FindPin(Source);
    bool bLinked = false;
    if (ValuePin && ValuePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean &&
        ReturnPin->PinType.PinSubCategoryObject == StaticEnum<ESlateVisibility>())
    {
        FGraphNodeCreator<UK2Node_Select> SelectCreator(*Graph);
        UK2Node_Select* Select = SelectCreator.CreateNode();
        Select->NodePosX = Results[0]->NodePosX - 200;
        Select->NodePosY = Results[0]->NodePosY + 120;
        SelectCreator.Finalize();
        bLinked = Schema->TryCreateConnection(ValuePin, Select->GetIndexPin()) &&
                  Schema->TryCreateConnection(Select->GetReturnValuePin(), ReturnPin);
        TArray<UEdGraphPin*> Options;
        Select->GetOptionPins(Options);
        bLinked = bLinked && Options.Num() == 2;
        if (bLinked)
        {
            Options[0]->DefaultValue = TEXT("Collapsed");
            Options[1]->DefaultValue = TEXT("Visible");
        }
    }
    else if (ValuePin)
    {
        bLinked = Schema->TryCreateConnection(ValuePin, ReturnPin);
    }
    if (!bLinked)
    {
        OutError = FString::Printf(TEXT("'%s' (%s) cannot be turned into the %s the binding returns"), *Source.ToString(),
            ValuePin ? *UEdGraphSchema_K2::TypeToText(ValuePin->PinType).ToString() : TEXT("unreadable"),
            *UEdGraphSchema_K2::TypeToText(ReturnPin->PinType).ToString());
        FBlueprintEditorUtils::RemoveGraph(WidgetBP, Graph);
        return nullptr;
    }
    return Graph;
}
}

bool HandleWidgetAuthoringPropertyBindings(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    const TArray<FName> Candidates = BindableProperties(SubAction);
    if (Candidates.Num() == 0)
    {
        return false;
    }
    const FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
    const FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
    const FName Source(*GetJsonStringField(Payload, TEXT("bindingSource")));
    if (WidgetPath.IsEmpty() || SlotName.IsEmpty() || Source.IsNone())
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId,
            TEXT("widgetPath, slotName and bindingSource (a variable or pure function of the Widget Blueprint) are required."),
            TEXT("MISSING_PARAMETER"));
        return true;
    }
    UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
    UWidget* Widget = WidgetBP && WidgetBP->WidgetTree ? WidgetBP->WidgetTree->FindWidget(FName(*SlotName)) : nullptr;
    if (!Widget)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("No widget '%s' in '%s'."), *SlotName, *WidgetPath),
            WidgetBP ? TEXT("WIDGET_NOT_FOUND") : TEXT("NOT_FOUND"));
        return true;
    }
    FName Property;
    FDelegateProperty* Delegate = nullptr;
    for (const FName& Candidate : Candidates)
    {
        Delegate = FindFProperty<FDelegateProperty>(Widget->GetClass(), *(Candidate.ToString() + TEXT("Delegate")));
        if (Delegate)
        {
            Property = Candidate;
            break;
        }
    }
    if (!Delegate)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(
            TEXT("'%s' is a %s, which has no bindable %s. Bindable on it: %s."), *SlotName, *Widget->GetClass()->GetName(),
            *Candidates[0].ToString(), *ListBindable(Widget->GetClass())), TEXT("NOT_BINDABLE"));
        return true;
    }
    UClass* Skeleton = WidgetBP->SkeletonGeneratedClass ? WidgetBP->SkeletonGeneratedClass : WidgetBP->GeneratedClass;
    FName FunctionName;
    FGuid MemberGuid;
    UEdGraph* Generated = nullptr;
    FString Error;
    if (UFunction* Existing = Skeleton ? Skeleton->FindFunctionByName(Source) : nullptr)
    {
        if (!Existing->IsSignatureCompatibleWith(Delegate->SignatureFunction, UFunction::GetDefaultIgnoredSignatureCompatibilityFlags() | CPF_ReturnParm) ||
            !Existing->HasAnyFunctionFlags(FUNC_Const | FUNC_BlueprintPure))
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(
                TEXT("Function '%s' cannot drive %s: a property binding needs a pure function with no inputs that returns the property's type. "
                     "Bind a variable instead and the getter is generated for you."), *Source.ToString(), *Property.ToString()),
                TEXT("SIGNATURE_MISMATCH"));
            return true;
        }
        FunctionName = Existing->GetFName();
        UBlueprint::GetGuidFromClassByFieldName<UFunction>(Skeleton, FunctionName, MemberGuid);
    }
    else if ((Skeleton && Skeleton->FindPropertyByName(Source)) || FBlueprintEditorUtils::FindNewVariableIndex(WidgetBP, Source) != INDEX_NONE)
    {
        Generated = BuildGetter(WidgetBP, Widget->GetName(), Property, Delegate->SignatureFunction, Source, Error);
        if (!Generated)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("TYPE_MISMATCH"));
            return true;
        }
        FunctionName = Generated->GetFName();
        MemberGuid = Generated->GraphGuid;
    }
    else
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(
            TEXT("'%s' has no variable or function named '%s'. Add the variable first (edit_variable add_variable), then bind it."),
            *WidgetBP->GetName(), *Source.ToString()), TEXT("SOURCE_NOT_FOUND"));
        return true;
    }
    Widget->bIsVariable = true;
    FDelegateEditorBinding Binding;
    Binding.ObjectName = Widget->GetName();
    Binding.PropertyName = Property;
    Binding.FunctionName = FunctionName;
    Binding.MemberGuid = MemberGuid;
    Binding.Kind = EBindingKind::Function;
    WidgetBP->Bindings.Remove(Binding);
    WidgetBP->Bindings.Add(Binding);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
    if (!McpCompileBlueprintWithDiagnostics(WidgetBP, ResultJson, Error))
    {
        // Leave nothing behind that keeps the Widget Blueprint from compiling.
        WidgetBP->Bindings.Remove(Binding);
        if (Generated)
        {
            FBlueprintEditorUtils::RemoveGraph(WidgetBP, Generated);
        }
        McpSafeCompileBlueprint(WidgetBP);
        Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(
            TEXT("The binding did not compile and was removed: %s"), *Error), TEXT("BINDING_INVALID"));
        return true;
    }
    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetStringField(TEXT("widgetPath"), WidgetBlueprintPackagePath(WidgetBP));
    ResultJson->SetStringField(TEXT("slotName"), SlotName);
    ResultJson->SetStringField(TEXT("property"), Property.ToString());
    ResultJson->SetStringField(TEXT("bindingSource"), Source.ToString());
    ResultJson->SetStringField(TEXT("functionName"), FunctionName.ToString());
    ResultJson->SetBoolField(TEXT("generatedGetter"), Generated != nullptr);
    ResultJson->SetBoolField(TEXT("saved"), McpSafeAssetSave(WidgetBP));
    McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(
        TEXT("Bound %s.%s to %s"), *SlotName, *Property.ToString(), *FunctionName.ToString()), ResultJson);
    return true;
}
}
