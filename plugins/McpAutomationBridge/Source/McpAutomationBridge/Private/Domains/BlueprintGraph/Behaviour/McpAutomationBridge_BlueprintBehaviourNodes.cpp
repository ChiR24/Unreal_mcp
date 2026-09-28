#include "Domains/BlueprintGraph/Behaviour/McpAutomationBridge_BlueprintBehaviourNodes.h"

#include "BlueprintActionDatabase.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"
#include "Foundation/BridgeHelpers/Reflection/McpAutomationBridgeHelpersClassResolution.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "K2Node_BaseAsyncTask.h"
#include "K2Node_CallDelegate.h"
#include "K2Node_Message.h"

namespace McpBlueprintGraphHandlers
{
namespace
{
bool IsBehaviourKind(const FString& NodeType, std::initializer_list<const TCHAR*> Names)
{
    for (const TCHAR* Name : Names)
    {
        if (NodeType.Equals(Name, ESearchCase::IgnoreCase))
        {
            return true;
        }
    }
    return false;
}

bool IsCallDelegateKind(const FString& NodeType)
{
    return IsBehaviourKind(NodeType, {TEXT("CallDelegate"), TEXT("K2Node_CallDelegate")});
}

bool IsMessageKind(const FString& NodeType)
{
    return IsBehaviourKind(NodeType, {TEXT("Message"), TEXT("K2Node_Message")});
}

// The async node classes that are unusable without a factory function; AI MoveTo and
// the other nodes that fix their own factory keep the generic path.
bool IsAsyncTaskKind(const FString& NodeType)
{
    return IsBehaviourKind(NodeType, {TEXT("AsyncTask"), TEXT("AsyncAction"), TEXT("K2Node_AsyncAction"),
                                      TEXT("K2Node_LatentAbilityCall"), TEXT("K2Node_LatentGameplayTaskCall")});
}

UClass* ResolveBehaviourClass(const FString& Name)
{
    UClass* Class = Name.IsEmpty() ? nullptr : ResolveUClass(Name);
    return Class ? Class : ResolveTargetClassFromString(Name);
}

// A dispatcher on Owner, else this Blueprint's own (on the skeleton class, where a
// dispatcher added earlier in the same batch already is).
FMulticastDelegateProperty* FindBehaviourDispatcher(UBlueprint* Blueprint, const FString& Name, UClass* Owner)
{
    UClass* Skeleton = Blueprint->SkeletonGeneratedClass;
    UClass* Generated = Blueprint->GeneratedClass;
    for (UClass* Class : {Owner, Owner ? nullptr : Skeleton, Owner ? nullptr : Generated})
    {
        FMulticastDelegateProperty* Found = Class && !Name.IsEmpty()
            ? FindFProperty<FMulticastDelegateProperty>(Class, FName(*Name)) : nullptr;
        if (Found)
        {
            return Found;
        }
    }
    return nullptr;
}

// The spawner the editor's own palette uses for this factory. Several node classes
// can register one (a GameplayTask and an AbilityTask factory); the most derived wins.
UBlueprintFunctionNodeSpawner* FindAsyncSpawner(const UFunction* Factory)
{
    UBlueprintFunctionNodeSpawner* Best = nullptr;
    for (const auto& Entry : FBlueprintActionDatabase::Get().GetAllActions())
    {
        for (UBlueprintNodeSpawner* Raw : Entry.Value)
        {
            UBlueprintFunctionNodeSpawner* Spawner = Cast<UBlueprintFunctionNodeSpawner>(Raw);
            UClass* NodeClass = Spawner ? Spawner->NodeClass.Get() : nullptr;
            if (NodeClass && Spawner->GetFunction() == Factory &&
                NodeClass->IsChildOf(UK2Node_BaseAsyncTask::StaticClass()) &&
                (!Best || NodeClass->IsChildOf(Best->NodeClass.Get())))
            {
                Best = Spawner;
            }
        }
    }
    return Best;
}

bool CreateAsyncTaskNode(FActionContext& Context, float X, float Y)
{
    const FString Member = GetJsonStringField(Context.Payload, TEXT("memberName"));
    const FString MemberClass = McpGetFirstStringField(Context.Payload, {TEXT("memberClass"), TEXT("targetClass")});
    UClass* Owner = ResolveBehaviourClass(MemberClass);
    UFunction* Factory = Owner ? Owner->FindFunctionByName(FName(*Member)) : nullptr;
    if (!Factory || !Factory->HasAnyFunctionFlags(FUNC_Static))
    {
        Context.SendError(FString::Printf(TEXT("AsyncTask needs memberClass (the task or async-action class, e.g. "
                                               "AbilityTask_WaitDelay) and memberName (its static factory, e.g. "
                                               "WaitDelay); '%s' on '%s' is not one."), *Member, *MemberClass),
                          TEXT("FUNCTION_NOT_FOUND"));
        return true;
    }
    if (Context.Blueprint->FunctionGraphs.Contains(Context.TargetGraph))
    {
        Context.SendError(TEXT("A latent task node cannot live in a function graph; put it in an event graph."),
                          TEXT("LATENT_NODE_IN_FUNCTION"));
        return true;
    }
    UBlueprintFunctionNodeSpawner* Spawner = FindAsyncSpawner(Factory);
    if (!Spawner)
    {
        FBlueprintActionDatabase::Get().RefreshClassActions(Owner);
        Spawner = FindAsyncSpawner(Factory);
    }
    UEdGraphNode* Node = Spawner ? Spawner->Invoke(Context.TargetGraph, IBlueprintNodeBinder::FBindingSet(), FVector2D(X, Y))
                                 : nullptr;
    if (!Node)
    {
        Context.SendError(FString::Printf(TEXT("No latent node is registered for %s.%s: the editor module that provides "
                                               "its node (GameplayAbilities editor for ability tasks) is not loaded."),
                                          *Owner->GetName(), *Member),
                          TEXT("ASYNC_NODE_NOT_AVAILABLE"));
        return true;
    }
    FString OverlapMessage;
    TSharedPtr<FJsonObject> OverlapDetails;
    if (McpGraphLayout::RefuseOverlappingNode(Context.TargetGraph, Node, X, Y, OverlapMessage, OverlapDetails))
    {
        Context.SendErrorWithDetails(OverlapMessage, TEXT("NODE_OVERLAP"), OverlapDetails);
        return true;
    }
    FBlueprintEditorUtils::MarkBlueprintAsModified(Context.Blueprint);
    SaveLoadedAssetThrottled(Context.Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeGuid"), Node->NodeGuid.ToString());
    Result->SetStringField(TEXT("nodeId"), Node->NodeGuid.ToString());
    Result->SetStringField(TEXT("nodeName"), Node->GetName());
    Result->SetStringField(TEXT("nodeClass"), Node->GetClass()->GetName());
    McpGraphLayout::AddNodePlacementFields(Result, *Node);
    McpHandlerUtils::AddVerification(Result, Context.Blueprint);
    Context.SendResponse(TEXT("Async task node created."), Result);
    return true;
}
} // namespace

bool TryCreateBehaviourNode(FActionContext& Context, const FString& NodeType, float X, float Y)
{
    if (IsAsyncTaskKind(NodeType))
    {
        return CreateAsyncTaskNode(Context, X, Y);
    }
    const FString Member = GetJsonStringField(Context.Payload, TEXT("memberName"));
    const FString MemberClass = McpGetFirstStringField(Context.Payload, {TEXT("memberClass"), TEXT("targetClass")});
    if (IsCallDelegateKind(NodeType))
    {
        UClass* Owner = MemberClass.IsEmpty() ? nullptr : ResolveBehaviourClass(MemberClass);
        FMulticastDelegateProperty* Dispatcher = FindBehaviourDispatcher(Context.Blueprint, Member, Owner);
        if (!Dispatcher || (!MemberClass.IsEmpty() && !Owner))
        {
            Context.SendError(FString::Printf(TEXT("No event dispatcher '%s' on %s. Declare it first (build_graph step "
                                                   "add_event_dispatcher), or name its class in memberClass."),
                                              *Member, MemberClass.IsEmpty() ? TEXT("this Blueprint") : *MemberClass),
                              TEXT("DISPATCHER_NOT_FOUND"));
            return true;
        }
        FGraphNodeCreator<UK2Node_CallDelegate> Creator(*Context.TargetGraph);
        UK2Node_CallDelegate* Node = Creator.CreateNode(false);
        Node->SetFromProperty(Dispatcher, /*bSelfContext=*/Owner == nullptr, Dispatcher->GetOwnerClass());
        Context.FinalizeNode(Creator, Node, X, Y);
        return true;
    }
    if (!IsMessageKind(NodeType))
    {
        return false;
    }
    UClass* Interface = ResolveBehaviourClass(MemberClass);
    UFunction* Function = Interface && Interface->HasAnyClassFlags(CLASS_Interface)
        ? Interface->FindFunctionByName(FName(*Member)) : nullptr;
    if (!Function)
    {
        Context.SendError(FString::Printf(TEXT("A Message node needs memberClass naming an interface (a Blueprint "
                                               "Interface asset path or a native interface) and memberName naming one "
                                               "of its functions; '%s' on '%s' is not one."), *Member, *MemberClass),
                          TEXT("FUNCTION_NOT_FOUND"));
        return true;
    }
    FGraphNodeCreator<UK2Node_Message> Creator(*Context.TargetGraph);
    UK2Node_Message* Node = Creator.CreateNode(false);
    Node->SetFromFunction(Function);
    Context.FinalizeNode(Creator, Node, X, Y);
    return true;
}

FString PrecheckBehaviourNode(const FActionContext& Context, const FString& NodeType, const FString& Member,
                              const FString& MemberClass, const TSet<FName>& Declared, FString& OutCode)
{
    if (IsCallDelegateKind(NodeType))
    {
        UClass* Owner = MemberClass.IsEmpty() ? nullptr : ResolveBehaviourClass(MemberClass);
        if ((MemberClass.IsEmpty() && Declared.Contains(FName(*Member))) ||
            ((MemberClass.IsEmpty() || Owner) && FindBehaviourDispatcher(Context.Blueprint, Member, Owner)))
        {
            return FString();
        }
        OutCode = TEXT("DISPATCHER_NOT_FOUND");
        return FString::Printf(TEXT("Event dispatcher '%s' not found, and no earlier add_event_dispatcher step "
                                    "declares it."), *Member);
    }
    if (!IsAsyncTaskKind(NodeType) && !IsMessageKind(NodeType))
    {
        return FString();
    }
    UClass* Owner = ResolveBehaviourClass(MemberClass);
    if (Owner && Owner->FindFunctionByName(FName(*Member)))
    {
        return FString();
    }
    OutCode = TEXT("FUNCTION_NOT_FOUND");
    return FString::Printf(TEXT("%s step: memberClass '%s' has no function '%s'."), *NodeType, *MemberClass, *Member);
}
} // namespace McpBlueprintGraphHandlers
