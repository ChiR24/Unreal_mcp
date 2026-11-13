#include "Domains/AI/McpAIContext.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTNode.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BTTaskNode.h"
#include "Domains/BehaviorTree/McpBTPrivate.h"
#include "Foundation/McpScopedEditorTransaction.h"

namespace McpAIHandlers
{
// Implements the "configure_bt_node" action.
bool HandleConfigureBehaviorTreeNode(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString BTPath = GetJsonStringField(Payload, TEXT("behaviorTreePath"));
    FString NodeId = GetJsonStringField(Payload, TEXT("nodeId"));

    if (NodeId.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Missing nodeId parameter"),
                            TEXT("INVALID_PARAMS"));
        return true;
    }

    UBehaviorTree* BT = LoadBehaviorTreeOrReply(Self, RequestId, RequestingSocket, BTPath);
    if (!BT)
    {
        return true;
    }

    UBTNode* TargetNode = nullptr;
    FString ResolvedNodeRole;

    TFunction<void(UBTCompositeNode*)> VisitComposite;
    VisitComposite = [&](UBTCompositeNode* Composite)
    {
        if (!Composite || TargetNode)
        {
            return;
        }

        if (McpBTNodeMatchesId(Composite, NodeId))
        {
            TargetNode = Composite;
            ResolvedNodeRole = TEXT("composite");
            return;
        }

        for (UBTService* Service : Composite->Services)
        {
            if (McpBTNodeMatchesId(Service, NodeId))
            {
                TargetNode = Service;
                ResolvedNodeRole = TEXT("service");
                return;
            }
        }

        for (const FBTCompositeChild& Child : Composite->Children)
        {
            if (McpBTNodeMatchesId(Child.ChildTask, NodeId))
            {
                TargetNode = Child.ChildTask;
                ResolvedNodeRole = TEXT("task");
                return;
            }

            for (UBTDecorator* Decorator : Child.Decorators)
            {
                if (McpBTNodeMatchesId(Decorator, NodeId))
                {
                    TargetNode = Decorator;
                    ResolvedNodeRole = TEXT("decorator");
                    return;
                }
            }

            if (Child.ChildComposite)
            {
                VisitComposite(Child.ChildComposite);
                if (TargetNode)
                {
                    return;
                }
            }
        }
    };

    // get_tree numbers nodes by DFS index (root decorators first, then node, services,
    // children, entry decorators); accept those ids here as well.
    if (!NodeId.IsEmpty() && NodeId.IsNumeric())
    {
        TArray<UBTNode*> Ordered;
        TFunction<void(UBTNode*, const TArray<TObjectPtr<UBTDecorator>>*)> Walk;
        Walk = [&](UBTNode* Node, const TArray<TObjectPtr<UBTDecorator>>* EntryDecorators)
        {
            if (!Node) { return; }
            Ordered.Add(Node);
            if (UBTCompositeNode* Composite = Cast<UBTCompositeNode>(Node))
            {
                for (UBTService* Service : Composite->Services) { if (Service) { Ordered.Add(Service); } }
                for (const FBTCompositeChild& Child : Composite->Children)
                {
                    if (Child.ChildComposite) { Walk(Child.ChildComposite, &Child.Decorators); }
                    else if (Child.ChildTask) { Walk(Child.ChildTask, &Child.Decorators); }
                }
            }
            else if (UBTTaskNode* Task = Cast<UBTTaskNode>(Node))
            {
                for (UBTService* Service : Task->Services) { if (Service) { Ordered.Add(Service); } }
            }
            if (EntryDecorators) { for (const TObjectPtr<UBTDecorator>& Decorator : *EntryDecorators) { if (Decorator) { Ordered.Add(Decorator); } } }
        };
        for (const TObjectPtr<UBTDecorator>& Decorator : BT->RootDecorators) { if (Decorator) { Ordered.Add(Decorator); } }
        Walk(BT->RootNode, nullptr);
        const int32 Index = FCString::Atoi(*NodeId);
        if (Ordered.IsValidIndex(Index))
        {
            TargetNode = Ordered[Index];
            ResolvedNodeRole = Cast<UBTCompositeNode>(TargetNode) ? TEXT("composite") : Cast<UBTTaskNode>(TargetNode) ? TEXT("task") : Cast<UBTService>(TargetNode) ? TEXT("service") : TEXT("decorator");
        }
    }
    if (!TargetNode && BT->RootNode)
    {
        const bool bRootAlias = NodeId.Equals(TEXT("Root"), ESearchCase::IgnoreCase) ||
                                NodeId.Equals(TEXT("RootNode"), ESearchCase::IgnoreCase);
        if (bRootAlias)
        {
            TargetNode = BT->RootNode;
            ResolvedNodeRole = TEXT("root");
        }
        else
        {
            VisitComposite(BT->RootNode);
        }
    }

