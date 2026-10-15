#include "Domains/NiagaraAuthoring/McpNiagaraContext.h"

#include "UObject/UObjectHash.h"

namespace McpNiagaraAuthoringHandlers
{
// The user parameters an emitter reads, as the editor's paste gathers them: User.<Name> pins in its graph and the
// FNiagaraUserParameterBinding of its renderers and data interfaces.
static TSet<FName> ReadUserParameterNames(UObject* Emitter)
{
    TSet<FName> Names;
    ForEachObjectWithOuter(Emitter, [&Names](UObject* Object)
    {
        if (const UEdGraphNode* Node = Cast<UEdGraphNode>(Object))
        {
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin && Pin->PinName.ToString().StartsWith(TEXT("User.")))
                {
                    Names.Add(Pin->PinName);
                }
            }
            return;
        }
        for (TFieldIterator<FStructProperty> It(Object->GetClass()); It; ++It)
        {
            if (It->Struct && It->Struct->GetFName() == FName(TEXT("NiagaraUserParameterBinding")))
            {
                Names.Add(It->ContainerPtrToValuePtr<FNiagaraUserParameterBinding>(Object)->Parameter.GetName());
            }
        }
    });
    return Names;
}

// A module of the emitter that is a scratch pad script of its system, not of the emitter: a copy would still call
// into the source system. Empty when there is none.
static FString FindSystemScratchModule(UObject* Emitter, const UNiagaraSystem* Source)
{
    FString Found;
    ForEachObjectWithOuter(Emitter, [&Found, Emitter, Source](UObject* Object)
    {
        const UNiagaraNodeFunctionCall* Call = Cast<UNiagaraNodeFunctionCall>(Object);
        if (Found.IsEmpty() && Call && Call->FunctionScript && Call->FunctionScript->IsIn(Source) && !Call->FunctionScript->IsIn(Emitter))
        {
            Found = Call->GetFunctionName();
        }
    });
    return Found;
}

// Adds the user parameters the copied emitter reads that the system lacks, with the source system's values; a name
// the system already has as another type is named in OutConflicts.
static TArray<FString> CopyUserParameters(UNiagaraSystem* System, UNiagaraSystem* Source, UObject* SourceEmitter, TArray<FString>& OutConflicts)
{
    TArray<FString> Added;
    const TSet<FName> Wanted = ReadUserParameterNames(SourceEmitter);
    FNiagaraUserRedirectionParameterStore& Target = System->GetExposedParameters();
    for (const FNiagaraVariableWithOffset& Var : Source->GetExposedParameters().ReadParameterVariables())
    {
        if (!Wanted.Contains(Var.GetName()))
        {
            continue;
        }
        const FNiagaraVariableWithOffset* Existing = Target.ReadParameterVariables().FindByPredicate(
            [&Var](const FNiagaraVariableWithOffset& Other) { return Other.GetName() == Var.GetName(); });
        if (Existing)
        {
            if (Existing->GetType() != Var.GetType())
            {
                OutConflicts.Add(FString::Printf(TEXT("%s is a %s here and a %s in the source"), *Var.GetName().ToString(),
                                                 *Existing->GetType().GetName(), *Var.GetType().GetName()));
            }
            continue;
        }
        const FNiagaraVariable Param(Var);
        Target.AddParameter(Param, true);
        Source->GetExposedParameters().CopyParameterData(Target, Param);
        Added.Add(Param.GetName().ToString());
    }
    return Added;
}

