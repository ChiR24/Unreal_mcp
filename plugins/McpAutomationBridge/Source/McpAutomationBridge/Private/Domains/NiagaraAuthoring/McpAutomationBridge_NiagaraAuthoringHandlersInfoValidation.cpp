#include "Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

#if WITH_EDITOR
namespace McpNiagaraAuthoringHandlers
{
// A module input's current value in its own type (int for an enum), or null for a type not rendered.
static TSharedPtr<FJsonValue> ReadModuleInputValue(const FNiagaraParameterStore& Store, const FNiagaraVariableWithOffset& Entry)
{
    const FNiagaraTypeDefinition& Type = Entry.GetType();
    const uint8* Data = Store.GetParameterData(FNiagaraVariable(Entry));
    if (!Data) return nullptr;
    if (Type == FNiagaraTypeDefinition::GetFloatDef()) return MakeShared<FJsonValueNumber>(*reinterpret_cast<const float*>(Data));
    if (Type == FNiagaraTypeDefinition::GetIntDef() || Type.IsEnum()) return MakeShared<FJsonValueNumber>(*reinterpret_cast<const int32*>(Data));
    if (Type == FNiagaraTypeDefinition::GetBoolDef()) return MakeShared<FJsonValueBoolean>(reinterpret_cast<const FNiagaraBool*>(Data)->GetValue());
    const int32 Count = Type == FNiagaraTypeDefinition::GetVec2Def() ? 2
        : Type == FNiagaraTypeDefinition::GetVec3Def() ? 3
        : (Type == FNiagaraTypeDefinition::GetVec4Def() || Type == FNiagaraTypeDefinition::GetColorDef()) ? 4 : 0;
    if (Count == 0) return nullptr;
    TArray<TSharedPtr<FJsonValue>> Components;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        Components.Add(MakeShared<FJsonValueNumber>(reinterpret_cast<const float*>(Data)[Index]));
    }
    return MakeShared<FJsonValueArray>(Components);
}

// Module inputs with their current values, keyed Module.Input -- the names set_parameter_value
// takes -- so an emitter can be tuned without guessing input names.
static TSharedPtr<FJsonObject> CollectModuleInputs(const TArray<UNiagaraScript*>& Scripts, const FString& EmitterName)
{
    TSharedPtr<FJsonObject> Inputs = MakeShared<FJsonObject>();
    const FString Scope = TEXT("Constants.") + EmitterName + TEXT(".");
    for (UNiagaraScript* Script : Scripts)
    {
        for (const FNiagaraVariableWithOffset& Entry : Script ? Script->RapidIterationParameters.ReadParameterVariables() : TArrayView<const FNiagaraVariableWithOffset>())
        {
            const FString Name = Entry.GetName().ToString();
            const FString Short = Name.StartsWith(Scope) ? Name.RightChop(Scope.Len()) : FString();
            if (Short.IsEmpty() || Inputs->HasField(Short)) continue;
            if (TSharedPtr<FJsonValue> Value = ReadModuleInputValue(Script->RapidIterationParameters, Entry))
            {
                Inputs->SetField(Short, Value);
            }
        }
    }
    return Inputs;
}

static void AddSystemInfo(TSharedPtr<FJsonObject>& InfoObj, UNiagaraSystem* System)
{
    InfoObj->SetStringField(TEXT("assetType"), TEXT("System"));
    InfoObj->SetNumberField(TEXT("emitterCount"), System->GetEmitterHandles().Num());
    TArray<TSharedPtr<FJsonValue>> EmittersArray;
    bool bHasGPU = false;
    const TArray<UNiagaraScript*> Scripts = GatherModuleInputScripts(System);
    for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
    {
        TSharedPtr<FJsonObject> EmitterObj = McpHandlerUtils::CreateResultObject();
        EmitterObj->SetStringField(TEXT("name"), Handle.GetName().ToString());
        EmitterObj->SetBoolField(TEXT("enabled"), Handle.GetIsEnabled());
        EmitterObj->SetObjectField(TEXT("moduleInputs"), CollectModuleInputs(Scripts, Handle.GetName().ToString()));
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
#if MCP_HAS_NIAGARA_STACK_GRAPH_UTILITIES
        // Enumerate the real stack modules per emitter. Without this, get_niagara_info reports
        // only emitters + user parameters, so callers cannot tell a genuine stack module from a
        // user-parameter shim — the readback that proves modules were actually authored. Walk the
        // script graph's function-call nodes and keep those whose called usage is Module (excludes
        // dynamic-input function calls). NOTE: FNiagaraStackGraphUtilities::GetOrderedModuleNodes is
        // declared but NOT DLL-exported, so it cannot be linked from another module — hence the
        // direct graph walk using the exported UNiagaraNodeFunctionCall::GetCalledUsage().
        if (UNiagaraScriptSource* ScriptSource = GetEmitterScriptSource(const_cast<FNiagaraEmitterHandle*>(&Handle)))
        {
            if (ScriptSource->NodeGraph)
            {
                TArray<TSharedPtr<FJsonValue>> ModulesArray;
                for (UEdGraphNode* GraphNode : ScriptSource->NodeGraph->Nodes)
                {
                    UNiagaraNodeFunctionCall* FuncNode = Cast<UNiagaraNodeFunctionCall>(GraphNode);
                    if (!FuncNode || FuncNode->FunctionScript == nullptr)
                    {
                        continue;
                    }
                    if (FuncNode->GetCalledUsage() != ENiagaraScriptUsage::Module)
                    {
                        continue;
                    }
                    TSharedPtr<FJsonObject> ModuleObj = McpHandlerUtils::CreateResultObject();
                    ModuleObj->SetStringField(TEXT("name"), FuncNode->GetFunctionName());
                    ModulesArray.Add(MakeShared<FJsonValueObject>(ModuleObj));
                }
                EmitterObj->SetNumberField(TEXT("moduleCount"), ModulesArray.Num());
                EmitterObj->SetArrayField(TEXT("modules"), ModulesArray);
            }
        }
#endif
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
    if (System)
    {
        Context.Result->SetNumberField(TEXT("emitterCount"), System->GetEmitterHandles().Num());
    }
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

    // Cheap structural warnings, independent of the stack.
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
    }

    // The real validation: harvest the stack issues that the Niagara editor itself computes
    // (unmet module dependencies, deprecated modules, compile failures). The harvest lives in
    // CollectNiagaraSystemStackIssues so the add_*_module responses run the identical check.
    CollectNiagaraSystemStackIssues(System, ErrorsArray, WarningsArray);

    const bool bIsValid = ErrorsArray.Num() == 0;
    ValidationResult->SetBoolField(TEXT("isValid"), bIsValid);
    ValidationResult->SetArrayField(TEXT("errors"), ErrorsArray);
    ValidationResult->SetArrayField(TEXT("warnings"), WarningsArray);
    Context.Result->SetObjectField(TEXT("validationResult"), ValidationResult);
    Context.Result->SetBoolField(TEXT("valid"), bIsValid);
    Context.Result->SetArrayField(TEXT("errors"), ErrorsArray);
    Context.Result->SetStringField(TEXT("message"), bIsValid ? TEXT("System is valid.") : TEXT("System has errors."));
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
