#include "Domains/NiagaraAuthoring/McpNiagaraContext.h"

namespace McpNiagaraAuthoringHandlers
{
// The emitter a rapid-iteration name belongs to: "Constants.<Emitter>.<Module>.<Input>", in that emitter's scripts
// or mirrored into the system scripts. Empty for a system-stage input ("Constants.<Module>.<Input>").
static FString RapidIterationOwner(UNiagaraSystem* System, const FString& Name)
{
    for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
    {
        if (Name.StartsWith(TEXT("Constants.") + Handle.GetName().ToString() + TEXT(".")))
        {
            return Handle.GetName().ToString();
        }
    }
    return FString();
}

// Whether Graph calls ModuleName with a static switch named SwitchName: the match SetModuleStaticSwitch writes.
static bool GraphHasStaticSwitch(UNiagaraGraph* Graph, const FString& ModuleName, const FString& SwitchName)
{
    if (!Graph)
    {
        return false;
    }
    for (UEdGraphNode* GraphNode : Graph->Nodes)
    {
        UNiagaraNodeFunctionCall* Node = Cast<UNiagaraNodeFunctionCall>(GraphNode);
        if (!Node || !Node->FunctionScript || !Node->GetFunctionName().Equals(ModuleName, ESearchCase::IgnoreCase))
        {
            continue;
        }
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (IsStaticSwitchPin(Pin) && Pin->PinName.ToString().Equals(SwitchName, ESearchCase::IgnoreCase))
            {
                return true;
            }
        }
    }
    return false;
}

// Where ParamName is found, as SetModuleInputValue and SetModuleStaticSwitch match it: the module inputs first,
// else the static switches. Each emitter's name, and an empty name for the system stage.
static TArray<FString> FindInputOwners(UNiagaraSystem* System, const FString& ParamName)
{
    TArray<FString> Owners;
    for (UNiagaraScript* Script : GatherModuleInputScripts(System))
    {
        for (const FNiagaraVariableWithOffset& Entry : Script ? Script->RapidIterationParameters.ReadParameterVariables() : TArrayView<const FNiagaraVariableWithOffset>())
        {
            const FString Name = Entry.GetName().ToString();
            if (Name == ParamName || Name.EndsWith(TEXT(".") + ParamName))
            {
                Owners.AddUnique(RapidIterationOwner(System, Name));
            }
        }
    }
    FString ModuleName;
    FString SwitchName;
    if (Owners.Num() > 0 || !ParamName.Split(TEXT("."), &ModuleName, &SwitchName))
    {
        return Owners;
    }
    UNiagaraScript* SystemScript = System->GetSystemSpawnScript();
    UNiagaraScriptSource* SystemSource = SystemScript ? Cast<UNiagaraScriptSource>(SystemScript->GetLatestSource()) : nullptr;
    if (SystemSource && GraphHasStaticSwitch(SystemSource->NodeGraph, ModuleName, SwitchName))
    {
        Owners.Add(FString());
    }
    for (FNiagaraEmitterHandle& Handle : MCP_MUTABLE_EMITTER_HANDLES(System))
    {
        UNiagaraScriptSource* Source = GetEmitterScriptSource(&Handle);
        if (Source && GraphHasStaticSwitch(Source->NodeGraph, ModuleName, SwitchName))
        {
            Owners.AddUnique(Handle.GetName().ToString());
        }
    }
    return Owners;
}

FString ListEmitterNames(UNiagaraSystem* System)
{
    TArray<FString> Names;
    for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
    {
        Names.Add(Handle.GetName().ToString());
    }
    return FString::Join(Names, TEXT(", "));
}

bool FindInputEmitter(UNiagaraSystem* System, const FString& ParamName, FString& OutEmitter, FString& OutError)
{
    TArray<FString> Owners = FindInputOwners(System, ParamName);
    OutEmitter = Owners.Num() == 1 ? Owners[0] : FString();
    if (Owners.Num() <= 1)
    {
        return true;
    }
    const bool bSystemStage = Owners.Remove(FString()) > 0;
    OutError = FString::Printf(TEXT("needs emitterName: one of [%s]%s"), *FString::Join(Owners, TEXT(", ")),
        bSystemStage ? TEXT(" (the system-stage one takes its full Constants.<Module>.<Input> name)") : TEXT(""));
    return false;
}

bool ResolveInputEmitter(FActionContext& Context, UNiagaraSystem* System, const FString& ParamName)
{
    FString Error;
    if (!FindInputEmitter(System, ParamName, Context.EmitterName, Error))
    {
        Context.SendError(FString::Printf(TEXT("Parameter '%s' matches in more than one place in Niagara system '%s' and %s, or its full Constants.<Emitter>.<Module>.<Input> name. Nothing was changed."),
            *ParamName, *System->GetPathName(), *Error), TEXT("EMITTER_NOT_FOUND"));
        return false;
    }
    Context.Result->SetStringField(TEXT("systemPath"), System->GetPathName());
    if (!Context.EmitterName.IsEmpty())
    {
        Context.Result->SetStringField(TEXT("emitterName"), Context.EmitterName);
        Context.Result->SetStringField(TEXT("emitterResolvedBy"), TEXT("only-match"));
    }
    return true;
}
}