bool CopyEmitterFromSystem(FActionContext& Context)
{
    UNiagaraSystem* System = LoadSystemOrError(Context);
    if (!System)
    {
        return true;
    }
    const FString SourcePath = GetJsonStringField(Context.Payload, TEXT("sourceSystemPath"));
    const FString SourceName = GetJsonStringField(Context.Payload, TEXT("sourceEmitterName"));
    UNiagaraSystem* Source = LoadObject<UNiagaraSystem>(nullptr, *SourcePath, nullptr, LOAD_NoWarn);
    if (!Source)
    {
        Context.SendError(FString::Printf(TEXT("sourceSystemPath '%s' is not a Niagara system."), *SourcePath), TEXT("ASSET_NOT_FOUND"));
        return true;
    }
    const FNiagaraEmitterHandle* SourceHandle = FindEmitterHandle(Source, SourceName);
    if (!SourceHandle)
    {
        TArray<FString> Names;
        for (const FNiagaraEmitterHandle& Handle : Source->GetEmitterHandles())
        {
            Names.Add(Handle.GetName().ToString());
        }
        Context.SendError(FString::Printf(TEXT("No emitter '%s' in %s. Its emitters: %s."), *SourceName, *SourcePath,
                                          *FString::Join(Names, TEXT(", "))), TEXT("EMITTER_NOT_FOUND"));
        return true;
    }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    const FVersionedNiagaraEmitter Instance = SourceHandle->GetInstance();
    UNiagaraEmitter* SourceEmitter = Instance.Emitter;
    FVersionedNiagaraEmitterData* SourceData = SourceEmitter ? SourceHandle->GetEmitterData() : nullptr;
    const UNiagaraEmitter* Parent = SourceData ? SourceData->GetParent().Emitter : nullptr;
#else
    UNiagaraEmitter* SourceEmitter = SourceHandle->GetInstance();
    const UNiagaraEmitter* Parent = SourceEmitter ? SourceEmitter->GetParent() : nullptr;
#endif
    if (!SourceEmitter)
    {
        Context.SendError(FString::Printf(TEXT("'%s' is a lightweight (stateless) emitter, which cannot be copied."), *SourceName), TEXT("UNSUPPORTED_EMITTER"));
        return true;
    }
    // A copy would lose its link to the parent emitter asset, so the parent is added instead (emitterPath).
    if (Parent)
    {
        Context.SendError(FString::Printf(TEXT("'%s' inherits from %s: add that emitter with emitterPath, then set the values that differ."),
                                          *SourceName, *Parent->GetPathName()), TEXT("EMITTER_INHERITS"));
        return true;
    }
    const FString ScratchModule = FindSystemScratchModule(SourceEmitter, Source);
    if (!ScratchModule.IsEmpty())
    {
        Context.SendError(FString::Printf(TEXT("Module '%s' of '%s' is a scratch pad of %s, which a copy would keep calling; copy it in the Niagara editor."),
                                          *ScratchModule, *SourceName, *SourcePath), TEXT("UNSUPPORTED_EMITTER"));
        return true;
    }
    FModuleManager::Get().LoadModule(TEXT("NiagaraEditor"));
    System->Modify();
    // The editor's own add: it stops the system's instances and rebuilds its emitter nodes and overview graph.
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    const FGuid AddedId = FNiagaraEditorUtilities::AddEmitterToSystem(*System, *SourceEmitter, Instance.Version, false);
#else
    const FGuid AddedId = FNiagaraEditorUtilities::AddEmitterToSystem(*System, *SourceEmitter, false);
#endif
    FNiagaraEmitterHandle* Added = nullptr;
    for (FNiagaraEmitterHandle& Handle : MCP_MUTABLE_EMITTER_HANDLES(System))
    {
        Added = Handle.GetId() == AddedId ? &Handle : Added;
    }
    if (!Added || !MCP_GET_EMITTER_DATA(*Added))
    {
        Context.SendError(TEXT("Failed to add the copied emitter to the system."), TEXT("CREATE_FAILED"));
        return true;
    }
    // The add made the source emitter the copy's parent; the copy stands alone, as its source does.
    MCP_GET_EMITTER_DATA(*Added)->RemoveParent();
    Added->SetName(FName(*(Context.EmitterName.IsEmpty() ? SourceName : Context.EmitterName)), *System);
    TArray<FString> Conflicts;
    const TArray<FString> AddedParams = CopyUserParameters(System, Source, SourceEmitter, Conflicts);
    System->RequestCompile(false);
    MarkDirtyAndVerify(Context, System);
    Context.Result->SetStringField(TEXT("emitterName"), Added->GetName().ToString());
    if (AddedParams.Num() > 0)
    {
        Context.Result->SetArrayField(TEXT("userParametersAdded"), McpHandlerUtils::ToJsonStringArray(AddedParams));
    }
    if (Conflicts.Num() > 0)
    {
        Context.Result->SetArrayField(TEXT("warnings"), McpHandlerUtils::ToJsonStringArray(Conflicts));
    }
    Context.SendSuccess(true, FString::Printf(TEXT("Copied emitter '%s' into the system as '%s'."), *SourceName, *Added->GetName().ToString()));
    return true;
}
}
