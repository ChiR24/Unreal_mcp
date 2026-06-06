#include "McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

#if WITH_EDITOR
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
    UNiagaraSystem* NewSystem = NewObject<UNiagaraSystem>(Package, FName(*Context.Name), RF_Public | RF_Standalone);
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
    McpHandlerUtils::AddVerification(Context.Result, NewSystem);
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
    UNiagaraEmitter* NewEmitter = NewObject<UNiagaraEmitter>(Package, FName(*Context.Name), RF_Public | RF_Standalone);
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
    if (Context.SystemPath.IsEmpty() || Context.EmitterPath.IsEmpty())
    {
        Context.SendError(TEXT("Missing 'systemPath' or 'emitterPath'."), TEXT("INVALID_ARGUMENT"));
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
    AddedEmitterName = AddedHandle->GetName().ToString();
#else
    AddedEmitterName = System->AddEmitterHandle(*Emitter, FName(*Emitter->GetName())).GetName().ToString();
#endif
    MarkDirtyAndVerify(Context, System);
    Context.Result->SetStringField(TEXT("emitterName"), AddedEmitterName);
    Context.Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added emitter '%s' to system."), *Emitter->GetName()));
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
    const TSharedPtr<FJsonObject>* PropsObj;
    if (Context.Payload->TryGetObjectField(TEXT("emitterProperties"), PropsObj) && PropsObj->IsValid())
    {
        bool bEnabled = false;
        if ((*PropsObj)->TryGetBoolField(TEXT("enabled"), bEnabled))
        {
            Handle->SetIsEnabled(bEnabled, *System, false);
        }
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
#endif
