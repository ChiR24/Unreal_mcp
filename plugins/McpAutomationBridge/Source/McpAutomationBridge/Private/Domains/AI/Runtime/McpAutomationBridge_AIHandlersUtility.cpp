#include "Domains/AI/McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
#include "Domains/AI/BehaviorTree/McpAutomationBridge_AIBehaviorTreeGraphFeature.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTNode.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BTTaskNode.h"
#include "Engine/Blueprint.h"
#include "EnvironmentQuery/EnvQuery.h"

class UEnvQuery;
void McpSerializeEnvQueryInfo(UEnvQuery* Query, const TSharedPtr<FJsonObject>& Out); // Runtime/McpAutomationBridge_AIHandlersEnvQueryInfo.cpp
#include "Domains/BehaviorTree/McpAutomationBridge_BehaviorTreeSerializers.h"
#include "UObject/UnrealType.h"

namespace McpAIHandlers
{
bool HandleGetAIInfo(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("get_ai_info");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("get_ai_info"))
    {
        TSharedPtr<FJsonObject> AIInfo = McpHandlerUtils::CreateResultObject();

        // --- no target: refuse, and inventory what the project has to point at ---
        const TCHAR* TargetFields[] = { TEXT("blueprintPath"), TEXT("controllerPath"), TEXT("behaviorTreePath"),
                                        TEXT("blackboardPath"), TEXT("stateTreePath"), TEXT("queryPath") };
        bool bHasTarget = false;
        for (const TCHAR* Field : TargetFields)
        {
            bHasTarget |= !GetJsonStringField(Payload, Field).IsEmpty();
        }
        if (!bHasTarget)
        {
            AddAIAssetInventory(Result);
            Self->SendAutomationResponse(RequestingSocket, RequestId, false,
                TEXT("Pass blueprintPath, controllerPath, behaviorTreePath, blackboardPath, stateTreePath or queryPath"),
                Result, TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // --- blueprintPath / controllerPath: Pawn, Character or AIController blueprint ---
        // controllerPath is read after blueprintPath so an explicit controller
        // overrides what the pawn discovery wrote.
        for (const TCHAR* Field : { TEXT("blueprintPath"), TEXT("controllerPath") })
        {
            const FString RequestedPath = GetJsonStringField(Payload, Field);
            if (RequestedPath.IsEmpty())
            {
                continue;
            }
            const FString BlueprintPath = SanitizeProjectRelativePath(RequestedPath);
            if (BlueprintPath.IsEmpty())
            {
                Self->SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Invalid %s: must be a valid project-relative path"), Field),
                    TEXT("INVALID_PATH"));
                return true;
            }
            UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
            if (!BP)
            {
                Self->SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Blueprint not found for %s: %s"), Field, *RequestedPath),
                    TEXT("NOT_FOUND"));
                return true;
            }
            DescribeAIBlueprint(BP, AIInfo, Result);
        }

        // --- stateTreePath ---
        const FString StateTreePath = GetJsonStringField(Payload, TEXT("stateTreePath"));
        if (!StateTreePath.IsEmpty())
        {
            FString StateTreeError;
            if (!DescribeAIStateTree(StateTreePath, Result, StateTreeError))
            {
                Self->SendAutomationError(RequestingSocket, RequestId, StateTreeError, TEXT("NOT_FOUND"));
                return true;
            }
        }

        // --- behaviorTreePath ---
        FString BTPath = GetJsonStringField(Payload, TEXT("behaviorTreePath"));
        if (!BTPath.IsEmpty())
        {
            UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *BTPath);
            if (BT)
            {
                auto CreateBTNodeRuntimeInfo = [](UBTNode* Node) -> TSharedPtr<FJsonObject>
                {
                    TSharedPtr<FJsonObject> NodeInfo = McpHandlerUtils::CreateResultObject();
                    if (!Node)
                    {
                        return NodeInfo;
                    }

                    NodeInfo->SetStringField(TEXT("className"), Node->GetClass() ? Node->GetClass()->GetName() : TEXT("Unknown"));
                    NodeInfo->SetStringField(TEXT("nodeName"), Node->GetNodeName());

                    FString SelectedBlackboardKey;
                    for (TFieldIterator<FProperty> PropIt(Node->GetClass()); PropIt; ++PropIt)
                    {
                        if (FStructProperty* StructProp = CastField<FStructProperty>(*PropIt))
                        {
                            if (StructProp->Struct == FBlackboardKeySelector::StaticStruct())
                            {
                                FBlackboardKeySelector* Selector = StructProp->ContainerPtrToValuePtr<FBlackboardKeySelector>(Node);
                                if (Selector && !Selector->SelectedKeyName.IsNone())
                                {
                                    SelectedBlackboardKey = Selector->SelectedKeyName.ToString();
                                    break;
                                }
                            }
                        }
                    }
                    NodeInfo->SetStringField(TEXT("selectedBlackboardKey"), SelectedBlackboardKey);
                    return NodeInfo;
                };

                AIInfo->SetStringField(TEXT("assignedBehaviorTree"), BT->GetName());
                AIInfo->SetBoolField(TEXT("hasRootNode"), BT->RootNode != nullptr);

                AIInfo->SetNumberField(TEXT("rootDecoratorCount"), BT->RootDecorators.Num());

                TArray<TSharedPtr<FJsonValue>> RootDecoratorClassesArr;
                TArray<TSharedPtr<FJsonValue>> RootDecoratorsArr;
                for (UBTDecorator* RootDecorator : BT->RootDecorators)
                {
                    if (RootDecorator)
                    {
                        RootDecoratorClassesArr.Add(MakeShared<FJsonValueString>(RootDecorator->GetClass()->GetName()));
                        RootDecoratorsArr.Add(MakeShared<FJsonValueObject>(CreateBTNodeRuntimeInfo(RootDecorator)));
                    }
                }
                AIInfo->SetArrayField(TEXT("rootDecoratorClasses"), RootDecoratorClassesArr);
                AIInfo->SetArrayField(TEXT("rootDecorators"), RootDecoratorsArr);

                // Report associated blackboard from BT asset (only if
                // blackboardPath was not explicitly provided, to avoid
                // silently overwriting an explicit value)
                FString ExplicitBBPath = GetJsonStringField(Payload, TEXT("blackboardPath"));
                if (BT->BlackboardAsset && ExplicitBBPath.IsEmpty())
                {
                    AIInfo->SetStringField(TEXT("assignedBlackboard"),
                        BT->BlackboardAsset->GetName());
                }

#if MCP_AI_HAS_BEHAVIOR_TREE_GRAPH
                if (UBehaviorTreeGraph* BTGraph = Cast<UBehaviorTreeGraph>(BT->BTGraph))
                {
                    UClass* BTRootNodeClass = FindObject<UClass>(nullptr, TEXT("/Script/BehaviorTreeEditor.BehaviorTreeGraphNode_Root"));
                    for (UEdGraphNode* GraphNode : BTGraph->Nodes)
                    {
                        if (BTRootNodeClass && GraphNode && GraphNode->GetClass()->IsChildOf(BTRootNodeClass))
                        {
                            UBehaviorTreeGraphNode_Root* RootNode = static_cast<UBehaviorTreeGraphNode_Root*>(GraphNode);
                            AIInfo->SetStringField(TEXT("rootGraphBlackboard"), GetNameSafe(RootNode->BlackboardAsset));
                            AIInfo->SetBoolField(TEXT("rootGraphBlackboardMatchesAssigned"), RootNode->BlackboardAsset == BT->BlackboardAsset);
                            break;
                        }
                    }
                }
#endif
                // Count BT nodes (composites + tasks + decorators + services)
                if (BT->RootNode)
                {
                    int32 NodeCount = 0;
                    TArray<TSharedPtr<FJsonValue>> ChildDecorators;
                    TArray<TSharedPtr<FJsonValue>> Services;
                    TArray<UBTCompositeNode*> Stack;
                    Stack.Add(BT->RootNode);
                    while (Stack.Num() > 0)
                    {
                        UBTCompositeNode* Current = Stack.Pop();
                        NodeCount++;
                        NodeCount += Current->Services.Num();
                        for (UBTService* Service : Current->Services)
                        {
                            if (Service)
                            {
                                Services.Add(MakeShared<FJsonValueObject>(CreateBTNodeRuntimeInfo(Service)));
                            }
                        }
                        for (const FBTCompositeChild& Child : Current->Children)
                        {
                            NodeCount += Child.Decorators.Num();
                            for (UBTDecorator* Decorator : Child.Decorators)
                            {
                                if (Decorator)
                                {
                                    ChildDecorators.Add(MakeShared<FJsonValueObject>(CreateBTNodeRuntimeInfo(Decorator)));
                                }
                            }
                            if (Child.ChildComposite)
                            {
                                Stack.Add(Child.ChildComposite);
                            }
                            if (Child.ChildTask)
                            {
                                NodeCount++;
                                NodeCount += Child.ChildTask->Services.Num();
                                for (UBTService* Service : Child.ChildTask->Services)
                                {
                                    if (Service)
                                    {
                                        Services.Add(MakeShared<FJsonValueObject>(CreateBTNodeRuntimeInfo(Service)));
                                    }
                                }
                            }
                        }
                    }
                    AIInfo->SetNumberField(TEXT("btNodeCount"), NodeCount);
                    AIInfo->SetArrayField(TEXT("childDecorators"), ChildDecorators);
                    AIInfo->SetArrayField(TEXT("services"), Services);
                }
            }
        }

