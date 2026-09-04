#include "Domains/AI/McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
#include "Domains/AI/StateTree/McpAutomationBridge_AIStateTreeFeature.h"
#include "Dom/JsonValue.h"

#if MCP_HAS_STATE_TREE && MCP_STATE_TREE_HEADERS_AVAILABLE
#include "StateTreeNodeBase.h"
#include MCP_INSTANCED_STRUCT_HEADER
#endif

namespace McpAIHandlers
{
#if MCP_HAS_STATE_TREE && MCP_STATE_TREE_HEADERS_AVAILABLE
namespace
{
UStateTreeState* FindStateTreeStateByName(UStateTreeState* State, const FString& Name)
{
    if (!State)
    {
        return nullptr;
    }
    if (State->Name.ToString().Equals(Name, ESearchCase::IgnoreCase))
    {
        return State;
    }
    for (UStateTreeState* Child : State->Children)
    {
        if (UStateTreeState* Found = FindStateTreeStateByName(Child, Name))
        {
            return Found;
        }
    }
    return nullptr;
}

FString StateTreeTaskName(const FStateTreeEditorNode& Task)
{
    if (const FStateTreeNodeBase* Node = Task.Node.GetPtr<FStateTreeNodeBase>())
    {
        if (!Node->Name.IsNone())
        {
            return Node->Name.ToString();
        }
    }
    if (Task.InstanceObject)
    {
        return Task.InstanceObject->GetClass()->GetName();
    }
    return Task.Node.IsValid() ? Task.Node.GetScriptStruct()->GetName() : FString(TEXT("None"));
}

// Properties land on the task's Blueprint instance object, its instance-data
// struct and finally the node struct itself, so a name is matched wherever the
// task exposes it. Available collects every property name seen for the
// PROPERTY_NOT_FOUND hint.
int32 ApplyStateTreeTaskProperties(FStateTreeEditorNode& Task, const TSharedPtr<FJsonObject>& Properties,
    TArray<FString>& Applied, TArray<FString>& Failed, TArray<FString>& Available)
{
    int32 Count = 0;
    if (UObject* InstanceObject = Task.InstanceObject)
    {
        Count += ApplyAIJsonProperties(InstanceObject->GetClass(), InstanceObject, Properties, Applied, Failed);
        ListAIPropertyNames(InstanceObject->GetClass(), Available);
    }
    if (Task.Instance.IsValid())
    {
        Count += ApplyAIJsonProperties(Task.Instance.GetScriptStruct(), Task.Instance.GetMutableMemory(), Properties, Applied, Failed);
        ListAIPropertyNames(Task.Instance.GetScriptStruct(), Available);
    }
    if (Task.Node.IsValid())
    {
        Count += ApplyAIJsonProperties(Task.Node.GetScriptStruct(), Task.Node.GetMutableMemory(), Properties, Applied, Failed);
        ListAIPropertyNames(Task.Node.GetScriptStruct(), Available);
    }
    return Count;
}

TArray<TSharedPtr<FJsonValue>> ToJsonStringArray(const TArray<FString>& Values)
{
    TArray<TSharedPtr<FJsonValue>> Out;
    for (const FString& Value : Values)
    {
        Out.Add(MakeShared<FJsonValueString>(Value));
    }
    return Out;
}
}
#endif

bool HandleConfigureStateTreeTask(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
#if MCP_HAS_STATE_TREE && MCP_STATE_TREE_HEADERS_AVAILABLE
    const FString StateTreePath = GetJsonStringField(Payload, TEXT("stateTreePath"));
    const FString StateName = GetJsonStringField(Payload, TEXT("stateName"));
    if (StateTreePath.IsEmpty() || StateName.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("stateTreePath and stateName are required"), TEXT("INVALID_PARAMS"));
        return true;
    }

