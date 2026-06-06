#include "McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
#include "McpAutomationBridge_AIBehaviorTreeGraphFeature.h"

#include "AIController.h"
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
#include "GameFramework/Pawn.h"
#include "McpAutomationBridge_BehaviorTreeSerializers.h"
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

        // --- blueprintPath: auto-discover AI setup from Pawn/Character/AIController blueprint ---
        FString BlueprintPath = GetStringFieldAI(Payload, TEXT("blueprintPath"));
        if (!BlueprintPath.IsEmpty())
        {
            BlueprintPath = SanitizeProjectRelativePath(BlueprintPath);
            if (BlueprintPath.IsEmpty())
            {
                Self->SendAutomationError(RequestingSocket, RequestId,
                    TEXT("Invalid blueprintPath: must be a valid project-relative path"),
                    TEXT("INVALID_PATH"));
                return true;
            }
            UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
            if (BP && BP->GeneratedClass)
            {
                UObject* CDO = BP->GeneratedClass->GetDefaultObject();

                // Pawn/Character: extract AIControllerClass
                if (APawn* PawnCDO = Cast<APawn>(CDO))
                {
                    if (PawnCDO->AIControllerClass)
                    {
                        AIInfo->SetStringField(TEXT("controllerClass"),
                            PawnCDO->AIControllerClass->GetName());
                    }
                }
                // AIController: report directly
                else if (Cast<AAIController>(CDO))
                {
                    AIInfo->SetStringField(TEXT("controllerClass"),
                        BP->GeneratedClass->GetName());
                }
            }
        }

        // --- controllerPath: explicit controller blueprint (overrides blueprintPath discovery) ---
        FString ControllerPath = GetStringFieldAI(Payload, TEXT("controllerPath"));
        if (!ControllerPath.IsEmpty())
        {
            UBlueprint* Controller = LoadObject<UBlueprint>(nullptr, *ControllerPath);
            if (Controller)
            {
                AIInfo->SetStringField(TEXT("controllerClass"),
                    Controller->GeneratedClass ? Controller->GeneratedClass->GetName() : TEXT("Unknown"));
            }
        }

        // --- behaviorTreePath ---
        FString BTPath = GetStringFieldAI(Payload, TEXT("behaviorTreePath"));
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

                TArray<TSharedPtr<FJsonValue>> RootDecoratorClasses;
                TArray<TSharedPtr<FJsonValue>> RootDecorators;
                for (UBTDecorator* RootDecorator : BT->RootDecorators)
                {
                    if (!RootDecorator)
                    {
                        continue;
                    }
                    RootDecoratorClasses.Add(MakeShared<FJsonValueString>(RootDecorator->GetClass()->GetName()));
                    RootDecorators.Add(MakeShared<FJsonValueObject>(CreateBTNodeRuntimeInfo(RootDecorator)));
                }
                AIInfo->SetNumberField(TEXT("rootDecoratorCount"), BT->RootDecorators.Num());
                AIInfo->SetArrayField(TEXT("rootDecoratorClasses"), RootDecoratorClasses);
                AIInfo->SetArrayField(TEXT("rootDecorators"), RootDecorators);

                // Report associated blackboard from BT asset (only if
                // blackboardPath was not explicitly provided, to avoid
                // silently overwriting an explicit value)
                FString ExplicitBBPath = GetStringFieldAI(Payload, TEXT("blackboardPath"));
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
        FString BBPath = GetStringFieldAI(Payload, TEXT("blackboardPath"));
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
        FString QueryPath = GetStringFieldAI(Payload, TEXT("queryPath"));
        if (!QueryPath.IsEmpty())
        {
            UEnvQuery* Query = LoadObject<UEnvQuery>(nullptr, *QueryPath);
            if (Query)
            {
                AIInfo->SetStringField(TEXT("queryName"), Query->GetName());
            }
        }

        Result->SetObjectField(TEXT("aiInfo"), AIInfo);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("AI info retrieved"), Result);
        return true;
    }

    // =========================================================================
    // Configuration Actions (3 new actions)
    // =========================================================================

    // set_ai_perception - Unified perception configuration (sight/hearing/damage in one call)
    return true;
}
}
#endif
