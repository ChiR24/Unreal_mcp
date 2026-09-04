#include "Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

#if WITH_EDITOR && MCP_HAS_NIAGARA_STACK_GRAPH_UTILITIES
namespace McpNiagaraAuthoringHandlers
{
namespace
{
FString BareInputName(const FString& InputName)
{
    int32 DotIndex = INDEX_NONE;
    return InputName.FindLastChar(TEXT('.'), DotIndex) ? InputName.Mid(DotIndex + 1) : InputName;
}

// Module inputs are stored as "Module.<Name>"; callers may pass the bare name, that form,
// or the stack's aliased "<ModuleName>.<Name>" form.
bool ModuleExposesInput(UNiagaraNodeFunctionCall* Module, const FString& InputName)
{
    UNiagaraGraph* CalledGraph = Module ? Module->GetCalledGraph() : nullptr;
    if (!CalledGraph)
    {
        return false;
    }
    const FString Bare = BareInputName(InputName);
    for (UEdGraphNode* Node : CalledGraph->Nodes)
    {
        UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(Node);
        if (!InputNode)
        {
            continue;
        }
        const FString Candidate = InputNode->Input.GetName().ToString();
        if (Candidate.Equals(InputName, ESearchCase::IgnoreCase) ||
            Candidate.Equals(Bare, ESearchCase::IgnoreCase) ||
            Candidate.EndsWith(TEXT(".") + Bare, ESearchCase::IgnoreCase))
        {
            return true;
        }
    }
    return false;
}
}

// targetNodeId is optional (the contract only requires systemPath + inputName): without it
// the module is resolved by moduleName / targetModuleName, the "Module.Input" prefix of
// inputName, or - when exactly one module exposes the input - by the input alone (dogfood #105).
UNiagaraNodeFunctionCall* ResolveDynamicInputTargetNode(
    FActionContext& Context,
    UNiagaraGraph* Graph,
    const FString& TargetNodeId,
    const FString& InputName,
    FString& OutError,
    FString& OutErrorCode)
{
    TArray<UNiagaraNodeFunctionCall*> Modules;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UNiagaraNodeFunctionCall* FuncCall = Cast<UNiagaraNodeFunctionCall>(Node);
        if (FuncCall && FuncCall->GetCalledUsage() == ENiagaraScriptUsage::Module)
        {
            Modules.Add(FuncCall);
        }
    }

    if (!TargetNodeId.IsEmpty())
    {
        FGuid TargetGuid;
        if (!FGuid::Parse(TargetNodeId, TargetGuid))
        {
            OutError = TEXT("Invalid 'targetNodeId' GUID format.");
            OutErrorCode = TEXT("INVALID_ARGUMENT");
            return nullptr;
        }
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            UNiagaraNodeFunctionCall* FuncCall = Cast<UNiagaraNodeFunctionCall>(Node);
            if (FuncCall && FuncCall->NodeGuid == TargetGuid)
            {
                return FuncCall;
            }
        }
        OutError = FString::Printf(TEXT("Target node '%s' not found."), *TargetNodeId);
        OutErrorCode = TEXT("NODE_NOT_FOUND");
        return nullptr;
    }

    FString ModuleName = GetJsonStringField(Context.Payload, TEXT("moduleName"));
    if (ModuleName.IsEmpty())
    {
        ModuleName = GetJsonStringField(Context.Payload, TEXT("targetModuleName"));
    }
    int32 DotIndex = INDEX_NONE;
    if (ModuleName.IsEmpty() && InputName.FindChar(TEXT('.'), DotIndex))
    {
        const FString Prefix = InputName.Left(DotIndex);
        if (!Prefix.Equals(TEXT("Module"), ESearchCase::IgnoreCase))
        {
            ModuleName = Prefix;
        }
    }

    TArray<UNiagaraNodeFunctionCall*> Candidates;
    FString Available;
    FString NamedModuleInputs; // inputs exposed by modules whose name matched (diagnostic for a wrong inputName)
    for (UNiagaraNodeFunctionCall* Module : Modules)
    {
        const FString FunctionName = Module->GetFunctionName();
        const FString ScriptName = Module->FunctionScript ? Module->FunctionScript->GetName() : FString();
        if (!Available.IsEmpty())
        {
            Available += TEXT(", ");
        }
        Available += FString::Printf(TEXT("%s (%s)"), *FunctionName, *Module->NodeGuid.ToString());
        const bool bNameMatches = ModuleName.IsEmpty() ||
            FunctionName.Equals(ModuleName, ESearchCase::IgnoreCase) ||
            ScriptName.Equals(ModuleName, ESearchCase::IgnoreCase);
        // A module whose function graph is not loaded cannot list its inputs; trust an explicit module name.
        const bool bGraphUnavailable = !ModuleName.IsEmpty() && Module->GetCalledGraph() == nullptr;
        if (bNameMatches && !ModuleName.IsEmpty())
        {
            if (UNiagaraGraph* Called = Module->GetCalledGraph())
            {
                for (UEdGraphNode* Node : Called->Nodes)
                {
                    if (UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(Node))
                    {
                        NamedModuleInputs += (NamedModuleInputs.IsEmpty() ? TEXT("") : TEXT(", ")) + InputNode->Input.GetName().ToString();
                    }
                }
            }
        }
        if (bNameMatches && (ModuleExposesInput(Module, InputName) || bGraphUnavailable))
        {
            Candidates.Add(Module);
        }
    }
    if (Candidates.Num() == 1)
    {
        Context.Result->SetStringField(
            TEXT("targetNodeResolvedBy"), ModuleName.IsEmpty() ? TEXT("inputName") : TEXT("moduleName"));
        return Candidates[0];
    }

    const FString Problem = Candidates.Num() == 0
        ? FString::Printf(TEXT("No module%s exposes input '%s'"),
            ModuleName.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" named '%s'"), *ModuleName), *InputName)
        : FString::Printf(TEXT("%d modules expose input '%s'"), Candidates.Num(), *InputName);
    OutError = FString::Printf(
        TEXT("%s. Accepted: targetNodeId (module node GUID), or moduleName plus inputName ('ModuleName.Input' in inputName also works). Modules on this emitter: %s"),
        *Problem, Available.IsEmpty() ? TEXT("<none>") : *Available);
    if (!NamedModuleInputs.IsEmpty())
    {
        OutError += FString::Printf(TEXT(" Inputs exposed by '%s': %s"), *ModuleName, *NamedModuleInputs);
    }
    OutErrorCode = TEXT("INVALID_ARGUMENT");
    return nullptr;
}
}
#endif
