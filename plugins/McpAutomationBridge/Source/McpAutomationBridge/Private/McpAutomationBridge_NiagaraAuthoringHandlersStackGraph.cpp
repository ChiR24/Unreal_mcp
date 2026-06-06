#include "McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

#if WITH_EDITOR
namespace McpNiagaraAuthoringHandlers
{
UNiagaraNodeFunctionCall* AddModuleToEmitterStack(
    FNiagaraEmitterHandle* Handle,
    const FString& ModuleScriptPath,
    ENiagaraScriptUsage TargetUsage,
    const FString& SuggestedName)
{
    if (!Handle)
    {
        return nullptr;
    }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle->GetEmitterData();
#else
    MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle->GetInstance();
#endif
    if (!EmitterData)
    {
        return nullptr;
    }
    UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(EmitterData->GraphSource);
    if (!ScriptSource || !ScriptSource->NodeGraph)
    {
        return nullptr;
    }
    UNiagaraNodeOutput* TargetOutput = nullptr;
    for (UEdGraphNode* Node : ScriptSource->NodeGraph->Nodes)
    {
        if (UNiagaraNodeOutput* OutputNode = Cast<UNiagaraNodeOutput>(Node))
        {
            if (OutputNode->GetUsage() == TargetUsage)
            {
                TargetOutput = OutputNode;
                break;
            }
        }
    }
    if (!TargetOutput)
    {
        return nullptr;
    }
    FSoftObjectPath AssetRef(ModuleScriptPath);
    UNiagaraScript* ModuleScript = Cast<UNiagaraScript>(AssetRef.TryLoad());
    if (!ModuleScript)
    {
        return nullptr;
    }
#if MCP_HAS_NIAGARA_STACK_GRAPH_UTILITIES
    return FNiagaraStackGraphUtilities::AddScriptModuleToStack(
        ModuleScript,
        *TargetOutput,
        INDEX_NONE,
        SuggestedName.IsEmpty() ? ModuleScript->GetName() : SuggestedName);
#else
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("AddModule failed: FNiagaraStackGraphUtilities is not available in UE 5.0. Consider upgrading to UE 5.1+ for full Niagara stack graph support."));
    return nullptr;
#endif
}

UNiagaraScriptSource* GetEmitterScriptSource(FNiagaraEmitterHandle* Handle)
{
    if (!Handle)
    {
        return nullptr;
    }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle->GetEmitterData();
#else
    MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle->GetInstance();
#endif
    return EmitterData ? Cast<UNiagaraScriptSource>(EmitterData->GraphSource) : nullptr;
}

bool EnsureScriptOutputGraph(UNiagaraScriptSource* ScriptSource, ENiagaraScriptUsage ScriptUsage, FGuid ScriptUsageId)
{
    if (!ScriptSource || !ScriptSource->NodeGraph)
    {
        return false;
    }
    UNiagaraGraph* Graph = ScriptSource->NodeGraph;
    Graph->Modify();
    UNiagaraNodeOutput* OutputNode = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (UNiagaraNodeOutput* Candidate = Cast<UNiagaraNodeOutput>(Node))
        {
            if (Candidate->GetUsage() == ScriptUsage && Candidate->GetUsageId() == ScriptUsageId)
            {
                OutputNode = Candidate;
                break;
            }
        }
    }
    if (!OutputNode)
    {
        FGraphNodeCreator<UNiagaraNodeOutput> OutputNodeCreator(*Graph);
        OutputNode = OutputNodeCreator.CreateNode();
        OutputNode->SetUsage(ScriptUsage);
        OutputNode->SetUsageId(ScriptUsageId);
        OutputNode->Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetParameterMapDef(), TEXT("Out")));
        OutputNodeCreator.Finalize();
    }
    FGraphNodeCreator<UNiagaraNodeInput> InputNodeCreator(*Graph);
    UNiagaraNodeInput* InputNode = InputNodeCreator.CreateNode();
    InputNode->Input = FNiagaraVariable(FNiagaraTypeDefinition::GetParameterMapDef(), TEXT("InputMap"));
    InputNode->Usage = ENiagaraInputNodeUsage::Parameter;
    InputNodeCreator.Finalize();
    TArray<UEdGraphPin*> OutputInputPins;
    OutputNode->GetInputPins(OutputInputPins);
    TArray<UEdGraphPin*> InputOutputPins;
    InputNode->GetOutputPins(InputOutputPins);
    if (OutputInputPins.Num() == 0 || InputOutputPins.Num() == 0 || !OutputInputPins[0] || !InputOutputPins[0])
    {
        return false;
    }
    UEdGraphPin* OutputInputPin = OutputInputPins[0];
    UEdGraphPin* InputOutputPin = InputOutputPins[0];
    OutputInputPin->BreakAllPinLinks(true);
    OutputInputPin->MakeLinkTo(InputOutputPin);
    OutputInputPin->GetOwningNode()->PinConnectionListChanged(OutputInputPin);
    InputOutputPin->GetOwningNode()->PinConnectionListChanged(InputOutputPin);
    Graph->NotifyGraphChanged();
    return true;
}
}
#endif
