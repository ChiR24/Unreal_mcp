#include "Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

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

// The static switches that gate those inputs, keyed Module.Switch as set_parameter_value takes them: an enum by
// its display name, a bool, an int. A Ribbon Width written while Ribbon Width Mode is Unset draws nothing.
static TSharedPtr<FJsonObject> CollectModuleStaticSwitches(const FNiagaraEmitterHandle& Handle)
{
    TSharedPtr<FJsonObject> Switches = MakeShared<FJsonObject>();
    UNiagaraScriptSource* Source = GetEmitterScriptSource(const_cast<FNiagaraEmitterHandle*>(&Handle));
    if (!Source || !Source->NodeGraph)
    {
        return Switches;
    }
    for (UEdGraphNode* GraphNode : Source->NodeGraph->Nodes)
    {
        UNiagaraNodeFunctionCall* Node = Cast<UNiagaraNodeFunctionCall>(GraphNode);
        if (!Node || !Node->FunctionScript || Node->GetCalledUsage() != ENiagaraScriptUsage::Module)
        {
            continue;
        }
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (!IsStaticSwitchPin(Pin))
            {
                continue;
            }
            const FString Key = Node->GetFunctionName() + TEXT(".") + Pin->PinName.ToString();
            const UEnum* Enum = Cast<UEnum>(Pin->PinType.PinSubCategoryObject.Get());
            const int64 EnumValue = Enum ? Enum->GetValueByNameString(Pin->DefaultValue) : INDEX_NONE;
            if (EnumValue != INDEX_NONE)
            {
                Switches->SetStringField(Key, Enum->GetDisplayNameTextByValue(EnumValue).ToString());
            }
            else if (PinStructName(*Pin) == FName(TEXT("NiagaraBool")))
            {
                Switches->SetBoolField(Key, Pin->DefaultValue.ToBool());
            }
            else if (PinStructName(*Pin) == FName(TEXT("NiagaraInt32")))
            {
                Switches->SetNumberField(Key, FCString::Atoi(*Pin->DefaultValue));
            }
            else
            {
                Switches->SetStringField(Key, Pin->DefaultValue);
            }
        }
    }
    return Switches;
}

// Each renderer by the object path inspect.set_property writes (Material, bCastShadows ...): finding it took a dump
// of the emitter's own properties.
static TArray<TSharedPtr<FJsonValue>> CollectRenderers(MCP_NIAGARA_EMITTER_DATA_TYPE& EmitterData)
{
    TArray<TSharedPtr<FJsonValue>> Renderers;
    for (UNiagaraRendererProperties* Renderer : EmitterData.GetRenderers())
    {
        if (!Renderer)
        {
            continue;
        }
        TSharedPtr<FJsonObject> RendererObj = MakeShared<FJsonObject>();
        RendererObj->SetStringField(TEXT("class"), Renderer->GetClass()->GetName());
        RendererObj->SetStringField(TEXT("objectPath"), Renderer->GetPathName());
        RendererObj->SetBoolField(TEXT("enabled"), Renderer->GetIsEnabled());
        const UNiagaraSpriteRendererProperties* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer);
        const UNiagaraRibbonRendererProperties* Ribbon = Cast<UNiagaraRibbonRendererProperties>(Renderer);
        const UMaterialInterface* Material = Sprite ? Sprite->Material.Get() : Ribbon ? Ribbon->Material.Get() : nullptr;
        if (Material)
        {
            RendererObj->SetStringField(TEXT("material"), Material->GetPathName());
        }
        Renderers.Add(MakeShared<FJsonValueObject>(RendererObj));
    }
    return Renderers;
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
        EmitterObj->SetObjectField(TEXT("staticSwitches"), CollectModuleStaticSwitches(Handle));
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
            EmitterObj->SetArrayField(TEXT("renderers"), CollectRenderers(*MCP_GET_LATEST_EMITTER_DATA(Emitter)));
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
    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *TargetPath, nullptr, LOAD_NoWarn);
    UNiagaraEmitter* Emitter = System ? nullptr : LoadObject<UNiagaraEmitter>(nullptr, *TargetPath, nullptr, LOAD_NoWarn);
    if (!System && !Emitter)
    {
        Context.SendError(FString::Printf(TEXT("Niagara asset not found: %s"), *TargetPath), TEXT("ASSET_NOT_FOUND"));
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
    UNiagaraSystem* System = LoadSystemOrError(Context);
    if (!System)
    {
        return true;
    }

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
    Context.Result->SetBoolField(TEXT("valid"), bIsValid);
    Context.Result->SetArrayField(TEXT("errors"), ErrorsArray);
    Context.Result->SetArrayField(TEXT("warnings"), WarningsArray);
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
