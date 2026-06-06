#include "McpAutomationBridge_AIHandlerContext.h"

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
        FString BTPath = GetStringFieldAI(Payload, TEXT("behaviorTreePath"));
        FString NodeId = GetStringFieldAI(Payload, TEXT("nodeId"));

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

        if (BT->RootNode)
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
        const TSharedPtr<FJsonObject>* PropertiesObject = nullptr;
        if (Payload->TryGetObjectField(TEXT("properties"), PropertiesObject) && PropertiesObject && PropertiesObject->IsValid())
        {
            for (const auto& Pair : (*PropertiesObject)->Values)
            {
                const FString PropertyName(*Pair.Key);
                FProperty* Property = TargetNode->GetClass()->FindPropertyByName(FName(*PropertyName));
                if (!Property || !Pair.Value.IsValid())
                {
                    continue;
                }

                void* ValuePtr = Property->ContainerPtrToValuePtr<void>(TargetNode);
                if (FStrProperty* StrProperty = CastField<FStrProperty>(Property))
                {
                    FString Value;
                    if (Pair.Value->TryGetString(Value))
                    {
                        StrProperty->SetPropertyValue(ValuePtr, Value);
                    }
                    else
                    {
                        continue;
                    }
                }
                else if (FNameProperty* NameProperty = CastField<FNameProperty>(Property))
                {
                    FString Value;
                    if (Pair.Value->TryGetString(Value))
                    {
                        NameProperty->SetPropertyValue(ValuePtr, FName(*Value));
                    }
                    else
                    {
                        continue;
                    }
                }
                else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
                {
                    bool bValue = false;
                    if (Pair.Value->TryGetBool(bValue))
                    {
                        BoolProperty->SetPropertyValue(ValuePtr, bValue);
                    }
                    else
                    {
                        continue;
                    }
                }
                else if (FIntProperty* IntProperty = CastField<FIntProperty>(Property))
                {
                    double Number = 0.0;
                    if (Pair.Value->TryGetNumber(Number))
                    {
                        IntProperty->SetPropertyValue(ValuePtr, static_cast<int32>(Number));
                    }
                    else
                    {
                        continue;
                    }
                }
                else if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
                {
                    double Number = 0.0;
                    if (Pair.Value->TryGetNumber(Number))
                    {
                        FloatProperty->SetPropertyValue(ValuePtr, static_cast<float>(Number));
                    }
                    else
                    {
                        continue;
                    }
                }
                else if (FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(Property))
                {
                    double Number = 0.0;
                    if (Pair.Value->TryGetNumber(Number))
                    {
                        DoubleProperty->SetPropertyValue(ValuePtr, Number);
                    }
                    else
                    {
                        continue;
                    }
                }
                else
                {
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
        McpHandlerUtils::AddVerification(Result, BT);

        Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                               ConfiguredPropertyCount > 0
                                   ? TEXT("Behavior Tree node configured")
                                   : TEXT("Behavior Tree node resolved; no properties supplied"),
                               Result);
        return true;
    }

    // =========================================================================
    // 16.4 Environment Query System - EQS (5 actions)
    // =========================================================================

    return true;
}
}
#endif
