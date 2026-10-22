#include "Domains/PCG/Parameters/McpPCGParameters.h"
#include "Domains/PCG/Discovery/McpPCGRead.h"
#include "Domains/PCG/Editing/McpPCGEditState.h"
#include "Domains/PCG/Editor/McpPCGEditor.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Domains/PCG/Discovery/McpPCGBudget.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
bool HandlePCGParameters(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave)
{
    const TSet<FString> Actions = {TEXT("get_pcg_parameters"), TEXT("add_pcg_parameter"), TEXT("set_pcg_parameter"),
        TEXT("rename_pcg_parameter"), TEXT("remove_pcg_parameter"), TEXT("reset_pcg_parameter_override")};
    if (!Actions.Contains(Action)) return false;
#if !MCP_HAS_PCG_PARAMETERS
    Bridge->SendAutomationError(Socket, RequestId, TEXT("These native PCG graph parameter APIs are unavailable in this engine version."), TEXT("NOT_SUPPORTED"));
    return true;
#else
    FString Error;
    FPCGParameterTarget Target;
    if (!ResolvePCGParameterTarget(Payload, Target, Error))
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("TARGET_NOT_FOUND"));
        return true;
    }
    const FString NameString = GetJsonStringField(Payload, TEXT("name"));
    const FName Name(*NameString);
    auto Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("target"), Target.Scope);
    Result->SetStringField(TEXT("graphPath"), Target.Graph->GetPathName());
    if (Target.Component) Result->SetStringField(TEXT("componentPath"), Target.Component->GetPathName());
    if (Target.Scope == TEXT("graphInstance")) Result->SetStringField(TEXT("graphInstancePath"), Target.Interface->GetPathName());
    if (Action == TEXT("get_pcg_parameters"))
    {
        const FString Revision = GetPCGGraphRevision(Target.Graph);
        FPCGReadBudget ReadBudget;
        if (Revision.IsEmpty() || !ReadBudget.CheckParameters(Target.Interface->GetUserParametersStruct()))
        {
            Bridge->SendAutomationError(Socket, RequestId, TEXT("Complete parameter state exceeds the authored read budget or has invalid dependencies."), TEXT("PCG_SNAPSHOT_UNAVAILABLE"));
            return true;
        }
        const int32 Offset = FMath::Max(0, GetJsonIntField(Payload, TEXT("offset"), 0));
        const int32 Limit = FMath::Clamp(GetJsonIntField(Payload, TEXT("limit"), 50), 1, 100);
        int32 Total = 0;
        Result->SetArrayField(TEXT("parameters"), ReadPCGParameters(Target, NameString, Offset, Limit, &Total));
        Result->SetNumberField(TEXT("total"), Total);
        Result->SetNumberField(TEXT("offset"), Offset);
        Result->SetNumberField(TEXT("limit"), Limit);
        Result->SetBoolField(TEXT("truncated"), Offset + Limit < Total);
        Result->SetStringField(TEXT("revision"), Revision);
        Bridge->SendAutomationResponse(Socket, RequestId, true, TEXT("PCG parameters read."), Result);
        return true;
    }
    if (Name.IsNone() || !CheckPCGGraphRevision(Target.Graph, Payload, Error))
    {
        Bridge->SendAutomationError(Socket, RequestId, Name.IsNone() ? TEXT("A parameter name is required.") : Error,
            Name.IsNone() ? TEXT("INVALID_ARGUMENT") : TEXT("STALE_STATE"));
        return true;
    }
    const bool bSchema = Action == TEXT("add_pcg_parameter") || Action == TEXT("rename_pcg_parameter") || Action == TEXT("remove_pcg_parameter");
    if (bSchema && Target.Scope != TEXT("graph"))
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Parameter schema edits must target the graph, not an instance."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    UPCGGraphInstance* Instance = Cast<UPCGGraphInstance>(Target.Interface);
    const FInstancedPropertyBag* Current = Target.Interface->GetUserParametersStruct();
    const FPropertyBagPropertyDesc* Existing = Current->FindPropertyDescByName(Name);
    if ((Action == TEXT("add_pcg_parameter")) == (Existing != nullptr))
    {
        Bridge->SendAutomationError(Socket, RequestId, Existing ? TEXT("The parameter already exists.") : TEXT("The parameter does not exist."),
            Existing ? TEXT("PARAMETER_EXISTS") : TEXT("PARAMETER_NOT_FOUND"));
        return true;
    }
    FInstancedPropertyBag Staged;
    FPropertyBagPropertyDesc Desc;
    if (Action == TEXT("add_pcg_parameter"))
    {
        if (!BuildPCGParameterDesc(Payload, Desc, Error))
        {
            Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_PARAMETER_SCHEMA"));
            return true;
        }
        Staged = *Current;
        Staged.AddProperties({Desc});
        if (!Staged.FindPropertyDescByName(Name))
        {
            Bridge->SendAutomationError(Socket, RequestId, TEXT("The native property bag refused the parameter schema."), TEXT("INVALID_PARAMETER_SCHEMA"));
            return true;
        }
    }
    if ((Action == TEXT("set_pcg_parameter") || Action == TEXT("add_pcg_parameter")) && Payload->HasField(TEXT("value")))
    {
        FInstancedPropertyBag Values;
        if (!StagePCGParameterValue(Action == TEXT("add_pcg_parameter") ? Staged : *Current, Name, Payload->TryGetField(TEXT("value")), Values, Error))
        {
            Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_PARAMETER_VALUE"));
            return true;
        }
        Staged = MoveTemp(Values);
    }
    else if (Action == TEXT("set_pcg_parameter"))
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("set_pcg_parameter requires value, including explicit null for an empty object reference."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (Action == TEXT("reset_pcg_parameter_override") && !Instance)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Reset requires a graph instance or component target."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const FName NewName(*GetJsonStringField(Payload, TEXT("newName")));
    if (Action == TEXT("rename_pcg_parameter") && (NewName.IsNone() || Current->FindPropertyDescByName(NewName)))
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("newName must be nonempty and not already used."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (Action == TEXT("remove_pcg_parameter"))
    {
        const auto References = FindPCGParameterReferences(Target.Graph, Name, Existing->ID);
        if (!References.IsEmpty())
        {
            Result->SetArrayField(TEXT("references"), References);
            Result->SetBoolField(TEXT("loadedReferencesOnly"), true);
            Bridge->SendAutomationResponse(Socket, RequestId, false, TEXT("The parameter is referenced by loaded PCG nodes; no implicit cascade is permitted."), Result, TEXT("PARAMETER_IN_USE"));
            return true;
        }
    }
    FPCGReadBudget Budget;
    if (!Budget.CheckParameters(Current) || !Budget.CheckParameters(&Staged))
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Staged parameter values exceed the bounded authored-state budget."), TEXT("PCG_SNAPSHOT_UNAVAILABLE"));
        return true;
    }
    FPCGEditState EditorState(Target.Graph);
    auto CapturedObjects = EditorState.GetObjects();
    CapturedObjects.AddUnique(Target.Interface);
    if (Target.Component) CapturedObjects.AddUnique(Target.Component);
    for (TObjectIterator<UPCGGraphInstance> It; It; ++It) if (It->GetGraph() == Target.Graph) CapturedObjects.AddUnique(*It);
    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Edit PCG Parameters")),
        bSave ? EMcpMutationDurability::DurablePackageWrite : EMcpMutationDurability::EditorStateOnly, CapturedObjects);
    Target.Interface->Modify();
    if (Target.Component) Target.Component->Modify();
    bool bSuccess = true;
    if (Action == TEXT("add_pcg_parameter")) bSuccess = Target.Graph->AddUserParameters({Desc}) == EPropertyBagAlterationResult::Success;
    if (bSuccess && (Action == TEXT("set_pcg_parameter") || (Action == TEXT("add_pcg_parameter") && Payload->HasField(TEXT("value")))))
    {
        if (Instance) Instance->UpdatePropertyOverride(Existing->CachedProperty, true);
        bSuccess = CommitPCGParameterValue(Target.Interface, Name, Staged, Error);
    }
    if (Action == TEXT("rename_pcg_parameter")) bSuccess = Target.Graph->RenameUserParameter(Name, NewName) == EPropertyBagAlterationResult::Success;
    if (Action == TEXT("remove_pcg_parameter"))
    {
        bool bRemoved = false;
        Target.Graph->UpdateUserParametersStruct([Name, &bRemoved](FInstancedPropertyBag& Bag) { bRemoved = Bag.RemovePropertyByName(Name) == EPropertyBagAlterationResult::Success; });
        bSuccess = bRemoved;
    }
    if (Action == TEXT("reset_pcg_parameter_override"))
    {
        Instance->ResetPropertyToDefault(Existing->CachedProperty);
        Instance->UpdatePropertyOverride(Existing->CachedProperty, false);
    }
    Result->SetBoolField(TEXT("committed"), bSuccess);
    Result->SetBoolField(TEXT("saved"), false);
    Result->SetBoolField(TEXT("rolledBack"), false);
    Result->SetStringField(TEXT("rollbackScope"), TEXT("none_cross_instance_parameter_propagation"));
    if (!bSuccess)
    {
        Result->SetBoolField(TEXT("mayHaveCommitted"), true);
        Bridge->SendAutomationResponse(Socket, RequestId, false, Error.IsEmpty() ? TEXT("The native parameter API did not complete the edit; inspect target state.") : Error, Result, TEXT("PARAMETER_EDIT_FAILED"));
        return true;
    }
    Target.Interface->MarkPackageDirty();
    TSharedPtr<FJsonObject> EditorResult;
    bSuccess = SyncPCGEditorGraph(Target.Graph, EditorResult, Error);
    Result->SetObjectField(TEXT("editorGraph"), EditorResult);
    Result->SetStringField(TEXT("revision"), GetPCGGraphRevision(Target.Graph));
    Result->SetArrayField(TEXT("parameters"), ReadPCGParameters(Target));
    Result->SetBoolField(TEXT("loadedReferencesOnly"), true);
    Transaction.DescribeInto(Result);
    bool bSaved = false;
    if (bSuccess && bSave)
    {
        if (Target.Component) bSuccess = SaveEditorWorldIfRequested(Target.Component->GetWorld(), true, bSaved, Error);
        else bSuccess = bSaved = McpSafeAssetSave(Target.Interface);
    }
    Result->SetBoolField(TEXT("saved"), bSaved);
    Bridge->SendAutomationResponse(Socket, RequestId, bSuccess, bSuccess ? TEXT("PCG parameter edit committed.") : Error, Result,
        bSuccess ? FString() : TEXT("PARAMETER_POST_COMMIT_FAILED"));
    return true;
#endif
}
}
#endif
