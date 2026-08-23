#include "Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

#if WITH_EDITOR
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/NiagaraEmitterHandleViewModel.h"
#include "ViewModels/Stack/NiagaraStackViewModel.h"
#include "ViewModels/Stack/NiagaraStackEntry.h"
#endif

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

// Recursively collect Error/Warning issues from a Niagara stack-entry tree. These are the same
// issues the Niagara editor surfaces in the stack panel (unmet module dependencies, deprecated
// modules, compile failures), so harvesting them is the authoritative "is this system broken?".
static void CollectStackIssues(UNiagaraStackEntry* Entry, TArray<TSharedPtr<FJsonValue>>& Errors, TArray<TSharedPtr<FJsonValue>>& Warnings)
{
    if (!Entry)
    {
        return;
    }
    for (const UNiagaraStackEntry::FStackIssue& Issue : Entry->GetIssues())
    {
        const FString Message = Issue.GetShortDescription().ToString();
        if (Issue.GetSeverity() == EStackIssueSeverity::Error)
        {
            Errors.Add(MakeShared<FJsonValueString>(Message));
        }
        else if (Issue.GetSeverity() == EStackIssueSeverity::Warning)
        {
            Warnings.Add(MakeShared<FJsonValueString>(Message));
        }
    }
    TArray<UNiagaraStackEntry*> Children;
    Entry->GetUnfilteredChildren(Children);
    for (UNiagaraStackEntry* Child : Children)
    {
        CollectStackIssues(Child, Errors, Warnings);
    }
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

    // The real validation: harvest the stack issues that the Niagara editor itself computes.
    // The previous implementation hard-coded isValid=true; an earlier attempt that inspected each
    // script's ENiagaraScriptCompileStatus missed the common failures ("unmet dependencies",
    // deprecated modules) because those are *stack issues*, not VM compile-status errors.
    //
    // We build a throwaway view model in FULL (non-data-processing) mode and let it refresh the
    // system + emitter stacks, then collect every Error/Warning stack issue. Full mode is REQUIRED:
    // UNiagaraStackModuleItem::RefreshIssues() early-outs to an empty issue list whenever the owning
    // system view model GetIsForDataProcessingOnly() is true (NiagaraStackModuleItem.cpp ~L967), so a
    // data-processing-only VM can never surface per-module errors — including the dependency check
    // that produces "The module has unmet dependencies." We keep the heavy bits off: bCanSimulate is
    // false (so SetupPreviewComponentAndInstance() creates no preview UNiagaraComponent) and
    // bCanAutoCompile/bCompileForEdit are false. SetupSequencer() still runs but only builds a
    // detached transient Sequencer (the same construction the Niagara asset editor performs).
    //
    // We can't reuse an already-open editor's view model: TNiagaraViewModelManager's lookup
    // references a static member not exported to other modules, and FNiagaraSystemToolkit lives in
    // NiagaraEditor/Private. So we always spin our own VM. We deliberately do NOT call the unexported
    // Cleanup(); letting the shared pointer drop runs ~FNiagaraSystemViewModel -> Cleanup() for us.
    TSharedRef<FNiagaraSystemViewModel> SystemViewModel = MakeShared<FNiagaraSystemViewModel>();
    {
        FNiagaraSystemViewModelOptions Options;
        Options.bCanAutoCompile = false;
        Options.bCanModifyEmittersFromTimeline = false;
        Options.bCanSimulate = false;
        Options.bCompileForEdit = false;
        Options.bIsForDataProcessingOnly = false;
        Options.EditMode = ENiagaraSystemViewModelEditMode::SystemAsset;
        // Initialize() -> RefreshAll() subscribes to the Niagara message manager keyed by this GUID;
        // it asserts on an empty key (NiagaraMessageManager.cpp: "Tried to subscribe to an asset
        // without a set asset key"). A throwaway unique key is fine — we never route messages, and the
        // view model's destructor (~FNiagaraSystemViewModel -> Cleanup()) tears the subscription down.
        Options.MessageLogGuid = FGuid::NewGuid();
        SystemViewModel->Initialize(*System, Options);
    }

    // Initialize() already RefreshAll()'d the stacks, but harvest defensively by refreshing each
    // root's children before walking it, so the per-module issues (the dependency check included)
    // are guaranteed current.
    auto RefreshAndCollect = [&ErrorsArray, &WarningsArray](UNiagaraStackViewModel* Stack)
    {
        if (!Stack)
        {
            return;
        }
        if (UNiagaraStackEntry* Root = Stack->GetRootEntry())
        {
            Root->RefreshChildren();
            CollectStackIssues(Root, ErrorsArray, WarningsArray);
        }
    };
    RefreshAndCollect(SystemViewModel->GetSystemStackViewModel());
    for (const TSharedRef<FNiagaraEmitterHandleViewModel>& EmitterHandleViewModel : SystemViewModel->GetEmitterHandleViewModels())
    {
        RefreshAndCollect(EmitterHandleViewModel->GetEmitterStackViewModel());
    }

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
