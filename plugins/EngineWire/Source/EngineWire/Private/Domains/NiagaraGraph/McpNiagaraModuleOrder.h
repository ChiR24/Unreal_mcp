#pragma once

#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraScript.h"

// The order of a stack's modules, and where a new module goes in it. The engine's GetOrderedModuleNodes is not
// exported, so the stack is walked here.
namespace McpNiagaraModuleOrder
{
// The input pin of a stack node that carries the parameter map.
inline UEdGraphPin* MapInputPin(UEdGraphNode& Node)
{
    for (UEdGraphPin* Pin : Node.Pins)
    {
        const UObject* Type = Pin && Pin->Direction == EGPD_Input ? Pin->PinType.PinSubCategoryObject.Get() : nullptr;
        if (Type && Type->GetFName() == TEXT("NiagaraParameterMap"))
        {
            return Pin;
        }
    }
    return nullptr;
}

// Every module of the stack an output node ends, first to last: walked back along the parameter map from the output,
// through each module and the override node in front of it, to the stack's input node.
inline TArray<UNiagaraNodeFunctionCall*> StackModules(UNiagaraNodeOutput& Output)
{
    TArray<UNiagaraNodeFunctionCall*> Modules;
    UEdGraphNode* Node = &Output;
    for (int32 Steps = 0; Node && Steps < 4096; ++Steps)
    {
        UEdGraphPin* MapIn = MapInputPin(*Node);
        Node = MapIn && MapIn->LinkedTo.Num() > 0 ? MapIn->LinkedTo[0]->GetOwningNode() : nullptr;
        if (UNiagaraNodeFunctionCall* Module = Cast<UNiagaraNodeFunctionCall>(Node))
        {
            Modules.Insert(Module, 0);
        }
    }
    return Modules;
}

// Where a module goes so its dependencies hold: before the first module of the stack that provides one of its
// post-dependencies (a force before Solve Forces and Velocity), else INDEX_NONE, the end. Appended, a force landed
// after the solver and the stack failed "The module has unmet dependencies".
inline int32 DependencyInsertIndex(UNiagaraScript& Script, UNiagaraNodeOutput& Output)
{
    const FVersionedNiagaraScriptData* Data = Script.GetLatestScriptData();
    const TArray<UNiagaraNodeFunctionCall*> Modules = StackModules(Output);
    for (int32 Index = 0; Data && Index < Modules.Num(); ++Index)
    {
        UNiagaraScript* Other = Modules[Index]->FunctionScript;
        const FVersionedNiagaraScriptData* Provider = Other ? Other->GetScriptData(Modules[Index]->SelectedScriptVersion) : nullptr;
        for (const FNiagaraModuleDependency& Dependency : Data->RequiredDependencies)
        {
            if (Provider && Dependency.Type == ENiagaraModuleDependencyType::PostDependency &&
                Provider->ProvidedDependencies.Contains(Dependency.Id))
            {
                return Index;
            }
        }
    }
    return INDEX_NONE;
}
}
