#include "McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#if WITH_EDITOR
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"

namespace McpBlueprintGraphHandlers
{
bool TryCreateVariableNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y)
{
    const bool bIsGet =
        NodeType == TEXT("VariableGet") ||
        NodeType == TEXT("K2Node_VariableGet");
    const bool bIsSet =
        NodeType == TEXT("VariableSet") ||
        NodeType == TEXT("K2Node_VariableSet");
    if (!bIsGet && !bIsSet)
    {
        return false;
    }

    FString VariableName;
    Context.Payload->TryGetStringField(
        TEXT("variableName"),
        VariableName);
    const FName VariableFName(*VariableName);

    FString MemberClassName;
    Context.Payload->TryGetStringField(
        TEXT("memberClass"),
        MemberClassName);

    FProperty* FoundProperty = nullptr;
    UClass* ResolvedOwnerClass = nullptr;
    bool bFoundAsBlueprintVariable = false;

    if (!MemberClassName.IsEmpty())
    {
        if (UClass* OwnerClass = ResolveUClass(MemberClassName))
        {
            FoundProperty =
                McpFindPropertyRecursive(OwnerClass, VariableFName);
            if (FoundProperty)
            {
                ResolvedOwnerClass = OwnerClass;
            }
        }
    }
    else
    {
        for (const FBPVariableDescription& Variable :
             Context.Blueprint->NewVariables)
        {
            if (Variable.VarName == VariableFName)
            {
                bFoundAsBlueprintVariable = true;
                ResolvedOwnerClass = Context.Blueprint->GeneratedClass;
                break;
            }
        }
        if (!bFoundAsBlueprintVariable &&
            Context.Blueprint->GeneratedClass)
        {
            FoundProperty = McpFindPropertyRecursive(
                Context.Blueprint->GeneratedClass,
                VariableFName);
            if (FoundProperty)
            {
                ResolvedOwnerClass = FoundProperty->GetOwnerClass();
            }
        }
    }

    if (!FoundProperty && !bFoundAsBlueprintVariable)
    {
        Context.SendError(
            FString::Printf(
                TEXT("Variable '%s' not found in Blueprint or any parent class (memberClass='%s')"),
                *VariableName,
                *MemberClassName),
            TEXT("VARIABLE_NOT_FOUND"));
        return true;
    }

    const bool bIsSelfContext =
        bFoundAsBlueprintVariable ||
        (Context.Blueprint->GeneratedClass &&
         Context.Blueprint->GeneratedClass->IsChildOf(
             ResolvedOwnerClass));

    if (bIsSet)
    {
        FGraphNodeCreator<UK2Node_VariableSet> NodeCreator(
            *Context.TargetGraph);
        UK2Node_VariableSet* Node = NodeCreator.CreateNode(false);
        if (bIsSelfContext)
        {
            Node->VariableReference.SetSelfMember(VariableFName);
        }
        else
        {
            Node->VariableReference.SetFromField<FProperty>(
                FoundProperty,
                false,
                ResolvedOwnerClass);
        }
        Context.FinalizeNode(NodeCreator, Node, X, Y);
        return true;
    }

    FGraphNodeCreator<UK2Node_VariableGet> NodeCreator(
        *Context.TargetGraph);
    UK2Node_VariableGet* Node = NodeCreator.CreateNode(false);
    if (bIsSelfContext)
    {
        Node->VariableReference.SetSelfMember(VariableFName);
    }
    else
    {
        Node->VariableReference.SetFromField<FProperty>(
            FoundProperty,
            false,
            ResolvedOwnerClass);
    }
    Context.FinalizeNode(NodeCreator, Node, X, Y);
    return true;
}
}
#endif
