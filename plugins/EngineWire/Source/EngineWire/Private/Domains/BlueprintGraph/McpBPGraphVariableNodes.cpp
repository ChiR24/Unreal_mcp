#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"

#include "Components/Widget.h"
#include "Editor.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"

namespace McpBlueprintGraphHandlers
{
// edit_graph's own contract offers GetVariable, and add_node took it, while
// create_node and build_graph answered NODE_TYPE_NOT_FOUND for it.
bool ParseVariableNodeType(const FString& NodeType, bool& bOutIsSet)
{
    FString Type = NodeType;
    Type.RemoveFromStart(TEXT("K2Node_"), ESearchCase::IgnoreCase);
    bOutIsSet = Type.Equals(TEXT("VariableSet"), ESearchCase::IgnoreCase) ||
                Type.Equals(TEXT("SetVariable"), ESearchCase::IgnoreCase);
    return bOutIsSet || Type.Equals(TEXT("VariableGet"), ESearchCase::IgnoreCase) ||
           Type.Equals(TEXT("GetVariable"), ESearchCase::IgnoreCase);
}

FProperty* ResolveVariableMember(UBlueprint* Blueprint, FName VariableName, const FString& MemberClassName,
                                 UClass*& OutOwner, bool& bOutOwnVariable)
{
    OutOwner = nullptr;
    bOutOwnVariable = false;
    if (!MemberClassName.IsEmpty())
    {
        // A Blueprint asset path (/Game/UI/WBP_Pop) names the Blueprint, not its class.
        OutOwner = ResolveUClass(MemberClassName);
        OutOwner = OutOwner ? OutOwner : ResolveTargetClassFromString(MemberClassName);
        return OutOwner ? OutOwner->FindPropertyByName(VariableName) : nullptr;
    }
    for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
    {
        if (Variable.VarName == VariableName)
        {
            bOutOwnVariable = true;
            OutOwner = Blueprint->GeneratedClass;
            return nullptr;
        }
    }
    FProperty* Property = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->FindPropertyByName(VariableName) : nullptr;
    OutOwner = Property ? Property->GetOwnerClass() : nullptr;
    return Property;
}

void SetVariableReference(UK2Node_Variable& Node, UBlueprint* Blueprint, FProperty* Property, UClass* Owner,
                          bool bOwnVariable, FName VariableName)
{
    if (bOwnVariable || (Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(Owner)))
    {
        Node.VariableReference.SetSelfMember(VariableName);
    }
    else
    {
        Node.VariableReference.SetFromField<FProperty>(Property, false, Owner);
    }
}

bool TryCreateVariableNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y)
{
    bool bIsSet = false;
    if (!ParseVariableNodeType(NodeType, bIsSet))
    {
        return false;
    }

    // blueprint.create_node publishes the variable's name as `memberName` and
    // closes the schema (additionalProperties:false), so `variableName` cannot
    // reach this handler through the gateway. Reading only `variableName` meant
    // the name always arrived empty and every VariableGet/VariableSet failed
    // with VARIABLE_NOT_FOUND for a variable that exists — while the sibling
    // `memberClass` resolved correctly, which is what made the bug look like a
    // lookup failure rather than a dropped parameter. Consequence: no Blueprint
    // graph could read or write a variable at all. `variableName` is retained as
    // the legacy WebSocket spelling.
    FString VariableName;
    if (!Context.Payload->TryGetStringField(TEXT("memberName"), VariableName) ||
        VariableName.IsEmpty())
    {
        Context.Payload->TryGetStringField(
            TEXT("variableName"),
            VariableName);
    }
    const FName VariableFName(*VariableName);

    FString MemberClassName;
    Context.Payload->TryGetStringField(
        TEXT("memberClass"),
        MemberClassName);

    UClass* ResolvedOwnerClass = nullptr;
    bool bFoundAsBlueprintVariable = false;
    FProperty* FoundProperty = ResolveVariableMember(Context.Blueprint, VariableFName, MemberClassName,
                                                     ResolvedOwnerClass, bFoundAsBlueprintVariable);

    // A widget in a Widget Blueprint's tree is a graph variable once it is flagged "Is Variable" AND the
    // Blueprint has been compiled since: the generated class only gains its property then. The widget
    // handlers flag every widget they add, rename or copy, but a widget flagged some other way may still be
    // uncompiled, so compile once and look again, unless Play-In-Editor runs (a compile then reinstances the
    // live widget).
    UObject* WidgetTree = FindObject<UObject>(Context.Blueprint, TEXT("WidgetTree"));
    const UWidget* TreeWidget = WidgetTree && MemberClassName.IsEmpty()
        ? Cast<UWidget>(FindObject<UObject>(WidgetTree, *VariableName)) : nullptr;
    bool bCompiledHere = false;
    if (!FoundProperty && !bFoundAsBlueprintVariable && TreeWidget && TreeWidget->bIsVariable &&
        !(GEditor && GEditor->PlayWorld))
    {
        McpSafeCompileBlueprint(Context.Blueprint);
        bCompiledHere = true;
        FoundProperty = Context.Blueprint->GeneratedClass
            ? Context.Blueprint->GeneratedClass->FindPropertyByName(VariableFName) : nullptr;
        if (FoundProperty)
        {
            ResolvedOwnerClass = FoundProperty->GetOwnerClass();
        }
    }

    if (!FoundProperty && !bFoundAsBlueprintVariable)
    {
        // The bare not-found left callers hunting for a widget that plainly exists, so name which of the two
        // reasons it is: the flag is false, or the flag is set and the generated class is stale.
        FString Reason;
        if (TreeWidget && TreeWidget->bIsVariable)
        {
            Reason = FString::Printf(
                TEXT("'%s' is marked as a variable in this Widget Blueprint's tree, but its generated class has no property for it %s"),
                *VariableName,
                bCompiledHere
                    ? TEXT("even after a compile: manage_blueprint compile reports what stops it.")
                    : TEXT("because the Widget Blueprint has not been compiled since the widget was added or renamed, and Play-In-Editor is running ")
                      TEXT("(a compile would reinstance the live widget). Stop play, compile the Widget Blueprint (manage_blueprint compile), then retry."));
        }
        else if (TreeWidget)
        {
            Reason = FString::Printf(
                TEXT("'%s' is a widget in this Widget Blueprint's tree but is not marked as a variable. Set "
                     "bIsVariable true on %s:WidgetTree.%s (inspect set_property), compile the Widget "
                     "Blueprint, then retry."),
                *VariableName, *Context.Blueprint->GetPathName(), *VariableName);
        }
        else
        {
            Reason = FString::Printf(
                TEXT("Variable '%s' not found in Blueprint or any parent class (memberClass='%s')"),
                *VariableName, *MemberClassName);
        }
        Context.SendError(Reason, TEXT("VARIABLE_NOT_FOUND"));
        return true;
    }

    if (bIsSet)
    {
        FGraphNodeCreator<UK2Node_VariableSet> NodeCreator(
            *Context.TargetGraph);
        UK2Node_VariableSet* Node = NodeCreator.CreateNode(false);
        SetVariableReference(*Node, Context.Blueprint, FoundProperty, ResolvedOwnerClass,
                             bFoundAsBlueprintVariable, VariableFName);
        Context.FinalizeNode(NodeCreator, Node, X, Y);
        return true;
    }

    FGraphNodeCreator<UK2Node_VariableGet> NodeCreator(
        *Context.TargetGraph);
    UK2Node_VariableGet* Node = NodeCreator.CreateNode(false);
    SetVariableReference(*Node, Context.Blueprint, FoundProperty, ResolvedOwnerClass,
                         bFoundAsBlueprintVariable, VariableFName);
    Context.FinalizeNode(NodeCreator, Node, X, Y);
    return true;
}
}