    if (!TargetNode)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Behavior Tree node not found: %s"), *NodeId),
                            TEXT("NOT_FOUND"));
        return true;
    }

    // A call that set nothing answered success; it is refused like the other edits that set nothing.
    const TSharedPtr<FJsonObject>* PropertiesObject = nullptr;
    if (!Payload->TryGetObjectField(TEXT("properties"), PropertiesObject) || !PropertiesObject || !PropertiesObject->IsValid() || (*PropertiesObject)->Values.Num() == 0)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("properties is required: the node's properties to set, by name."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    TArray<FString> ConfiguredProperties;
    TArray<FString> SkippedReasons;
    {
        // One undo step for the properties applied; a skipped one changes nothing.
        TargetNode->SetFlags(RF_Transactional);
        FMcpAssetEdit Edit(TEXT("Configure Behavior Tree Node"), {BT, TargetNode}, Result);
        McpBehaviorTreeHandlers::ApplyNodeProperties(TargetNode, BT, **PropertiesObject, ConfiguredProperties, SkippedReasons);
        if (ConfiguredProperties.Num() == 0)
        {
            // Nothing changed, known only once tried: undo keeps no empty step.
            Edit.Transaction.Cancel();
        }
    }
    const int32 ConfiguredPropertyCount = ConfiguredProperties.Num();

    const bool bSaveAttempted = ConfiguredPropertyCount > 0;
    bool bSaved = true;
    if (bSaveAttempted)
    {
        BT->MarkPackageDirty();
        bSaved = McpSafeAssetSave(BT);
        if (!bSaved)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Failed to save Behavior Tree after configuring node: %s"), *BTPath),
                                TEXT("SAVE_FAILED"));
            return true;
        }
    }

    Result->SetStringField(TEXT("behaviorTreePath"), BTPath);
    Result->SetStringField(TEXT("nodeId"), NodeId);
    Result->SetStringField(TEXT("resolvedNodeName"), TargetNode->GetName());
    Result->SetStringField(TEXT("resolvedNodeTitle"), TargetNode->GetNodeName());
    Result->SetStringField(TEXT("nodeRole"), ResolvedNodeRole);
    Result->SetNumberField(TEXT("configuredPropertyCount"), ConfiguredPropertyCount);
    Result->SetBoolField(TEXT("saveAttempted"), bSaveAttempted);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetArrayField(TEXT("configuredProperties"), McpHandlerUtils::ToJsonStringArray(ConfiguredProperties));
    if (SkippedReasons.Num() > 0)
    {
        Result->SetArrayField(TEXT("skippedProperties"), McpHandlerUtils::ToJsonStringArray(SkippedReasons));
    }
    if (ConfiguredPropertyCount == 0)
    {
        TArray<FString> Skipped;
        for (const auto& Pair : (*PropertiesObject)->Values)
        {
            Skipped.Emplace(*Pair.Key);
        }
        Self->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("No supplied property could be applied to %s: %s (%s)"), *TargetNode->GetClass()->GetName(), *FString::Join(Skipped, TEXT(", ")), *FString::Join(SkippedReasons, TEXT("; "))), TEXT("PROPERTY_NOT_FOUND"));
        return true;
    }
    McpHandlerUtils::AddVerification(Result, BT);

    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Behavior Tree node configured"), Result);
    return true;
}
}
