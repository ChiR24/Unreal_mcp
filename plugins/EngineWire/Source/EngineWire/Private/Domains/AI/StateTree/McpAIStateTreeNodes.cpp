// edit_state_tree add_node: a condition, task, evaluator or consideration added to a State Tree by its node struct, in
// the list it belongs to: a state's enter conditions, tasks or considerations, a transition's conditions, or the tree's
// evaluators and global tasks. The node struct is made in the editor node's FInstancedStruct, which needs that type
// exported from CoreUObject (detected: MCP_HAS_CORE_INSTANCED_STRUCT); before it, StructUtils held it and this refuses.

#include "Domains/AI/McpAIContext.h"

#include "Domains/AI/StateTree/McpAIStateTreeFeature.h"
#include "Dom/JsonValue.h"

#if MCP_HAS_STATE_TREE && MCP_STATE_TREE_HEADERS_AVAILABLE && MCP_HAS_CORE_INSTANCED_STRUCT
#include "StateTreeNodeBase.h"
#include "UObject/UObjectIterator.h"

namespace McpAIStateTreeNodesDetail
{
// The node structs a list takes derive from this base, found by path so no StateTree header is needed for it.
const UScriptStruct* BaseOf(const FString& List)
{
    const TCHAR* Name = List.EndsWith(TEXT("condition")) ? TEXT("StateTreeConditionBase")
        : List == TEXT("evaluator") ? TEXT("StateTreeEvaluatorBase")
        : List == TEXT("consideration") ? TEXT("StateTreeConsiderationBase") : TEXT("StateTreeTaskBase");
    return FindObject<UScriptStruct>(nullptr, *(FString(TEXT("/Script/StateTreeModule.")) + Name));
}

// The node struct Type names (a path, or a name with or without its F) deriving from Base; null with OutError naming
// the near ones.
const UScriptStruct* FindNodeType(const FString& Type, const UScriptStruct* Base, FString& OutError)
{
    const FString Bare = Type.StartsWith(TEXT("F")) && Type.Len() > 1 && FChar::IsUpper(Type[1]) ? Type.Mid(1) : Type;
    const UScriptStruct* Found = Type.StartsWith(TEXT("/")) ? FindObject<UScriptStruct>(nullptr, *Type) : nullptr;
    TArray<FString> Near;
    for (TObjectIterator<UScriptStruct> It; It && !Found; ++It)
    {
        if (!It->IsChildOf(Base) || *It == Base)
        {
            continue;
        }
        if (It->GetName().Equals(Bare, ESearchCase::IgnoreCase))
        {
            Found = *It;
        }
        else if (Near.Num() < 20 && (Bare.IsEmpty() || It->GetName().Contains(Bare)))
        {
            Near.Add(It->GetName());
        }
    }
    if (!Found || !Found->IsChildOf(Base))
    {
        OutError = FString::Printf(TEXT("'%s' is no %s struct. %s"), *Type, *Base->GetName(),
            Near.Num() > 0 ? *(TEXT("Near: ") + FString::Join(Near, TEXT(", "))) : TEXT("nodeType names one by its struct name, such as StateTreeCompareIntCondition."));
        return nullptr;
    }
    return Found;
}

// The list the node goes in; null with OutError when the state or transition it needs is missing.
TArray<FStateTreeEditorNode>* ListOf(UStateTreeEditorData* Data, UStateTreeState* State, const FString& List, int32 Transition, FString& OutError)
{
    if (List == TEXT("evaluator")) return &Data->Evaluators;
    if (List == TEXT("global_task")) return &Data->GlobalTasks;
    if (!State)
    {
        OutError = TEXT("stateName is required for this list (only evaluator and global_task belong to the tree)");
        return nullptr;
    }
    if (List == TEXT("enter_condition")) return &State->EnterConditions;
    if (List == TEXT("task")) return &State->Tasks;
#if MCP_HAS_STATE_TREE_CONSIDERATIONS
    if (List == TEXT("consideration")) return &State->Considerations;
#endif
    if (List == TEXT("transition_condition") && State->Transitions.IsValidIndex(Transition)) return &State->Transitions[Transition].Conditions;
    OutError = List == TEXT("transition_condition")
        ? FString::Printf(TEXT("transitionIndex %d is not one of the state's %d transitions"), Transition, State->Transitions.Num())
        : FString::Printf(TEXT("'%s' is no list here: enter_condition, task, consideration, transition_condition, evaluator or global_task"), *List);
    return nullptr;
}
} // namespace McpAIStateTreeNodesDetail
#endif

