#include "Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

namespace McpNiagaraAuthoringHandlers
{
static bool CreateNiagaraSystem(FActionContext& Context)
{
    if (Context.Name.IsEmpty())
    {
        Context.SendError(TEXT("Missing 'name' parameter."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (!Context.Path.EndsWith(TEXT("/")))
    {
        Context.Path += TEXT("/");
    }
    const FString FullPath = Context.Path + Context.Name;
    UPackage* Package = CreatePackage(*FPackageName::ObjectPathToPackageName(FullPath));
    if (!Package)
    {
        Context.SendError(TEXT("Failed to create package."), TEXT("PACKAGE_ERROR"));
        return true;
    }
    // RF_Transactional as the asset factory gives it: without it undo skipped the system and every Niagara stack
    // read reported "Object is not transctional, undo won't work for it!".
    UNiagaraSystem* NewSystem = NewObject<UNiagaraSystem>(Package, FName(*Context.Name), RF_Public | RF_Standalone | RF_Transactional);
    if (NewSystem)
    {
#if MCP_HAS_NIAGARA_SYSTEM_FACTORY_NEW
        FModuleManager::Get().LoadModule(TEXT("NiagaraEditor"));
        UNiagaraSystemFactoryNew::InitializeSystem(NewSystem, true);
#endif
    }
    if (!NewSystem)
    {
        Context.SendError(TEXT("Failed to create Niagara System."), TEXT("CREATE_FAILED"));
        return true;
    }
    FAssetRegistryModule::AssetCreated(NewSystem);
    if (Context.bSave)
    {
        McpSafeAssetSave(NewSystem);
    }
    if (!FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(NewSystem->GetPathName())))
    {
        Context.SendError(TEXT("Created system package does not exist on disk."), TEXT("CREATE_FAILED"));
        return true;
    }
    McpHandlerUtils::AddVerification(Context.Result, NewSystem);
    Context.Result->SetStringField(TEXT("systemPath"), NewSystem->GetPathName());
    Context.Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Created Niagara System: %s"), *Context.Name));
    Context.SendSuccess(true, TEXT("System created."));
    return true;
}

static bool CreateNiagaraEmitter(FActionContext& Context)
{
    if (Context.Name.IsEmpty())
    {
        Context.SendError(TEXT("Missing 'name' parameter."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (!Context.Path.EndsWith(TEXT("/")))
    {
        Context.Path += TEXT("/");
    }
    UPackage* Package = CreatePackage(*FPackageName::ObjectPathToPackageName(Context.Path + Context.Name));
    if (!Package)
    {
        Context.SendError(TEXT("Failed to create package."), TEXT("PACKAGE_ERROR"));
        return true;
    }
    UNiagaraEmitter* NewEmitter = NewObject<UNiagaraEmitter>(Package, FName(*Context.Name), RF_Public | RF_Standalone | RF_Transactional);
    if (!NewEmitter)
    {
        Context.SendError(TEXT("Failed to create Niagara Emitter."), TEXT("CREATE_FAILED"));
        return true;
    }
#if MCP_HAS_NIAGARA_EMITTER_FACTORY_NEW
    FModuleManager::Get().LoadModule(TEXT("NiagaraEditor"));
    UNiagaraEmitterFactoryNew::InitializeEmitter(NewEmitter, true);
    NewEmitter->SetUniqueEmitterName(Context.Name);
#endif
    FAssetRegistryModule::AssetCreated(NewEmitter);
    if (Context.bSave)
    {
        McpSafeAssetSave(NewEmitter);
    }
    McpHandlerUtils::AddVerification(Context.Result, NewEmitter);
    Context.Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Created Niagara Emitter: %s"), *Context.Name));
    Context.SendSuccess(true, TEXT("Emitter created."));
    return true;
}

static bool AddEmitterToSystem(FActionContext& Context)
{
    // Naming both fields when only one is absent sends the caller looking at the
    // one they already supplied. Name the missing one, and say what emitterPath
    // is for: this instances an EXISTING emitter asset, it does not create one.
    if (Context.SystemPath.IsEmpty() || Context.EmitterPath.IsEmpty())
    {
        const bool bNoSystem = Context.SystemPath.IsEmpty();
        const bool bNoEmitter = Context.EmitterPath.IsEmpty();
        FString Missing;
        if (bNoSystem && bNoEmitter) { Missing = TEXT("'systemPath' and 'emitterPath'"); }
        else if (bNoSystem) { Missing = TEXT("'systemPath'"); }
        else { Missing = TEXT("'emitterPath'"); }
        Context.SendError(
            FString::Printf(TEXT("Missing %s. add_emitter instances an existing Niagara emitter "
                                 "asset into a system: emitterPath must point at a UNiagaraEmitter "
                                 "asset (create one with create_effect kind=niagara_emitter)."),
                            *Missing),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *Context.SystemPath);
    UNiagaraEmitter* Emitter = LoadObject<UNiagaraEmitter>(nullptr, *Context.EmitterPath);
    if (!System || !Emitter)
    {
        Context.SendError(!System ? TEXT("Could not load Niagara System.") : TEXT("Could not load Niagara Emitter."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }
    FString AddedEmitterName;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    FModuleManager::Get().LoadModule(TEXT("NiagaraEditor"));
    System->Modify();
    Emitter->CheckVersionDataAvailable();
    const FGuid EmitterVersion = Emitter->GetExposedVersion().VersionGuid;
    FVersionedNiagaraEmitterData* EmitterData = Emitter->GetEmitterData(EmitterVersion);
    if (!EmitterData || !EmitterData->GraphSource)
    {
        Context.SendError(TEXT("Emitter graph source is not initialized."), TEXT("NIAGARA_EMITTER_INIT_FAILED"));
        return true;
    }
    const FGuid NewEmitterHandleId = FNiagaraEditorUtilities::AddEmitterToSystem(*System, *Emitter, EmitterVersion, false);
    FNiagaraEmitterHandle* AddedHandle = nullptr;
    for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
    {
        if (Handle.GetId() == NewEmitterHandleId)
        {
            AddedHandle = const_cast<FNiagaraEmitterHandle*>(&Handle);
            break;
        }
    }
    if (!AddedHandle)
    {
        Context.SendError(TEXT("Failed to add emitter to Niagara system."), TEXT("CREATE_FAILED"));
        return true;
    }
    // Honor a caller-supplied 'emitterName' for the handle. Upstream derives the handle name
    // from the source emitter asset, silently ignoring the requested name; renaming here also
    // keeps the handle name consistent with the 'emitterName' that module actions resolve
    // against (the dispatch layer sends the same default for both add-emitter and modules).
    if (!Context.EmitterName.IsEmpty())
    {
        AddedHandle->SetName(FName(*Context.EmitterName), *System);
    }
    AddedEmitterName = AddedHandle->GetName().ToString();
#else
    AddedEmitterName = System->AddEmitterHandle(*Emitter, Context.EmitterName.IsEmpty() ? FName(*Emitter->GetName()) : FName(*Context.EmitterName)).GetName().ToString();
#endif
    MarkDirtyAndVerify(Context, System);
    Context.Result->SetStringField(TEXT("emitterName"), AddedEmitterName);
    Context.Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added emitter '%s' to system."), *AddedEmitterName));
    Context.SendSuccess(true, TEXT("Emitter added to system."));
    return true;
}

static bool SetEmitterProperties(FActionContext& Context)
{
    UNiagaraSystem* System = nullptr;
    FNiagaraEmitterHandle* Handle = nullptr;
    if (!LoadSystemAndEmitter(Context, System, Handle))
    {
        return true;
    }
    // enabled turns the emitter on or off; moduleEnabled {Module: bool} turns stack modules on or off by the names
    // get_niagara_info lists, so a template module that fights the values set (a size-over-life curve, wind) can go.
    // Any other key used to be ignored under an "updated" reply.
    const TSharedPtr<FJsonObject>* PropsObj = nullptr;
    const TSharedPtr<FJsonObject>* ModulesObj = nullptr;
    bool bEnabled = false;
    const bool bHasProps = Context.Payload->TryGetObjectField(TEXT("emitterProperties"), PropsObj) && PropsObj->IsValid();
    const bool bHasEnabled = bHasProps && (*PropsObj)->TryGetBoolField(TEXT("enabled"), bEnabled);
    const bool bHasModules = bHasProps && (*PropsObj)->TryGetObjectField(TEXT("moduleEnabled"), ModulesObj) && ModulesObj->IsValid();
    if ((!bHasEnabled && !bHasModules) || (*PropsObj)->Values.Num() != int32(bHasEnabled) + int32(bHasModules))
    {
        Context.SendError(TEXT("emitterProperties takes enabled (a boolean) and/or moduleEnabled ({ModuleName: boolean}, names as get_niagara_info lists them). Set module inputs with set_parameter_value."), TEXT("UNSUPPORTED_PROPERTY"));
        return true;
    }
    UNiagaraScriptSource* Source = bHasModules ? GetEmitterScriptSource(Handle) : nullptr;
    UNiagaraGraph* Graph = Source ? Source->NodeGraph : nullptr;
    TArray<FString> Missing;
    if (bHasModules)
    {
        for (const auto& Pair : (*ModulesObj)->Values)
        {
            const FString ModuleName(*Pair.Key);
            bool bOn = true;
            int32 Matched = 0;
            if (Graph && Pair.Value.IsValid() && Pair.Value->TryGetBool(bOn))
            {
                for (UEdGraphNode* GraphNode : Graph->Nodes)
                {
                    UNiagaraNodeFunctionCall* Module = Cast<UNiagaraNodeFunctionCall>(GraphNode);
                    if (Module && Module->GetCalledUsage() == ENiagaraScriptUsage::Module &&
                        Module->GetFunctionName().Equals(ModuleName, ESearchCase::IgnoreCase))
                    {
                        Module->Modify();
                        Module->SetEnabledState(bOn ? ENodeEnabledState::Enabled : ENodeEnabledState::Disabled, false);
                        Module->MarkNodeRequiresSynchronization(TEXT("Module enabled state changed"), true);
                        ++Matched;
                    }
                }
            }
            if (Matched == 0)
            {
                Missing.Add(ModuleName);
            }
        }
    }
    if (Missing.Num() > 0)
    {
        Context.SendError(FString::Printf(TEXT("No module named %s on emitter '%s' (or its value is not a boolean); get_niagara_info lists each emitter's modules."), *FString::Join(Missing, TEXT(", ")), *Context.EmitterName), TEXT("MODULE_NOT_FOUND"));
        return true;
    }
    if (bHasEnabled)
    {
        Handle->SetIsEnabled(bEnabled, *System, false);
    }
    if (bHasModules)
    {
        System->RequestCompile(false);
    }
    MarkDirtyAndVerify(Context, System);
    Context.Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Updated properties for emitter '%s'."), *Context.EmitterName));
    Context.SendSuccess(true, TEXT("Emitter properties updated."));
    return true;
}

bool HandleSystemEmitterAction(FActionContext& Context, const FString& SubAction)
{
    if (SubAction == TEXT("create_niagara_system")) return CreateNiagaraSystem(Context);
    if (SubAction == TEXT("create_niagara_emitter")) return CreateNiagaraEmitter(Context);
    if (SubAction == TEXT("add_emitter_to_system")) return AddEmitterToSystem(Context);
    if (SubAction == TEXT("set_emitter_properties")) return SetEmitterProperties(Context);
    return false;
}
}