    // Nothing to apply means nothing changes, and that must not read as an update.
    const TSharedPtr<FJsonObject>* PropertiesPtr = nullptr;
    const bool bHasProperties = Payload->TryGetObjectField(TEXT("properties"), PropertiesPtr)
        && PropertiesPtr && PropertiesPtr->IsValid() && (*PropertiesPtr)->Values.Num() > 0;
    const FString SelectionBehavior = GetJsonStringField(Payload, TEXT("selectionBehavior"));
    if (!bHasProperties && SelectionBehavior.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            TEXT("No configurable fields supplied (accepted: properties{...}, taskName/taskIndex, selectionBehavior)"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UStateTree* StateTree = LoadObject<UStateTree>(nullptr, *StateTreePath);
    if (!StateTree)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("StateTree not found: %s"), *StateTreePath), TEXT("NOT_FOUND"));
        return true;
    }

    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("StateTree has no EditorData"), TEXT("INVALID_STATE"));
        return true;
    }

    UStateTreeState* FoundState = nullptr;
    for (UStateTreeState* SubTree : EditorData->SubTrees)
    {
        FoundState = FindStateTreeStateByName(SubTree, StateName);
        if (FoundState)
        {
            break;
        }
    }
    if (!FoundState)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("State '%s' not found"), *StateName), TEXT("NOT_FOUND"));
        return true;
    }

    TArray<FString> Applied;
    TArray<FString> Failed;
    if (!SelectionBehavior.IsEmpty())
    {
        // Reflection keeps this valid on every engine version that exposes the
        // enum; unknown names fail here instead of being silently ignored.
        FProperty* SelectionProp = UStateTreeState::StaticClass()->FindPropertyByName(TEXT("SelectionBehavior"));
        FString Error = TEXT("SelectionBehavior is not exposed on this engine version");
        if (!SelectionProp || !ApplyJsonValueToProperty(FoundState, SelectionProp, MakeShared<FJsonValueString>(SelectionBehavior), Error))
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Cannot apply selectionBehavior '%s': %s"), *SelectionBehavior, *Error), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        Applied.Add(TEXT("selectionBehavior"));
    }

    if (bHasProperties)
    {
        const FString TaskName = GetJsonStringField(Payload, TEXT("taskName"));
        int32 TaskIndex = static_cast<int32>(GetJsonNumberField(Payload, TEXT("taskIndex"), 0));
        if (!TaskName.IsEmpty())
        {
            TaskIndex = INDEX_NONE;
            for (int32 Index = 0; Index < FoundState->Tasks.Num(); ++Index)
            {
                if (StateTreeTaskName(FoundState->Tasks[Index]).Equals(TaskName, ESearchCase::IgnoreCase))
                {
                    TaskIndex = Index;
                    break;
                }
            }
        }
        if (!FoundState->Tasks.IsValidIndex(TaskIndex))
        {
            const FString TaskLabel = TaskName.IsEmpty() ? FString::FromInt(TaskIndex) : TaskName;
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Task '%s' not found on state '%s' (%d task(s); pass taskName or taskIndex)"),
                    *TaskLabel, *StateName, FoundState->Tasks.Num()),
                TEXT("NOT_FOUND"));
            return true;
        }

        FStateTreeEditorNode& Task = FoundState->Tasks[TaskIndex];
        TArray<FString> Available;
        const int32 Written = ApplyStateTreeTaskProperties(Task, *PropertiesPtr, Applied, Failed, Available);
        Result->SetStringField(TEXT("taskName"), StateTreeTaskName(Task));
        Result->SetNumberField(TEXT("taskIndex"), TaskIndex);
        if (Written == 0)
        {
            const FString FailedSuffix = Failed.Num() > 0 ? TEXT("; failed: ") + FString::Join(Failed, TEXT("; ")) : FString();
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("No supplied property exists on task '%s' (available: %s)%s"),
                    *StateTreeTaskName(Task), *FString::Join(Available, TEXT(", ")), *FailedSuffix),
                TEXT("PROPERTY_NOT_FOUND"));
            return true;
        }
    }

    const bool bSaved = McpSafeAssetSave(StateTree);

    Result->SetStringField(TEXT("assetPath"), StateTree->GetPathName());
    Result->SetStringField(TEXT("packagePath"), StateTree->GetOutermost()->GetName()); // dogfood #66
    Result->SetStringField(TEXT("stateName"), StateName);
    Result->SetNumberField(TEXT("taskCount"), FoundState->Tasks.Num());
    Result->SetArrayField(TEXT("applied"), ToJsonStringArray(Applied));
    Result->SetArrayField(TEXT("failed"), ToJsonStringArray(Failed));
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("State task updated: %d field(s) applied"), Applied.Num()));
    Result->SetStringField(TEXT("note"), TEXT("Edits target the StateTree editor data; recompile the asset in the StateTree editor to bake them into the runtime tree."));
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("State task updated"), Result);
#elif MCP_HAS_STATE_TREE
    Self->SendAutomationError(RequestingSocket, RequestId,
        TEXT("StateTree headers are unavailable in this build; enable the StateTree plugin"),
        TEXT("STATE_TREE_NOT_AVAILABLE"));
#else
    Self->SendAutomationError(RequestingSocket, RequestId,
                        TEXT("State Trees require UE 5.3+"),
                        TEXT("UNSUPPORTED_VERSION"));
#endif
    return true;
}
}
#endif
