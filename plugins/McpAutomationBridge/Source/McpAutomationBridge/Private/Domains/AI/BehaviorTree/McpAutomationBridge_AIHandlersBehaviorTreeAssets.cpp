#include "Domains/AI/McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "Domains/BehaviorTree/McpAutomationBridge_BehaviorTreeHandlersPrivate.h"
#include "Misc/PackageName.h"

namespace
{
// Depth-first search for a composite by object name, path or display name.
UBTCompositeNode* FindCompositeById(UBTCompositeNode* Node, const FString& Id)
{
    if (!Node || Id.IsEmpty())
    {
        return nullptr;
    }
    if (Node->GetName().Equals(Id, ESearchCase::IgnoreCase) ||
        Node->GetPathName().Equals(Id, ESearchCase::IgnoreCase) ||
        Node->GetNodeName().Equals(Id, ESearchCase::IgnoreCase))
    {
        return Node;
    }
    for (const FBTCompositeChild& Child : Node->Children)
    {
        if (UBTCompositeNode* Found = FindCompositeById(Child.ChildComposite, Id))
        {
            return Found;
        }
    }
    return nullptr;
}
}

namespace McpAIHandlers
{
static UBehaviorTree* CreateBehaviorTreeAsset(const FString& Path, const FString& Name, FString& OutError)
{
    UBehaviorTree* BehaviorTree = CreateAIAssetInPackage<UBehaviorTree>(
        Path, Name, TEXT("Behavior Tree"), OutError);
    if (!BehaviorTree)
    {
        return nullptr;
    }

    UEdGraph* Graph = nullptr;
    McpBehaviorTreeHandlers::EnsureBehaviorTreeGraph(BehaviorTree, Graph);
    McpSafeAssetSave(BehaviorTree);

    return BehaviorTree;
}

// Implements the "create_behavior_tree" action.
bool HandleCreateBehaviorTree(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    // Accept savePath as documented on the capability (the schema advertises
    // "Directory path used when saving the created Behavior Tree"), and fall
    // back to path for callers that used the shorter form. Previously only
    // `path` was read, so a savePath-only call silently landed in the default
    // /Game/AI/BehaviorTrees folder while reporting success.
    FString Path = GetJsonStringField(Payload, TEXT("savePath"));
    if (Path.IsEmpty())
    {
        Path = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game/AI/BehaviorTrees"));
    }

    if (Name.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Missing name parameter"),
                            TEXT("INVALID_PARAMS"));
        return true;
    }

    FString Error;
    UBehaviorTree* BT = CreateBehaviorTreeAsset(Path, Name, Error);
    if (!BT)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
        return true;
    }

    Result->SetStringField(TEXT("behaviorTreePath"), BT->GetPathName());
    Result->SetStringField(TEXT("packagePath"), BT->GetOutermost()->GetName()); // dogfood #66
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Created Behavior Tree: %s"), *Name));
    McpHandlerUtils::AddVerification(Result, BT);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Behavior Tree created"), Result);
    return true;
}

// Implements the "add_composite_node" action.
bool HandleAddCompositeNode(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString BTPath = GetJsonStringField(Payload, TEXT("behaviorTreePath"));
    FString CompositeType = GetJsonStringField(Payload, TEXT("compositeType"));
    // Composite nodes were created unnamed and the response carried no
    // identifier, so a caller had nothing to hand to add_decorator /
    // add_service / add_task_node afterwards — the tree could be created
    // but never assembled.
    const FString RequestedNodeName = GetJsonStringField(Payload, TEXT("nodeName"));

    UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *BTPath);
    if (!BT)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Behavior Tree not found: %s"), *BTPath),
                            TEXT("NOT_FOUND"));
        return true;
    }

    UBTCompositeNode* NewNode = nullptr;
    if (CompositeType.Equals(TEXT("Selector"), ESearchCase::IgnoreCase))
    {
        NewNode = NewObject<UBTComposite_Selector>(BT);
    }
    else if (CompositeType.Equals(TEXT("Sequence"), ESearchCase::IgnoreCase))
    {
        NewNode = NewObject<UBTComposite_Sequence>(BT);
    }
    // Add more composite types as needed

    if (NewNode)
    {
#if WITH_EDITORONLY_DATA
        if (!RequestedNodeName.IsEmpty())
        {
            NewNode->NodeName = RequestedNodeName;
        }
#endif
        // Nest under the requested parent composite, else under the root; a tree
        // with no root adopts the node as root (dogfood #57/#58).
        const FString ParentNodeId = GetJsonStringField(Payload, TEXT("parentNodeId"));
        UBTCompositeNode* Parent = ParentNodeId.IsEmpty() ? nullptr : FindCompositeById(BT->RootNode, ParentNodeId);
        if (!ParentNodeId.IsEmpty() && !Parent)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Parent composite not found: %s"), *ParentNodeId),
                                TEXT("PARENT_NOT_FOUND"));
            return true;
        }
        if (!Parent && !BT->RootNode)
        {
            BT->RootNode = NewNode;
        }
        else
        {
            FBTCompositeChild Child;
            Child.ChildComposite = NewNode;
            (Parent ? Parent : BT->RootNode.Get())->Children.Add(Child);
        }
        BT->MarkPackageDirty();
        McpSafeAssetSave(BT);

        // Return an addressable identifier so the node can be referenced by
        // the decorator/service/task actions that follow.
        Result->SetStringField(TEXT("nodeName"), NewNode->GetNodeName());
        Result->SetStringField(TEXT("nodeId"), NewNode->GetName());
        Result->SetBoolField(TEXT("isRoot"), BT->RootNode == NewNode);
        Result->SetStringField(TEXT("compositeType"), CompositeType);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added %s node"), *CompositeType));
        McpHandlerUtils::AddVerification(Result, BT);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Composite node added"), Result);
    }
    else
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Failed to create composite node: %s"), *CompositeType),
                            TEXT("CREATION_FAILED"));
    }

    return true;
}

