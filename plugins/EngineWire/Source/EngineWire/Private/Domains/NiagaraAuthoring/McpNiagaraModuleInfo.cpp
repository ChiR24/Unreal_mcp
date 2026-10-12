#include "Domains/NiagaraAuthoring/McpNiagaraContext.h"

namespace McpNiagaraAuthoringHandlers
{
#if MCP_HAS_NIAGARA_STACK_GRAPH_UTILITIES
// The parameter-map get and set node classes live in NiagaraEditor's private headers and are not exported, so they
// are told apart by class name.
static bool IsNodeOfClass(const UEdGraphNode* Node, const TCHAR* ClassName)
{
    return Node && Node->GetClass()->GetName() == ClassName;
}

// What a linked input reads: the parameter a parameter-map get hands it (User.Tint), or the dynamic input script
// that computes it.
static FString DescribeInputSource(const UEdGraphPin& Source)
{
    const UEdGraphNode* Node = Source.GetOwningNode();
    if (IsNodeOfClass(Node, TEXT("NiagaraNodeParameterMapGet")))
    {
        return Source.PinName.ToString();
    }
    if (const UNiagaraNodeFunctionCall* Function = Cast<UNiagaraNodeFunctionCall>(Node))
    {
        return TEXT("dynamic input ") + Function->GetFunctionName();
    }
    return Node ? Node->GetNodeTitle(ENodeTitleType::ListView).ToString() : FString();
}

// The stack modules of an emitter by name. A module's override node (the parameter-map set feeding its map input)
// holds one pin per input set in the stack; an input wired there reads a parameter or a dynamic input, while
// moduleInputs only had the literal stored for it, so a linked input showed a stale value. A dynamic input has an
// override node of its own (RandomRangeFloat.Minimum bound to User.SpeedMin is wired there).
void AddEmitterModuleInfo(TSharedPtr<FJsonObject>& EmitterObj, const FNiagaraEmitterHandle& Handle)
{
    UNiagaraScriptSource* ScriptSource = GetEmitterScriptSource(const_cast<FNiagaraEmitterHandle*>(&Handle));
    if (!ScriptSource || !ScriptSource->NodeGraph)
    {
        return;
    }
    const TSharedPtr<FJsonObject>* InputsField = nullptr;
    const TSharedPtr<FJsonObject> Inputs =
        EmitterObj->TryGetObjectField(TEXT("moduleInputs"), InputsField) && InputsField ? *InputsField : MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Modules;
    for (UEdGraphNode* GraphNode : ScriptSource->NodeGraph->Nodes)
    {
        UNiagaraNodeFunctionCall* Module = Cast<UNiagaraNodeFunctionCall>(GraphNode);
        const ENiagaraScriptUsage Usage = Module && Module->FunctionScript ? Module->GetCalledUsage() : ENiagaraScriptUsage::Function;
        if (Usage != ENiagaraScriptUsage::Module && Usage != ENiagaraScriptUsage::DynamicInput)
        {
            continue;
        }
        if (Usage == ENiagaraScriptUsage::Module)
        {
            TSharedPtr<FJsonObject> ModuleObj = McpHandlerUtils::CreateResultObject();
            ModuleObj->SetStringField(TEXT("name"), Module->GetFunctionName());
            ModuleObj->SetBoolField(TEXT("enabled"), Module->IsNodeEnabled());
            Modules.Add(MakeShared<FJsonValueObject>(ModuleObj));
        }
        const FString Prefix = Module->GetFunctionName() + TEXT(".");
        for (const UEdGraphPin* MapIn : Module->Pins)
        {
            const UEdGraphNode* Overrides = MapIn->Direction == EGPD_Input && MapIn->LinkedTo.Num() > 0
                ? MapIn->LinkedTo[0]->GetOwningNode() : nullptr;
            if (!IsNodeOfClass(Overrides, TEXT("NiagaraNodeParameterMapSet")))
            {
                continue;
            }
            for (const UEdGraphPin* Pin : Overrides->Pins)
            {
                const FString Key = Pin->PinName.ToString();
                if (Pin->Direction == EGPD_Input && Pin->LinkedTo.Num() > 0 && Key.StartsWith(Prefix))
                {
                    TSharedPtr<FJsonObject> Link = MakeShared<FJsonObject>();
                    Link->SetStringField(TEXT("linkedTo"), DescribeInputSource(*Pin->LinkedTo[0]));
                    Inputs->SetObjectField(Key, Link);
                }
            }
        }
    }
    EmitterObj->SetNumberField(TEXT("moduleCount"), Modules.Num());
    EmitterObj->SetArrayField(TEXT("modules"), Modules);
}
#endif

// moduleNames keeps only those modules' inputs and switches ("Collision"): a whole system's dump ran to 15 KB.
void KeepModuleEntries(const TSharedPtr<FJsonObject>& EmitterObj, const TArray<FString>& ModuleNames)
{
    for (const TCHAR* FieldName : {TEXT("moduleInputs"), TEXT("staticSwitches")})
    {
        const TSharedPtr<FJsonObject>* Field = nullptr;
        if (ModuleNames.Num() == 0 || !EmitterObj->TryGetObjectField(FieldName, Field) || !Field)
        {
            continue;
        }
        TArray<FString> Drop;
        for (const auto& Pair : (*Field)->Values)
        {
            const FString Key(Pair.Key.Len(), *Pair.Key);
            const FString Module = Key.Left(Key.Find(TEXT(".")));
            if (!ModuleNames.ContainsByPredicate([&Module](const FString& Name) { return Name.Equals(Module, ESearchCase::IgnoreCase); }))
            {
                Drop.Add(Key);
            }
        }
        for (const FString& Key : Drop)
        {
            (*Field)->RemoveField(Key);
        }
    }
}
}
