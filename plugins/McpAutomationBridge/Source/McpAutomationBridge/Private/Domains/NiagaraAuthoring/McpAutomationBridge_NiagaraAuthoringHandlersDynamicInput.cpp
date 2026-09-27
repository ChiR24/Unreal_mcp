#include "Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

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

    // targetNodeId is optional: ResolveDynamicInputTargetNode finds the module by name/input.
    if (Context.SystemPath.IsEmpty() || InputName.IsEmpty() || DynamicInputScriptPath.IsEmpty())
    {
        Context.SendError(
            Context.SystemPath.IsEmpty() ? TEXT("Missing 'systemPath'.") :
            InputName.IsEmpty() ? TEXT("Missing 'inputName'.") :
            TEXT("Missing 'dynamicInputScriptPath'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UNiagaraSystem* System = nullptr;
    FNiagaraEmitterHandle* Handle = nullptr;
    if (!LoadSystemAndEmitter(Context, System, Handle))
    {
        return true;
    }
    UNiagaraScriptSource* ScriptSource = GetEmitterScriptSource(Handle);
    UNiagaraGraph* Graph = ScriptSource ? ScriptSource->NodeGraph : nullptr;
    if (!Graph)
    {
        Context.SendError(TEXT("Emitter has no Niagara graph source."), TEXT("NIAGARA_GRAPH_MISSING"));
        return true;
    }

    FString ResolveError;
    FString ResolveErrorCode;
    UNiagaraNodeFunctionCall* TargetNode =
        ResolveDynamicInputTargetNode(Context, Graph, TargetNodeId, InputName, ResolveError, ResolveErrorCode);
    if (!TargetNode)
    {
        Context.SendError(ResolveError, ResolveErrorCode);
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

    // Module inputs are stored as "Module.<Name>" while the stack addresses them through the
    // aliased "<FunctionName>.<Name>" handle, so accept the bare name, either spelling, and
    // build the aliased handle from the resolved module exactly as the stack does.
    const FNiagaraParameterHandle AliasedHandle(FName(*TargetNode->GetFunctionName()), FName(*BareInputName(InputName)));
    UNiagaraNodeInput* MatchedInput = FindModuleInputNode(TargetNode, InputName);
    if (!MatchedInput)
    {
        TArray<FString> Available;
        if (UNiagaraGraph* CalledGraph = TargetNode->GetCalledGraph())
        {
            for (UEdGraphNode* Node : CalledGraph->Nodes)
            {
                if (UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(Node))
                {
                    Available.Add(InputNode->Input.GetName().ToString());
                }
            }
        }
        Context.SendError(FString::Printf(TEXT("Input '%s' not found on target node. Available: %s"), *InputName, *FString::Join(Available, TEXT(", "))), TEXT("INPUT_NOT_FOUND"));
        return true;
    }
    const FNiagaraTypeDefinition InputType = MatchedInput->Input.GetType();

    FNiagaraTypeDefinition DIOutputType;
    bool bFoundDIOutputType = false;
    if (UNiagaraScriptSourceBase* DISourceBase = DynamicInputScript->GetLatestSource())
    {
        if (UNiagaraScriptSource* DIScriptSource = Cast<UNiagaraScriptSource>(DISourceBase))
        {
            if (UNiagaraGraph* DIGraph = DIScriptSource->NodeGraph)
            {
                for (UEdGraphNode* Node : DIGraph->Nodes)
                {
                    if (UNiagaraNodeOutput* OutputNode = Cast<UNiagaraNodeOutput>(Node))
                    {
                        const TArray<FNiagaraVariable>& DIOutputs = OutputNode->GetOutputs();
                        if (DIOutputs.Num() > 0)
                        {
                            DIOutputType = DIOutputs[0].GetType();
                            bFoundDIOutputType = true;
                            break;
                        }
                    }
                }
            }
        }
    }
    if (bFoundDIOutputType && DIOutputType != InputType)
    {
        Context.SendError(FString::Printf(TEXT("Dynamic Input output type does not match input '%s' type."), *InputName), TEXT("TYPE_MISMATCH"));
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
        FString ErrorMsg = TEXT("Failed to create Dynamic Input node.");
        if (bReplaceExisting && ExistingDIs.Num() > 0)
        {
            ErrorMsg += TEXT(" Note: existing Dynamic Input was removed.");
        }
        Context.SendError(ErrorMsg, TEXT("DYNAMIC_INPUT_CREATE_FAILED"));
        return true;
    }

    Graph->NotifyGraphChanged();
    MarkDirtyAndVerify(Context, System);

    Context.Result->SetStringField(TEXT("dynamicInputNodeId"), CreatedDINode->NodeGuid.ToString());
    Context.Result->SetStringField(TEXT("targetNodeId"), TargetNode->NodeGuid.ToString());
    Context.Result->SetStringField(TEXT("targetModuleName"), TargetNode->GetFunctionName());
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