namespace McpAIHandlers
{
bool HandleAddStateTreeNode(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if MCP_HAS_STATE_TREE && MCP_STATE_TREE_HEADERS_AVAILABLE && MCP_HAS_CORE_INSTANCED_STRUCT
    using namespace McpAIStateTreeNodesDetail;
    const FString List = GetJsonStringField(Payload, TEXT("list")).ToLower();
    const FString StateName = GetJsonStringField(Payload, TEXT("stateName"));
    UStateTree* Tree = nullptr;
    UStateTreeEditorData* Data = McpLoadStateTreeEditorDataOrReply(Self, Socket, RequestId, GetJsonStringField(Payload, TEXT("stateTreePath")), Tree);
    if (!Data)
    {
        return true;
    }
    UStateTreeState* State = StateName.IsEmpty() ? nullptr : McpFindStateTreeState(Data, StateName);
    FString Error;
    TArray<FStateTreeEditorNode>* Nodes = !StateName.IsEmpty() && !State ? nullptr
        : ListOf(Data, State, List, GetJsonIntField(Payload, TEXT("transitionIndex"), 0), Error);
    const UScriptStruct* Base = Nodes ? BaseOf(List) : nullptr;
    const UScriptStruct* Type = Base ? FindNodeType(GetJsonStringField(Payload, TEXT("nodeType")), Base, Error) : nullptr;
    if (!Type)
    {
        Self->SendAutomationError(Socket, RequestId, !StateName.IsEmpty() && !State ? FString::Printf(TEXT("State '%s' not found"), *StateName)
            : Error.IsEmpty() ? FString(TEXT("This engine's StateTree has no base struct for that list")) : Error,
            !StateName.IsEmpty() && !State ? TEXT("NOT_FOUND") : TEXT("INVALID_ARGUMENT"));
        return true;
    }
    (State ? static_cast<UObject*>(State) : Data)->Modify();
    FStateTreeEditorNode& Node = Nodes->AddDefaulted_GetRef();
    Node.ID = FGuid::NewGuid();
    Node.Node.InitializeAs(Type);
    // The node's instance data: a struct beside it, or an object of a Blueprint node's class.
    const UStruct* InstanceType = Node.Node.GetPtr<FStateTreeNodeBase>()->GetInstanceDataType();
    if (const UScriptStruct* InstanceStruct = Cast<UScriptStruct>(InstanceType))
    {
        Node.Instance.InitializeAs(InstanceStruct);
    }
    else if (const UClass* InstanceClass = Cast<UClass>(InstanceType))
    {
        Node.InstanceObject = NewObject<UObject>(Data, InstanceClass, NAME_None, RF_Transactional);
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    const TSharedPtr<FJsonObject>* Properties = nullptr;
    if (Payload->TryGetObjectField(TEXT("properties"), Properties) && Properties && (*Properties)->Values.Num() > 0)
    {
        TArray<FString> Applied, Failed, Available;
        ApplyStateTreeTaskProperties(Node, *Properties, Applied, Failed, Available);
        Result->SetArrayField(TEXT("applied"), McpHandlerUtils::ToJsonStringArray(Applied));
        Result->SetArrayField(TEXT("failed"), McpHandlerUtils::ToJsonStringArray(Failed));
    }
    McpCompileStateTreeInto(Tree, Result);
    Result->SetStringField(TEXT("list"), List);
    Result->SetStringField(TEXT("nodeType"), Type->GetName());
    Result->SetStringField(TEXT("nodeId"), Node.ID.ToString());
    Result->SetNumberField(TEXT("nodeCount"), Nodes->Num());
    Result->SetBoolField(TEXT("saved"), McpSafeAssetSave(Tree));
    McpHandlerUtils::AddVerification(Result, Tree);
    Self->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Added %s to %s"), *Type->GetName(), *List), Result);
#else
    Self->SendAutomationError(Socket, RequestId, TEXT("Adding State Tree nodes needs a State Tree and FInstancedStruct in CoreUObject (UE 5.5+); this build has neither or not both."), TEXT("FEATURE_UNAVAILABLE"));
#endif
    return true;
}
} // namespace McpAIHandlers
