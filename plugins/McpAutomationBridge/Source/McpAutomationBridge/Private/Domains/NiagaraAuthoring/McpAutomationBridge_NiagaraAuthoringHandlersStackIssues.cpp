#include "Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

#if WITH_EDITOR
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/NiagaraEmitterHandleViewModel.h"
#include "ViewModels/Stack/NiagaraStackViewModel.h"
#include "ViewModels/Stack/NiagaraStackEntry.h"

namespace McpNiagaraAuthoringHandlers
{
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

// Shared by validate_niagara_system and the add_*_module responses (dogfood #106), so an add
// call reports the very same "The module has unmet dependencies" the validator would.
//
// An earlier attempt that inspected each script's ENiagaraScriptCompileStatus missed the common
// failures ("unmet dependencies", deprecated modules) because those are *stack issues*, not VM
// compile-status errors.
//
// We build a throwaway view model in FULL (non-data-processing) mode and let it refresh the
// system + emitter stacks, then collect every Error/Warning stack issue. Full mode is REQUIRED:
// UNiagaraStackModuleItem::RefreshIssues() early-outs to an empty issue list whenever the owning
// system view model GetIsForDataProcessingOnly() is true (NiagaraStackModuleItem.cpp ~L967), so a
// data-processing-only VM can never surface per-module errors - including the dependency check
// that produces "The module has unmet dependencies." We keep the heavy bits off: bCanSimulate is
// false (so SetupPreviewComponentAndInstance() creates no preview UNiagaraComponent) and
// bCanAutoCompile/bCompileForEdit are false. SetupSequencer() still runs but only builds a
// detached transient Sequencer (the same construction the Niagara asset editor performs).
//
// We can't reuse an already-open editor's view model: TNiagaraViewModelManager's lookup
// references a static member not exported to other modules, and FNiagaraSystemToolkit lives in
// NiagaraEditor/Private. So we always spin our own VM. We deliberately do NOT call the unexported
// Cleanup(); letting the shared pointer drop runs ~FNiagaraSystemViewModel -> Cleanup() for us.
void CollectNiagaraSystemStackIssues(
    UNiagaraSystem* System,
    TArray<TSharedPtr<FJsonValue>>& OutErrors,
    TArray<TSharedPtr<FJsonValue>>& OutWarnings)
{
    if (!System)
    {
        return;
    }
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
        // without a set asset key"). A throwaway unique key is fine - we never route messages, and the
        // view model's destructor (~FNiagaraSystemViewModel -> Cleanup()) tears the subscription down.
        Options.MessageLogGuid = FGuid::NewGuid();
        SystemViewModel->Initialize(*System, Options);
    }

    // Initialize() already RefreshAll()'d the stacks, but harvest defensively by refreshing each
    // root's children before walking it, so the per-module issues (the dependency check included)
    // are guaranteed current.
    auto RefreshAndCollect = [&OutErrors, &OutWarnings](UNiagaraStackViewModel* Stack)
    {
        if (!Stack)
        {
            return;
        }
        if (UNiagaraStackEntry* Root = Stack->GetRootEntry())
        {
            Root->RefreshChildren();
            CollectStackIssues(Root, OutErrors, OutWarnings);
        }
    };
    RefreshAndCollect(SystemViewModel->GetSystemStackViewModel());
    for (const TSharedRef<FNiagaraEmitterHandleViewModel>& EmitterHandleViewModel : SystemViewModel->GetEmitterHandleViewModels())
    {
        RefreshAndCollect(EmitterHandleViewModel->GetEmitterStackViewModel());
    }
}
}
#endif
