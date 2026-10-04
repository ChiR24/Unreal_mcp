#include "Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

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

// The pin default the Niagara stack writes for Value: an enum entry by name (matched here by display name, name or
// value), a bool as true/false, an int as its digits. Empty when Value fits none; Allowed then names what fits.
static FString StaticSwitchDefault(const UEdGraphPin& Pin, const TSharedPtr<FJsonValue>& Value, FString& Allowed)
{
    FString Text;
    double Number = 0.0;
    const bool bText = Value.IsValid() && Value->Type == EJson::String && Value->TryGetString(Text);
    const bool bNumber = Value.IsValid() && Value->Type == EJson::Number && Value->TryGetNumber(Number);
    if (const UEnum* Enum = Cast<UEnum>(Pin.PinType.PinSubCategoryObject.Get()))
    {
        FString Match;
        const int32 Count = Enum->ContainsExistingMax() ? Enum->NumEnums() - 1 : Enum->NumEnums();
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const int64 EntryValue = Enum->GetValueByIndex(Index);
            const FString Display = Enum->GetDisplayNameTextByIndex(Index).ToString();
            const FString Name = Enum->GetNameStringByValue(EntryValue);
            Allowed += (Allowed.IsEmpty() ? TEXT("") : TEXT(", ")) + Display;
            if ((bNumber && EntryValue == static_cast<int64>(Number)) || (bText && (Text.Equals(Display, ESearchCase::IgnoreCase) || Text.Equals(Name, ESearchCase::IgnoreCase))))
            {
                Match = Name;
            }
        }
        return Match;
    }
    if (PinStructName(Pin) == FName(TEXT("NiagaraBool")))
    {
        Allowed = TEXT("true, false");
        bool bValue = bNumber && Number != 0.0;
        const bool bParsed = bNumber || (Value.IsValid() && Value->Type == EJson::Boolean && Value->TryGetBool(bValue)) || (bText && LexTryParseString(bValue, *Text));
        return bParsed ? LexToString(bValue) : FString();
    }
    Allowed = TEXT("a whole number");
    const bool bDigits = bText && Text.IsNumeric();
    return bNumber || bDigits ? LexToString(bNumber ? static_cast<int32>(Number) : FCString::Atoi(*Text)) : FString();
}

// Gathers the call nodes that can carry the switch: the named emitter's graph, or with no emitter the system
// stack's graph and every emitter's.
static TArray<UNiagaraGraph*> GatherStackGraphs(UNiagaraSystem* System, const FString& EmitterName)
{
    TArray<UNiagaraGraph*> Graphs;
    UNiagaraScript* SystemScript = EmitterName.IsEmpty() ? System->GetSystemSpawnScript() : nullptr;
    if (UNiagaraScriptSource* Source = SystemScript ? Cast<UNiagaraScriptSource>(SystemScript->GetLatestSource()) : nullptr)
    {
        Graphs.AddUnique(Source->NodeGraph);
    }
    for (FNiagaraEmitterHandle& Handle : MCP_MUTABLE_EMITTER_HANDLES(System))
    {
        UNiagaraScriptSource* Source = GetEmitterScriptSource(&Handle);
        if (Source && (EmitterName.IsEmpty() || Handle.GetName().ToString() == EmitterName))
        {
            Graphs.AddUnique(Source->NodeGraph);
        }
    }
    Graphs.Remove(nullptr);
    return Graphs;
}

int32 SetModuleStaticSwitch(UNiagaraSystem* System, const FString& EmitterName, const FString& ParamName, const TSharedPtr<FJsonValue>& Value, FString& OutError)
{
    FString ModuleName;
    FString SwitchName;
    if (!System || !ParamName.Split(TEXT("."), &ModuleName, &SwitchName))
    {
        return 0;
    }
    int32 Written = 0;
    for (UNiagaraGraph* Graph : GatherStackGraphs(System, EmitterName))
    {
        for (UEdGraphNode* GraphNode : Graph->Nodes)
        {
            UNiagaraNodeFunctionCall* Node = Cast<UNiagaraNodeFunctionCall>(GraphNode);
            if (!Node || !Node->FunctionScript || !Node->GetFunctionName().Equals(ModuleName, ESearchCase::IgnoreCase))
            {
                continue;
            }
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (!IsStaticSwitchPin(Pin) || !Pin->PinName.ToString().Equals(SwitchName, ESearchCase::IgnoreCase))
                {
                    continue;
                }
                FString Allowed;
                const FString Default = StaticSwitchDefault(*Pin, Value, Allowed);
                if (Default.IsEmpty())
                {
                    OutError = FString::Printf(TEXT("Static switch '%s' takes %s."), *ParamName, *Allowed);
                    return 0;
                }
                // What the stack does for a static switch: write the pin, then resynchronise the node so the
                // graph's change id moves and the next compile rebuilds the script.
                Pin->Modify();
                Pin->DefaultValue = Default;
                Node->MarkNodeRequiresSynchronization(TEXT("Static switch set over MCP"), true);
                ++Written;
            }
        }
    }
    return Written;
}
}
