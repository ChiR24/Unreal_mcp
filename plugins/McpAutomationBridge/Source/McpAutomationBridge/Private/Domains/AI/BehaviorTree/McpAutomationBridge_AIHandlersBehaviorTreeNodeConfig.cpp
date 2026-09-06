#include "Domains/AI/McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTNode.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BTTaskNode.h"
#include "UObject/UnrealType.h"

namespace McpAIHandlers
{
bool HandleConfigureBehaviorTreeNode(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("configure_bt_node");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("configure_bt_node"))
    {
        FString BTPath = GetJsonStringField(Payload, TEXT("behaviorTreePath"));
        FString NodeId = GetJsonStringField(Payload, TEXT("nodeId"));

        if (NodeId.IsEmpty())
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                                TEXT("Missing nodeId parameter"),
                                TEXT("INVALID_PARAMS"));
            return true;
        }

        UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *BTPath);
        if (!BT)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Behavior Tree not found: %s"), *BTPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        UBTNode* TargetNode = nullptr;
        FString ResolvedNodeRole;

        auto MatchesNodeId = [&NodeId](const UBTNode* Candidate) -> bool
        {
            if (!Candidate)
            {
                return false;
            }
            return Candidate->GetName().Equals(NodeId, ESearchCase::IgnoreCase) ||
                   Candidate->GetPathName().Equals(NodeId, ESearchCase::IgnoreCase) ||
                   Candidate->GetNodeName().Equals(NodeId, ESearchCase::IgnoreCase);
        };

        TFunction<void(UBTCompositeNode*)> VisitComposite;
        VisitComposite = [&](UBTCompositeNode* Composite)
        {
            if (!Composite || TargetNode)
            {
                return;
            }

            if (MatchesNodeId(Composite))
            {
                TargetNode = Composite;
                ResolvedNodeRole = TEXT("composite");
                return;
            }

            for (UBTService* Service : Composite->Services)
            {
                if (MatchesNodeId(Service))
                {
                    TargetNode = Service;
                    ResolvedNodeRole = TEXT("service");
                    return;
                }
            }

            for (const FBTCompositeChild& Child : Composite->Children)
            {
                if (MatchesNodeId(Child.ChildTask))
                {
                    TargetNode = Child.ChildTask;
                    ResolvedNodeRole = TEXT("task");
                    return;
                }

                for (UBTDecorator* Decorator : Child.Decorators)
                {
                    if (MatchesNodeId(Decorator))
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
        // children, entry decorators); accept those ids here as well (dogfood #60).
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

        int32 ConfiguredPropertyCount = 0;
        TArray<FString> ConfiguredProperties;
        TArray<FString> SuppliedProperties;
        TArray<FString> SkippedReasons;
        const TSharedPtr<FJsonObject>* PropertiesObject = nullptr;
        if (Payload->TryGetObjectField(TEXT("properties"), PropertiesObject) && PropertiesObject && PropertiesObject->IsValid())
        {
            for (const auto& Pair : (*PropertiesObject)->Values)
            {
                const FString PropertyName(*Pair.Key);
                SuppliedProperties.Add(PropertyName);
                FProperty* Property = TargetNode->GetClass()->FindPropertyByName(FName(*PropertyName));
                if (!Property || !Pair.Value.IsValid())
                {
                    SkippedReasons.Add(FString::Printf(TEXT("%s: no such property on %s"), *PropertyName, *TargetNode->GetClass()->GetName()));
                    continue;
                }

                // Shared reflection import (scalars, enums, structs incl. FValueOrBBKey_*, objects, arrays).
                FString ApplyError;
                if (!ApplyJsonValueToProperty(TargetNode, Property, Pair.Value, ApplyError))
                {
                    SkippedReasons.Add(FString::Printf(TEXT("%s: %s"), *PropertyName, *ApplyError));
                    continue;
                }
                ++ConfiguredPropertyCount;
                ConfiguredProperties.Add(PropertyName);
            }
        }

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
        TArray<TSharedPtr<FJsonValue>> ConfiguredPropertyValues;
        for (const FString& PropertyName : ConfiguredProperties)
        {
            ConfiguredPropertyValues.Add(MakeShared<FJsonValueString>(PropertyName));
        }
        Result->SetArrayField(TEXT("configuredProperties"), ConfiguredPropertyValues);
        if (SkippedReasons.Num() > 0)
        {
            TArray<TSharedPtr<FJsonValue>> SkippedValues;
            for (const FString& Reason : SkippedReasons) { SkippedValues.Add(MakeShared<FJsonValueString>(Reason)); }
            Result->SetArrayField(TEXT("skippedProperties"), SkippedValues);
        }
        TArray<FString> Skipped = SuppliedProperties.FilterByPredicate([&ConfiguredProperties](const FString& Name) { return !ConfiguredProperties.Contains(Name); });
        if (Skipped.Num() > 0 && ConfiguredPropertyCount == 0)
        {
            Self->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("No supplied property could be applied to %s: %s (%s)"), *TargetNode->GetClass()->GetName(), *FString::Join(Skipped, TEXT(", ")), *FString::Join(SkippedReasons, TEXT("; "))), TEXT("PROPERTY_NOT_FOUND"));
            return true;
        }
        McpHandlerUtils::AddVerification(Result, BT);

        Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                               ConfiguredPropertyCount > 0
                                   ? TEXT("Behavior Tree node configured")
                                   : TEXT("Behavior Tree node resolved; no properties supplied"),
                               Result);
        return true;
    }

    return true;
}
}
#endif