bool HandleAddTaskNode(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString BTPath = GetJsonStringField(Payload, TEXT("behaviorTreePath"));
    FString TaskType = GetJsonStringField(Payload, TEXT("taskType"));

    UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *BTPath);
    if (!BT)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Behavior Tree not found: %s"), *BTPath),
                            TEXT("NOT_FOUND"));
        return true;
    }

    // Map task type strings to actual UE classes via template or runtime lookup
    // Support both short names ("MoveTo") and full class names ("MoveToTask")
    UBTTaskNode* NewTask = nullptr;
    UClass* TaskClass = nullptr;

    // Template-based classes: use StaticClass() for known task types
    if (TaskType.Equals(TEXT("MoveTo"), ESearchCase::IgnoreCase) ||
        TaskType.Equals(TEXT("MoveToTask"), ESearchCase::IgnoreCase))
    {
        NewTask = NewObject<UBTTask_MoveTo>(BT);
    }
    else if (TaskType.Equals(TEXT("Wait"), ESearchCase::IgnoreCase) ||
             TaskType.Equals(TEXT("WaitTask"), ESearchCase::IgnoreCase))
    {
        NewTask = NewObject<UBTTask_Wait>(BT);
    }
    else
    {
        // Fallback: try runtime class lookup
        TaskClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/AIModule.%s"), *TaskType));
        if (!TaskClass)
        {
            // Try with "BTTask_" prefix
            TaskClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/AIModule.BTTask_%s"), *TaskType));
        }
        if (TaskClass)
        {
            UObject* TaskObj = NewObject<UObject>(BT, TaskClass);
            if (TaskObj && TaskObj->GetClass()->IsChildOf(UBTTaskNode::StaticClass()))
            {
                NewTask = static_cast<UBTTaskNode*>(TaskObj);
            }
        }
    }

    if (NewTask)
    {
        // Resolve the attach point BEFORE touching the editor graph. Creating a
        // BehaviorTree graph for an asset that has no root composite drives
        // BehaviorTreeEditor into an empty-array dereference (dogfood #63), so the
        // root/parent guard must come first and the graph is built only on success.
        const FString ParentNodeId = GetJsonStringField(Payload, TEXT("parentNodeId"));
        UBTCompositeNode* Parent = ParentNodeId.IsEmpty() ? BT->RootNode.Get() : FindCompositeById(BT->RootNode, ParentNodeId);
        if (!Parent)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                                ParentNodeId.IsEmpty()
                                    ? FString(TEXT("Behavior tree has no root composite; add_composite first"))
                                    : FString::Printf(TEXT("Parent composite not found: %s"), *ParentNodeId),
                                ParentNodeId.IsEmpty() ? TEXT("NO_ROOT") : TEXT("PARENT_NOT_FOUND"));
            return true;
        }
        UEdGraph* Graph = nullptr;
        McpBehaviorTreeHandlers::EnsureBehaviorTreeGraph(BT, Graph);
        FBTCompositeChild Child;
        Child.ChildTask = NewTask;
        Parent->Children.Add(Child);
        BT->MarkPackageDirty();
        McpSafeAssetSave(BT);
        Result->SetStringField(TEXT("nodeId"), NewTask->GetName());
        Result->SetStringField(TEXT("taskType"), TaskType);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added %s task"), *TaskType));
        McpHandlerUtils::AddVerification(Result, BT);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Task node added"), Result);
    }
    else
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Failed to create task node: %s"), *TaskType),
                            TEXT("CREATION_FAILED"));
    }

    return true;
}
}
#endif
