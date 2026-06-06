#include "McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

#if WITH_EDITOR
namespace McpNiagaraAuthoringHandlers
{
static void AddSystemInfo(TSharedPtr<FJsonObject>& InfoObj, UNiagaraSystem* System)
{
    InfoObj->SetStringField(TEXT("assetType"), TEXT("System"));
    InfoObj->SetNumberField(TEXT("emitterCount"), System->GetEmitterHandles().Num());
    TArray<TSharedPtr<FJsonValue>> EmittersArray;
    bool bHasGPU = false;
    for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
    {
        TSharedPtr<FJsonObject> EmitterObj = McpHandlerUtils::CreateResultObject();
        EmitterObj->SetStringField(TEXT("name"), Handle.GetName().ToString());
        EmitterObj->SetBoolField(TEXT("enabled"), Handle.GetIsEnabled());
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        UNiagaraEmitter* Emitter = Handle.GetInstance().Emitter;
#else
        UNiagaraEmitter* Emitter = Handle.GetInstance();
#endif
        if (Emitter && MCP_GET_LATEST_EMITTER_DATA(Emitter))
        {
            const bool bGpuEmitter = MCP_GET_LATEST_EMITTER_DATA(Emitter)->SimTarget == ENiagaraSimTarget::GPUComputeSim;
            EmitterObj->SetStringField(TEXT("simulationTarget"), bGpuEmitter ? TEXT("GPU") : TEXT("CPU"));
            bHasGPU = bHasGPU || bGpuEmitter;
        }
        EmittersArray.Add(MakeShared<FJsonValueObject>(EmitterObj));
    }
    InfoObj->SetArrayField(TEXT("emitters"), EmittersArray);
    TArray<FNiagaraVariable> Params;
    System->GetExposedParameters().GetParameters(Params);
    InfoObj->SetNumberField(TEXT("userParameterCount"), Params.Num());
    TArray<TSharedPtr<FJsonValue>> ParamsArray;
    for (const FNiagaraVariable& Param : Params)
    {
        TSharedPtr<FJsonObject> ParamObj = McpHandlerUtils::CreateResultObject();
        ParamObj->SetStringField(TEXT("name"), Param.GetName().ToString());
        ParamObj->SetStringField(TEXT("type"), Param.GetType().GetName());
        ParamsArray.Add(MakeShared<FJsonValueObject>(ParamObj));
    }
    InfoObj->SetArrayField(TEXT("userParameters"), ParamsArray);
    InfoObj->SetBoolField(TEXT("hasGPUEmitters"), bHasGPU);
}

static bool GetNiagaraInfo(FActionContext& Context)
{
    if (Context.AssetPath.IsEmpty() && Context.SystemPath.IsEmpty())
    {
        Context.SendError(TEXT("Missing 'assetPath' or 'systemPath'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const FString TargetPath = Context.AssetPath.IsEmpty() ? Context.SystemPath : Context.AssetPath;
    if (!UEditorAssetLibrary::DoesAssetExist(TargetPath))
    {
        Context.SendError(FString::Printf(TEXT("Niagara asset not found: %s"), *TargetPath), TEXT("ASSET_NOT_FOUND"));
        return true;
    }
    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *TargetPath);
    UNiagaraEmitter* Emitter = System ? nullptr : LoadObject<UNiagaraEmitter>(nullptr, *TargetPath);
    if (!System && !Emitter)
    {
        Context.SendError(TEXT("Could not load Niagara asset."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }
    TSharedPtr<FJsonObject> InfoObj = McpHandlerUtils::CreateResultObject();
    if (System)
    {
        AddSystemInfo(InfoObj, System);
    }
    else
    {
        InfoObj->SetStringField(TEXT("assetType"), TEXT("Emitter"));
        InfoObj->SetStringField(TEXT("name"), Emitter->GetName());
        if (MCP_NIAGARA_EMITTER_DATA_TYPE* EmData = MCP_GET_LATEST_EMITTER_DATA(Emitter))
        {
            InfoObj->SetStringField(TEXT("simulationTarget"), EmData->SimTarget == ENiagaraSimTarget::GPUComputeSim ? TEXT("GPU") : TEXT("CPU"));
        }
    }
    Context.Result->SetObjectField(TEXT("niagaraInfo"), InfoObj);
    Context.Result->SetStringField(TEXT("message"), TEXT("Retrieved Niagara asset information."));
    Context.SendSuccess(true, TEXT("Niagara info retrieved."));
    return true;
}

static bool ValidateNiagaraSystem(FActionContext& Context)
{
    if (Context.SystemPath.IsEmpty())
    {
        Context.SendError(TEXT("Missing 'systemPath'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (!UEditorAssetLibrary::DoesAssetExist(Context.SystemPath))
    {
        Context.SendError(FString::Printf(TEXT("Niagara system asset not found: %s"), *Context.SystemPath), TEXT("ASSET_NOT_FOUND"));
        return true;
    }
    UNiagaraSystem* System = LoadSystemOrError(Context);
    if (!System)
    {
        return true;
    }
    TSharedPtr<FJsonObject> ValidationResult = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> ErrorsArray;
    TArray<TSharedPtr<FJsonValue>> WarningsArray;
    if (System->GetEmitterHandles().Num() == 0)
    {
        WarningsArray.Add(MakeShared<FJsonValueString>(TEXT("System has no emitters.")));
    }
    for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
    {
        if (!Handle.GetIsEnabled())
        {
            WarningsArray.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Emitter '%s' is disabled."), *Handle.GetName().ToString())));
        }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle.GetEmitterData();
#else
        MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = Handle.GetInstance();
#endif
        if (EmitterData && EmitterData->GetRenderers().Num() == 0)
        {
            WarningsArray.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Emitter '%s' has no renderers."), *Handle.GetName().ToString())));
        }
    }
    ValidationResult->SetBoolField(TEXT("isValid"), true);
    ValidationResult->SetArrayField(TEXT("errors"), ErrorsArray);
    ValidationResult->SetArrayField(TEXT("warnings"), WarningsArray);
    Context.Result->SetObjectField(TEXT("validationResult"), ValidationResult);
    Context.Result->SetStringField(TEXT("message"), TEXT("System is valid."));
    Context.SendSuccess(true, TEXT("Validation complete."));
    return true;
}

bool HandleInfoValidationAction(FActionContext& Context, const FString& SubAction)
{
    if (SubAction == TEXT("get_niagara_info")) return GetNiagaraInfo(Context);
    if (SubAction == TEXT("validate_niagara_system")) return ValidateNiagaraSystem(Context);
    return false;
}
}
#endif
