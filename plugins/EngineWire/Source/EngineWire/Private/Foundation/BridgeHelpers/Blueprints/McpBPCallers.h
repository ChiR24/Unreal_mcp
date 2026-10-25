#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Variable.h"
#include "Kismet2/BlueprintEditorUtils.h"

// The nodes that call Member of this Blueprint (on itself, or on another object of its class), outside Except, each
// as "Graph (node <guid>)". Removing a function or custom event they call leaves every one calling nothing, and the
// Blueprint stops compiling.
inline TArray<FString> McpBlueprintCallersOf(UBlueprint* Blueprint, FName Member, const UEdGraph* Except = nullptr)
{
    TArray<UK2Node_CallFunction*> Calls;
    FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, Calls);
    TArray<FString> Callers;
    for (UK2Node_CallFunction* Call : Calls)
    {
        const UClass* Owner = Call->FunctionReference.GetMemberParentClass();
        if (Call->GetGraph() != Except && Call->FunctionReference.GetMemberName() == Member &&
            (Call->FunctionReference.IsSelfContext() || (Owner && Owner->ClassGeneratedBy == Blueprint)))
        {
            Callers.Add(FString::Printf(TEXT("%s (node %s)"), *Call->GetGraph()->GetName(), *Call->NodeGuid.ToString()));
        }
    }
    return Callers;
}

// The get and set nodes of this Blueprint's own member variable Name, each as "Graph (node <guid>)": the ones
// FBlueprintEditorUtils::RemoveMemberVariable deletes with the variable (RemoveVariableNodes, self only), which
// leaves whatever they were wired to unwired.
inline TArray<FString> McpBlueprintVariableUsers(UBlueprint* Blueprint, FName Name)
{
    TArray<UK2Node_Variable*> Nodes;
    FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, Nodes);
    TArray<FString> Users;
    for (UK2Node_Variable* Node : Nodes)
    {
        UClass* Self = Blueprint->GeneratedClass;
        if (Node->GetVarName() == Name && Node->VariableReference.GetMemberParentClass(Self) == Self)
        {
            Users.Add(FString::Printf(TEXT("%s (node %s)"), *Node->GetGraph()->GetName(), *Node->NodeGuid.ToString()));
        }
    }
    return Users;
}
