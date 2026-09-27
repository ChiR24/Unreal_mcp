// A literal for a read-only pin, fed through a UKismetSystemLibrary MakeLiteral
// node. Split from McpAutomationBridge_BlueprintGraphPinSetDefaultValue.cpp.
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "Kismet/KismetSystemLibrary.h"
#include "ScopedTransaction.h"

namespace McpBlueprintGraphHandlers
{
namespace
{
/** The MakeLiteral function whose output fits a pin of this type; NAME_None if none does. */
FName LiteralFunctionFor(const FEdGraphPinType& Type)
{
    // An enum byte is not a plain byte, and no MakeLiteral makes a struct or an array.
    if (Type.IsContainer() || (Type.PinCategory == UEdGraphSchema_K2::PC_Byte && Type.PinSubCategoryObject.IsValid()))
    {
        return NAME_None;
    }
    static const TMap<FName, FName> Functions = {
        {UEdGraphSchema_K2::PC_Text, TEXT("MakeLiteralText")}, {UEdGraphSchema_K2::PC_String, TEXT("MakeLiteralString")},
        {UEdGraphSchema_K2::PC_Name, TEXT("MakeLiteralName")}, {UEdGraphSchema_K2::PC_Boolean, TEXT("MakeLiteralBool")},
        {UEdGraphSchema_K2::PC_Int, TEXT("MakeLiteralInt")}, {UEdGraphSchema_K2::PC_Int64, TEXT("MakeLiteralInt64")},
        {UEdGraphSchema_K2::PC_Byte, TEXT("MakeLiteralByte")}, {UEdGraphSchema_K2::PC_Real, TEXT("MakeLiteralDouble")}};
    const FName* Found = Functions.Find(Type.PinCategory);
    return Found ? *Found : NAME_None;
}

/** The Value pin of a MakeLiteral node feeding only Pin; one shared with other pins is not Pin's to change. */
UEdGraphPin* FeedingLiteralValuePin(const UEdGraphPin& Pin)
{
    if (Pin.LinkedTo.Num() != 1 || !Pin.LinkedTo[0] || Pin.LinkedTo[0]->LinkedTo.Num() != 1)
    {
        return nullptr;
    }
    UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Pin.LinkedTo[0]->GetOwningNode());
    const UFunction* Function = Call ? Call->GetTargetFunction() : nullptr;
    const bool bLiteral = Function && Function->GetOwnerClass() == UKismetSystemLibrary::StaticClass() &&
                          Function->GetName().StartsWith(TEXT("MakeLiteral"));
    return bLiteral ? Call->FindPin(TEXT("Value")) : nullptr;
}

FString ReadLiteral(const UEdGraphPin& Pin)
{
    return Pin.PinType.PinCategory == UEdGraphSchema_K2::PC_Text ? Pin.DefaultTextValue.ToString() : Pin.DefaultValue;
}
}

bool FeedReadOnlyPinLiteral(FActionContext& Context, UEdGraphNode& TargetNode, UEdGraphPin& Pin, const FString& Value)
{
    const FString PinName = Pin.PinName.ToString();
    UEdGraphPin* ValuePin = FeedingLiteralValuePin(Pin);
    const FName FunctionName = LiteralFunctionFor(Pin.PinType);
    UFunction* Function = FunctionName.IsNone() ? nullptr
                                                : UKismetSystemLibrary::StaticClass()->FindFunctionByName(FunctionName);
    if (!ValuePin && (!Function || Pin.LinkedTo.Num() > 0))
    {
        Context.SendError(
            FString::Printf(
                TEXT("Pin '%s' takes no literal: it is a read-only (const reference or required) %s input%s, so a "
                     "value has to be wired into it - e.g. from a variable or a Make node (MakeVector, MakeRotator...)."),
                *PinName, *Pin.PinType.PinCategory.ToString(),
                Pin.LinkedTo.Num() > 0 ? TEXT(" that is already wired to something else") : TEXT("")),
            TEXT("PIN_REQUIRES_CONNECTION"));
        return true;
    }

    const FScopedTransaction Transaction(FText::FromString(TEXT("Set Pin Default Value")));
    Context.Blueprint->Modify();
    Context.TargetGraph->Modify();
    TargetNode.Modify();
    const UEdGraphSchema* Schema = Context.TargetGraph->GetSchema();
    UK2Node_CallFunction* Created = nullptr;
    if (ValuePin)
    {
        ValuePin->GetOwningNode()->Modify();
    }
    else
    {
        FGraphNodeCreator<UK2Node_CallFunction> Creator(*Context.TargetGraph);
        Created = Creator.CreateNode(false);
        Created->SetFromFunction(Function);
        Created->NodePosX = TargetNode.NodePosX - 300;
        Created->NodePosY = TargetNode.NodePosY + 160;
        Creator.Finalize();
        ValuePin = Created->FindPin(TEXT("Value"));
        if (ValuePin)
        {
            Schema->TryCreateConnection(Created->GetReturnValuePin(), &Pin);
        }
    }
    if (ValuePin && ValuePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Text)
    {
        Schema->TrySetDefaultText(*ValuePin, FText::FromString(Value));
    }
    else if (ValuePin)
    {
        Schema->TrySetDefaultValue(*ValuePin, Value);
    }
    const FString AppliedValue = ValuePin ? ReadLiteral(*ValuePin) : FString();
    if (!ValuePin || Pin.LinkedTo.Num() != 1 || (AppliedValue.IsEmpty() && !Value.IsEmpty()))
    {
        if (Created)
        {
            FBlueprintEditorUtils::RemoveNode(Context.Blueprint, Created, /*bDontRecompile=*/true);
        }
        Context.SendError(
            FString::Printf(TEXT("Pin '%s' is read-only, and the %s node meant to feed it did not take '%s'."),
                            *PinName, *FunctionName.ToString(), *Value),
            TEXT("PIN_VALUE_REJECTED"));
        return true;
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Context.Blueprint);
    SaveLoadedAssetThrottled(Context.Blueprint);
    UEdGraphNode* LiteralNode = ValuePin->GetOwningNode();
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), TargetNode.NodeGuid.ToString());
    Result->SetStringField(TEXT("nodeName"), TargetNode.GetName());
    Result->SetStringField(TEXT("pinName"), PinName);
    Result->SetStringField(TEXT("value"), Value);
    Result->SetStringField(TEXT("appliedValue"), AppliedValue);
    Result->SetStringField(TEXT("literalNodeId"), LiteralNode->NodeGuid.ToString());
    McpHandlerUtils::AddVerification(Result, Context.Blueprint);
    Context.SendResponse(
        FString::Printf(TEXT("Pin '%s' is read-only (a const reference), so the value went into the %s node %s it."),
                        *PinName, *LiteralNode->GetNodeTitle(ENodeTitleType::ListView).ToString(),
                        Created ? TEXT("now wired into") : TEXT("already feeding")),
        Result);
    return true;
}

void RemoveNodeWithLiterals(UBlueprint* Blueprint, UEdGraphNode* Node)
{
    if (!Blueprint || !Node)
    {
        return;
    }
    TArray<UEdGraphNode*> Doomed = {Node};
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->Direction == EGPD_Input && FeedingLiteralValuePin(*Pin))
        {
            Doomed.Add(Pin->LinkedTo[0]->GetOwningNode());
        }
    }
    for (UEdGraphNode* Each : Doomed)
    {
        FBlueprintEditorUtils::RemoveNode(Blueprint, Each, /*bDontRecompile=*/true);
    }
}
}
