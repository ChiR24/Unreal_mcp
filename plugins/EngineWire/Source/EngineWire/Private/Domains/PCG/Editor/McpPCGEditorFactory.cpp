#include "Domains/PCG/Editor/McpPCGEditor.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Misc/ScopeExit.h"

#if MCP_HAS_PCG_EDITOR
#include "Schema/PCGEditorGraphSchemaActions.h"
#include "Nodes/PCGEditorGraphNodeBase.h"
#include "EdGraph/EdGraphSchema.h"
#endif

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
UEdGraphNode* CreatePCGEditorCounterpart(UPCGGraph* Graph, UEdGraph* EditorGraph, UPCGNode* Model, FString& OutError)
{
#if MCP_HAS_PCG_EDITOR
    if (!Model->GetSettings() || Model == Graph->GetInputNode() || Model == Graph->GetOutputNode())
    {
        OutError = TEXT("Missing input/output editor nodes require native editor initialization; they are not synthesized as ordinary nodes.");
        return nullptr;
    }
    FGraphContextMenuBuilder Builder(EditorGraph);
    EditorGraph->GetSchema()->GetGraphContextActions(Builder);
    TSharedPtr<FEdGraphSchemaAction> Creator, Fallback;
    for (int32 Index = 0; Index < Builder.GetNumActions(); ++Index)
    {
        const auto Action = Builder.GetSchemaAction(Index);
        if (Action->GetTypeId() != FPCGEditorGraphSchemaAction_NewNativeElement::StaticGetTypeId()) continue;
        const auto Native = StaticCastSharedPtr<FPCGEditorGraphSchemaAction_NewNativeElement>(Action);
        if (Native->SettingsClass == Model->GetSettings()->GetClass())
        {
            Creator = Action;
            break;
        }
        if (Native->SettingsClass && Native->SettingsClass->GetName() == TEXT("PCGTransformPointsSettings")) Fallback = Action;
    }
    if (!Creator.IsValid() && Fallback.IsValid() && Model->GetSettings()->GetClass()->GetName() != TEXT("PCGUserParameterGetSettings"))
    {
        const auto Native = StaticCastSharedPtr<FPCGEditorGraphSchemaAction_NewNativeElement>(Fallback);
        Native->SettingsClass = Model->GetSettings()->GetClass();
        Native->PreconfiguredInfo = FPCGPreConfiguredSettingsInfo();
        Creator = Fallback;
    }
    if (!Creator.IsValid())
    {
        OutError = TEXT("The native PCG schema did not supply a supported node creation action.");
        return nullptr;
    }
    // The native schema action opens an undo transaction internally. If the caller deliberately
    // has no recording (for example a durable edit), contain and discard that internal recording;
    // never cancel a transaction owned by an existing caller and never use cancellation as rollback.
    TUniquePtr<FMcpScopedEditorTransaction> NativeRecording;
    if (GEditor && !GEditor->IsTransactionActive())
    {
        NativeRecording = MakeUnique<FMcpScopedEditorTransaction>(FText::FromString(TEXT("Construct PCG Editor Representation")),
            EMcpMutationDurability::EditorStateOnly, TArray<UObject*>());
    }
    ON_SCOPE_EXIT { if (NativeRecording.IsValid()) NativeRecording->DiscardOwnedRecording(); };
    const TArray<UPCGNode*> PreviousModels = Graph->GetNodes();
    int32 X = 0, Y = 0;
    Model->GetNodePosition(X, Y);
    UEdGraphNode* Created = Creator->PerformAction(EditorGraph, nullptr, FVector2f(X, Y), false);
    UPCGEditorGraphNodeBase* Typed = Cast<UPCGEditorGraphNodeBase>(Created);
    UPCGNode* Temporary = Typed ? Typed->GetPCGNode() : nullptr;
    if (!Typed || !Temporary || PreviousModels.Contains(Temporary))
    {
        if (Created) EditorGraph->RemoveNode(Created);
        for (UPCGNode* Candidate : TArray<UPCGNode*>(Graph->GetNodes()))
        {
            if (!PreviousModels.Contains(Candidate)) Graph->RemoveNode(Candidate);
        }
        OutError = TEXT("Native PCG editor node creation failed; no model node was rebound.");
        return nullptr;
    }
    // Construct is a public virtual API. The schema chooses the native editor node class;
    // rebind that representation to the existing model instead of replacing model identity.
    Temporary->OnNodeChangedDelegate.RemoveAll(Typed);
    Graph->RemoveNode(Temporary);
    Typed->Modify();
    Typed->Construct(Model);
    Created->ReconstructNode();
    Temporary->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
    return Created;
#else
    OutError = TEXT("Public PCG editor node APIs are unavailable in this build.");
    return nullptr;
#endif
}
}
#endif
