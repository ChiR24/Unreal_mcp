// McpLevelStructureBPVariables.cpp — variable nodes for add_level_blueprint_node: a level Blueprint variable, or a
// property of a placed actor's class (its component) read through the actor's K2Node_Literal.
#include "Domains/LevelStructure/McpLevelStructureActions.h"

#include "Components/ActorComponent.h"
#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"

namespace McpLevelStructure
{
FString DescribeUnknownMemberClass(const FString& MemberClass)
{
    return FString::Printf(TEXT("memberClass '%s' is not a class: pass a native class name (PointLight, SceneComponent) "
                                "or a Blueprint asset path."), *MemberClass);
}

// A variable node made by class name had no member, so no pins, and the add still reported success; a component
// function (a light's SetLightColor) needs that component, which only a getter on the actor's class reads.
bool BindLevelBlueprintVariableNode(UK2Node* Node, UBlueprint* LevelBP, const FString& MemberName,
                                    const FString& MemberClass, FString& OutError, FString& OutCode)
{
    // Make and Break Struct nodes are variable nodes too, and memberName binds neither.
    const bool bSet = Node->IsA<UK2Node_VariableSet>();
    UK2Node_Variable* VariableNode = bSet || Node->IsA<UK2Node_VariableGet>() ? Cast<UK2Node_Variable>(Node) : nullptr;
    OutCode = TEXT("INVALID_ARGUMENT");
    if (!VariableNode)
    {
        OutError = TEXT("memberName applies to a K2Node_VariableGet or K2Node_VariableSet only; a function goes in functionName.");
        return MemberName.IsEmpty();
    }
    if (MemberName.IsEmpty())
    {
        OutError = FString::Printf(TEXT("%s needs memberName: a level Blueprint variable, or with memberClass a property of "
                                        "that class, such as a placed actor's component (PointLightComponent on PointLight)."),
                                   *Node->GetClass()->GetName());
        return false;
    }
    UClass* Owner = nullptr;
    bool bOwnVariable = false;
    FProperty* Property = McpBlueprintGraphHandlers::ResolveVariableMember(LevelBP, FName(*MemberName), MemberClass, Owner, bOwnVariable);
    if (Property || bOwnVariable)
    {
        OutError = McpBlueprintGraphHandlers::DescribeVariableAccess(LevelBP, Property, bSet);
        OutCode = TEXT("VARIABLE_NOT_ACCESSIBLE");
        if (!OutError.IsEmpty())
        {
            return false;
        }
        // A named class is a placed actor's, never the level script actor's own, even Actor, which the level script
        // actor also is: the node always gets the self pin the actor's reference feeds.
        if (MemberClass.IsEmpty())
        {
            McpBlueprintGraphHandlers::SetVariableReference(*VariableNode, LevelBP, Property, Owner, bOwnVariable, FName(*MemberName));
        }
        else
        {
            VariableNode->VariableReference.SetFromField<FProperty>(Property, false, Owner);
        }
        return true;
    }
    if (!MemberClass.IsEmpty() && !Owner)
    {
        OutError = DescribeUnknownMemberClass(MemberClass);
        return false;
    }
    OutCode = TEXT("VARIABLE_NOT_FOUND");
    if (!Owner)
    {
        OutError = FString::Printf(TEXT("'%s' is not a variable of the level Blueprint; for a property of another class, "
                                        "such as a placed actor's component, pass memberClass (PointLight for "
                                        "PointLightComponent)."), *MemberName);
        return false;
    }
    // The component properties memberClass has, the names a component getter takes.
    TArray<FString> Components;
    for (TFieldIterator<FObjectPropertyBase> It(Owner); It; ++It)
    {
        if (It->HasAnyPropertyFlags(CPF_BlueprintVisible) && It->PropertyClass &&
            It->PropertyClass->IsChildOf(UActorComponent::StaticClass()))
        {
            Components.Add(It->GetName());
        }
    }
    OutError = FString::Printf(TEXT("%s has no property '%s'."), *Owner->GetName(), *MemberName);
    if (Components.Num() > 0)
    {
        OutError += FString::Printf(TEXT(" Its components: %s."), *FString::Join(Components, TEXT(", ")));
    }
    return false;
}
}
