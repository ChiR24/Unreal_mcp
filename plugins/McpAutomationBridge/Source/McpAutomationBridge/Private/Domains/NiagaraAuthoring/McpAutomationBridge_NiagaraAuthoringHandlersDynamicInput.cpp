#include "Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

#if WITH_EDITOR
namespace McpNiagaraAuthoringHandlers
{
#if MCP_HAS_NIAGARA_STACK_GRAPH_UTILITIES
static bool SetNiagaraDynamicInput(FActionContext& Context)
{
    const FString TargetNodeId = GetJsonStringField(Context.Payload, TEXT("targetNodeId"));
    const FString InputName = GetJsonStringField(Context.Payload, TEXT("inputName"));
    const FString DynamicInputScriptPath = GetJsonStringField(Context.Payload, TEXT("dynamicInputScriptPath"));
    bool bReplaceExisting = false;
    Context.Payload->TryGetBoolField(TEXT("replaceExisting"), bReplaceExisting);

    if (Context.SystemPath.IsEmpty() || TargetNodeId.IsEmpty() || InputName.IsEmpty() || DynamicInputScriptPath.IsEmpty())
    {
        Context.SendError(
            Context.SystemPath.IsEmpty() ? TEXT("Missing 'systemPath'.") :
            TargetNodeId.IsEmpty() ? TEXT("Missing 'targetNodeId'.") :
            InputName.IsEmpty() ? TEXT("Missing 'inputName'.") :
            TEXT("Missing 'dynamicInputScriptPath'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = LoadSystemOrError(Context);
    if (!System) { return true; }

    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, Context.EmitterName);
    if (!Handle)
    {
        Context.SendError(FString::Printf(TEXT("Emitter '%s' not found."), *Context.EmitterName), TEXT("EMITTER_NOT_FOUND"));
        return true;
    }
    UNiagaraScriptSource* ScriptSource = GetEmitterScriptSource(Handle);
    UNiagaraGraph* Graph = ScriptSource ? ScriptSource->NodeGraph : nullptr;
    if (!Graph)
    {
        Context.SendError(TEXT("Emitter has no Niagara graph source."), TEXT("NIAGARA_GRAPH_MISSING"));
        return true;
    }

    FGuid TargetGuid;
    if (!FGuid::Parse(TargetNodeId, TargetGuid))
    {
        Context.SendError(TEXT("Invalid 'targetNodeId' GUID format."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraNodeFunctionCall* TargetNode = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (UNiagaraNodeFunctionCall* FuncCall = Cast<UNiagaraNodeFunctionCall>(Node); FuncCall && FuncCall->NodeGuid == TargetGuid)
        {
            TargetNode = FuncCall;
            break;
        }
    }
    if (!TargetNode)
    {
        Context.SendError(FString::Printf(TEXT("Target node '%s' not found."), *TargetNodeId), TEXT("NODE_NOT_FOUND"));
        return true;
    }

    UNiagaraScript* DynamicInputScript = LoadObject<UNiagaraScript>(nullptr, *DynamicInputScriptPath);
    if (!DynamicInputScript)
    {
        Context.SendError(FString::Printf(TEXT("Dynamic Input script '%s' not found."), *DynamicInputScriptPath), TEXT("SCRIPT_NOT_FOUND"));
        return true;
    }
    if (!DynamicInputScript->IsDynamicInputScript())
    {
        Context.SendError(FString::Printf(TEXT("Script '%s' is not a Dynamic Input script."), *DynamicInputScriptPath), TEXT("INVALID_SCRIPT_USAGE"));
        return true;
    }

    const FNiagaraParameterHandle AliasedHandle(*InputName);
    const FName UnaliasedName = AliasedHandle.GetName();

    FNiagaraTypeDefinition InputType;
    bool bFoundType = false;
    FString AvailableInputs;
    if (UNiagaraGraph* CalledGraph = TargetNode->GetCalledGraph())
    {
        for (UEdGraphNode* Node : CalledGraph->Nodes)
        {
            if (UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(Node))
            {
                FString InputNameStr = InputNode->Input.GetName().ToString();
                if (!AvailableInputs.IsEmpty()) AvailableInputs += TEXT(", ");
                AvailableInputs += InputNameStr;
                if (InputNode->Input.GetName() == UnaliasedName)
                {
                    InputType = InputNode->Input.GetType();
                    bFoundType = true;
                }
            }
        }
    }
    if (!bFoundType)
    {
        Context.SendError(FString::Printf(TEXT("Input '%s' not found on target node. Available: %s"), *InputName, *AvailableInputs), TEXT("INPUT_NOT_FOUND"));
        return true;
    }

    Graph->Modify();
    UEdGraphPin& OverridePin = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(
        *TargetNode, AliasedHandle, InputType, FGuid(), FGuid());

    TArray<UNiagaraNodeFunctionCall*> ExistingDIs;
    for (UEdGraphPin* LinkedPin : OverridePin.LinkedTo)
    {
        if (UNiagaraNodeFunctionCall* ConnectedNode = Cast<UNiagaraNodeFunctionCall>(LinkedPin->GetOwningNode()))
        {
            ExistingDIs.Add(ConnectedNode);
        }
    }
    if (ExistingDIs.Num() > 0)
    {
        if (!bReplaceExisting)
        {
            Context.SendError(FString::Printf(TEXT("Input '%s' already has a Dynamic Input. Set replaceExisting=true to overwrite."), *InputName), TEXT("DYNAMIC_INPUT_EXISTS"));
            return true;
        }
        for (UNiagaraNodeFunctionCall* ExistingDI : ExistingDIs)
        {
            ExistingDI->DestroyNode();
        }
    }

    UNiagaraNodeFunctionCall* CreatedDINode = nullptr;
    FNiagaraStackGraphUtilities::SetDynamicInputForFunctionInput(
        OverridePin, DynamicInputScript, CreatedDINode);

    if (!CreatedDINode)
    {
        Context.SendError(TEXT("Failed to create Dynamic Input node."), TEXT("DYNAMIC_INPUT_CREATE_FAILED"));
        return true;
    }

    Graph->NotifyGraphChanged();
    MarkDirtyAndVerify(Context, System);

    Context.Result->SetStringField(TEXT("dynamicInputNodeId"), CreatedDINode->NodeGuid.ToString());
    Context.Result->SetStringField(TEXT("targetNodeId"), TargetNodeId);
    Context.Result->SetStringField(TEXT("inputName"), InputName);
    Context.Result->SetStringField(TEXT("dynamicInputScriptPath"), DynamicInputScriptPath);
    Context.Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Attached Dynamic Input '%s' to input '%s'."), *DynamicInputScriptPath, *InputName));
    Context.SendSuccess(true, TEXT("Niagara Dynamic Input assigned."));
    return true;
}
#endif

bool HandleDynamicInputAction(FActionContext& Context, const FString& SubAction)
{
#if MCP_HAS_NIAGARA_STACK_GRAPH_UTILITIES
    if (SubAction == TEXT("set_niagara_dynamic_input")) return SetNiagaraDynamicInput(Context);
#else
    if (SubAction == TEXT("set_niagara_dynamic_input"))
    {
        Context.SendError(TEXT("Niagara stack graph utilities are unavailable in this engine version."), TEXT("NIAGARA_DYNAMIC_INPUT_UNSUPPORTED"));
        return true;
    }
#endif
    return false;
}
}
#endif
