// McpAutomationBridge_BehaviorTreeSerializers.h
// Shared, runtime-only (AIModule) serializers for Behavior Tree + Blackboard assets.
// Traverses the runtime object graph (UBehaviorTree::RootNode), never the editor BTGraph,
// so it compiles in all targets and needs no BehaviorTreeEditor dependency.
#pragma once

#include "CoreMinimal.h"

class UBehaviorTree;
class UBTNode;
class UBTDecorator;
class UBlackboardData;
class FJsonObject;
class FJsonValue;
struct FBTDecoratorLogic;

namespace McpBehaviorTreeSerializers
{
    /**
     * Serialize a full Behavior Tree asset into Out:
     *   assetPath, blackboardAsset (path or JSON null), hasRootNode,
     *   nodeCount, executionNodeCount, rootDecorators[], rootDecoratorOpsRaw[], rootNode (or null).
     * Reads BT->RootNode (runtime). Honors the null-RootNode contract (success + rootNode:null).
     */
    void SerializeBehaviorTree(UBehaviorTree* BT, const TSharedRef<FJsonObject>& Out);

    /**
     * Serialize a single BT node into a new JSON object.
     * @param EntryDecorators / EntryDecoratorOps  decorators on the edge INTO this node
     *        (from the parent's FBTCompositeChild). Pass nullptr for the root and for
     *        aux nodes (decorators/services), which carry no incoming edge.
     * @param Depth / Visited                      cycle + max-depth defense (R12/R13).
     * @param InOutNodeCount                        ++ for every serialized node.
     * @param InOutExecutionNodeCount               ++ for composites + tasks only.
     */
    TSharedPtr<FJsonObject> SerializeBTNode(
        UBTNode* Node,
        const TArray<TObjectPtr<UBTDecorator>>* EntryDecorators,
        const TArray<FBTDecoratorLogic>* EntryDecoratorOps,
        int32 Depth,
        TSet<const UBTNode*>& Visited,
        int32& InOutNodeCount,
        int32& InOutExecutionNodeCount);

    /**
     * Serialize a Blackboard Data asset into Out: keyCount + blackboardKeys[]
     * (parent-chain-walked, parent-most keys first, with additive enrichment), and
     * parentBlackboard (path) when a Parent exists.
     * Does NOT set the root asset-name field — the caller sets `assignedBlackboard`
     * (get_ai_info, short name) or `assetPath` (PR2). Preserves the PR0a-pinned per-key
     * {name, type=BlackboardKeyType_*, instanceSynced} bit-identical for the no-parent case.
     */
    void SerializeBlackboardData(UBlackboardData* BB, const TSharedRef<FJsonObject>& Out);

    /** Postfix decorator-logic ops -> flat [{ op: "Test|And|Or|Not", number }]. */
    TArray<TSharedPtr<FJsonValue>> SerializeDecoratorOpsRaw(const TArray<FBTDecoratorLogic>& Ops);
}