        // --- blackboardPath ---
        FString BBPath = GetJsonStringField(Payload, TEXT("blackboardPath"));
        if (!BBPath.IsEmpty())
        {
            UBlackboardData* BB = LoadObject<UBlackboardData>(nullptr, *BBPath);
            if (BB)
            {
                // Backward-compat: assignedBlackboard stays a short name (BB->GetName()).
                AIInfo->SetStringField(TEXT("assignedBlackboard"), BB->GetName());
                // keyCount + blackboardKeys (+ parentBlackboard) via the shared serializer.
                // For a no-parent BB this is bit-identical to the prior output plus additive
                // per-key enrichment fields (the PR0a characterization tolerates added fields).
                McpBehaviorTreeSerializers::SerializeBlackboardData(BB, AIInfo.ToSharedRef());
            }
        }

        // --- queryPath ---
        FString QueryPath = GetJsonStringField(Payload, TEXT("queryPath"));
        if (!QueryPath.IsEmpty())
        {
            UEnvQuery* Query = LoadObject<UEnvQuery>(nullptr, *QueryPath);
            if (Query)
            {
                McpSerializeEnvQueryInfo(Query, AIInfo); // dogfood #76: options, generators, tests
            }
        }

        Result->SetObjectField(TEXT("aiInfo"), AIInfo);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("AI info retrieved"), Result);
        return true;
    }

    return true;
}
}
#endif
