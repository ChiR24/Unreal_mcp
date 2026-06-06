// McpAutomationBridge_BehaviorTreeSerializers.cpp
#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_BehaviorTreeSerializers.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "GameplayTagContainer.h"
#include "UObject/UnrealType.h"          // FStructProperty, TFieldIterator
#include "UObject/Class.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/BTNode.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BehaviorTreeTypes.h"          // FBlackboardKeySelector, FBTDecoratorLogic via BTCompositeNode
#include "BehaviorTree/Tasks/BTTask_RunBehavior.h"
#include "BehaviorTree/Tasks/BTTask_RunBehaviorDynamic.h"

namespace McpBehaviorTreeSerializers
{

// Max tree-traversal depth (R12). BT graphs are shallow; this is pure defense-in-depth.
static const int32 GMaxTreeDepth = 64;

// ---------------------------------------------------------------------------
// SerializeDecoratorOpsRaw — UE postfix EBTDecoratorLogic -> [{op, number}]
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// CollectBlackboardKeySelectors — enumerate ALL FBlackboardKeySelector props (R16)
// Fix of dev's CreateBTNodeRuntimeInfo, which broke after the first selector.
// ---------------------------------------------------------------------------
static void CollectBlackboardKeySelectors(UBTNode* Node, const TSharedRef<FJsonObject>& OutKeyProps)
{
    if (!Node || !Node->GetClass())
    {
        return;
    }
    for (TFieldIterator<FProperty> PropIt(Node->GetClass()); PropIt; ++PropIt)
    {
        if (FStructProperty* StructProp = CastField<FStructProperty>(*PropIt))
        {
            if (StructProp->Struct == FBlackboardKeySelector::StaticStruct())
            {
                const FBlackboardKeySelector* Selector =
                    StructProp->ContainerPtrToValuePtr<FBlackboardKeySelector>(Node);
                if (Selector && !Selector->SelectedKeyName.IsNone())
                {
                    OutKeyProps->SetStringField(StructProp->GetName(), Selector->SelectedKeyName.ToString());
                }
            }
        }
    }
}

static FString GetRunBehaviorDynamicInjectionTag(UBTTask_RunBehaviorDynamic* DynBT)
{
    if (!DynBT)
    {
        return FString();
    }

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
    return DynBT->GetInjectionTag().ToString();
#else
    FStructProperty* InjectionTagProperty = FindFProperty<FStructProperty>(DynBT->GetClass(), TEXT("InjectionTag"));
    if (InjectionTagProperty && InjectionTagProperty->Struct == FGameplayTag::StaticStruct())
    {
        const FGameplayTag* InjectionTag = InjectionTagProperty->ContainerPtrToValuePtr<FGameplayTag>(DynBT);
        if (InjectionTag)
        {
            return InjectionTag->ToString();
        }
    }
    return FString();
#endif
}

// ---------------------------------------------------------------------------
// One Blackboard entry. PR0a-pinned fields (name/type/instanceSynced) first,
// then additive enrichment. `Source` is the BB that declared the key; `SelfBB`
// is the asset being queried (for the inherited flag).
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Discriminate a UBTNode*. Composites & tasks are the execution nodes;
// decorators & services are auxiliary (share UBTAuxiliaryNode base).
// ---------------------------------------------------------------------------
static FString GetNodeTypeString(UBTNode* Node)
{
    if (Cast<UBTCompositeNode>(Node)) { return TEXT("composite"); }
    if (Cast<UBTTaskNode>(Node))      { return TEXT("task"); }
    if (Cast<UBTDecorator>(Node))     { return TEXT("decorator"); }
    if (Cast<UBTService>(Node))       { return TEXT("service"); }
    return TEXT("unknown");
}

TSharedPtr<FJsonObject> SerializeBTNode(
    UBTNode* Node,
    const TArray<TObjectPtr<UBTDecorator>>* EntryDecorators,
    const TArray<FBTDecoratorLogic>* EntryDecoratorOps,
    int32 Depth,
    TSet<const UBTNode*>& Visited,
    int32& InOutNodeCount,
    int32& InOutExecutionNodeCount)
{
    TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
    if (!Node)
    {
        return Obj;
    }

    // R12/R13: depth + cycle defense. Emit a per-node serializationError instead of
    // failing the whole response or looping forever.
    if (Depth > GMaxTreeDepth || Visited.Contains(Node))
    {
        Obj->SetStringField(TEXT("serializationError"),
            Depth > GMaxTreeDepth ? TEXT("max depth exceeded") : TEXT("cycle detected"));
        return Obj;
    }
    Visited.Add(Node);
    ++InOutNodeCount;

    const FString NodeType = GetNodeTypeString(Node);

    Obj->SetStringField(TEXT("nodeId"), FString::FromInt(InOutNodeCount - 1)); // opaque DFS index
    Obj->SetStringField(TEXT("nodeType"), NodeType);
    if (UClass* Cls = Node->GetClass())
    {
        Obj->SetStringField(TEXT("nodeClass"), Cls->GetName());
        Obj->SetStringField(TEXT("nodeClassPath"), Cls->GetPathName());
    }
    Obj->SetStringField(TEXT("nodeName"), Node->GetNodeName());

    // executionIndex (R14): UPROPERTY(transient), 0 until InitializeNode() runs at BT
    // runtime. On an asset-only load it is typically 0 -> OMIT (don't emit misleading 0).
    const uint16 ExecIdx = Node->GetExecutionIndex();
    if (ExecIdx != 0 && ExecIdx != MAX_uint16)
    {
        Obj->SetNumberField(TEXT("executionIndex"), ExecIdx);
    }

    // keyProperties{} — ALL FBlackboardKeySelector props (R16). Omit if none.
    {
        TSharedRef<FJsonObject> KeyProps = MakeShared<FJsonObject>();
        CollectBlackboardKeySelectors(Node, KeyProps);
        if (KeyProps->Values.Num() > 0)
        {
            Obj->SetObjectField(TEXT("keyProperties"), KeyProps);
        }
    }

    if (NodeType == TEXT("composite"))
    {
        ++InOutExecutionNodeCount;
        UBTCompositeNode* Comp = Cast<UBTCompositeNode>(Node);

        TArray<TSharedPtr<FJsonValue>> Services;
        for (UBTService* Svc : Comp->Services)
        {
            if (Svc)
            {
                Services.Add(MakeShared<FJsonValueObject>(
                    SerializeBTNode(Svc, nullptr, nullptr, Depth + 1, Visited,
                                    InOutNodeCount, InOutExecutionNodeCount).ToSharedRef()));
            }
        }
        Obj->SetArrayField(TEXT("services"), Services);

        TArray<TSharedPtr<FJsonValue>> Children;
        for (const FBTCompositeChild& Child : Comp->Children)
        {
            UBTNode* ChildNode = nullptr;
            if (Child.ChildComposite) { ChildNode = Child.ChildComposite; }
            else if (Child.ChildTask) { ChildNode = Child.ChildTask; }
            if (!ChildNode) { continue; }

            Children.Add(MakeShared<FJsonValueObject>(
                SerializeBTNode(ChildNode, &Child.Decorators, &Child.DecoratorOps,
                                Depth + 1, Visited, InOutNodeCount, InOutExecutionNodeCount).ToSharedRef()));
        }
        Obj->SetArrayField(TEXT("children"), Children);
    }
    else if (NodeType == TEXT("task"))
    {
        ++InOutExecutionNodeCount;
        UBTTaskNode* Task = Cast<UBTTaskNode>(Node);

        TArray<TSharedPtr<FJsonValue>> Services;
        for (UBTService* Svc : Task->Services)
        {
            if (Svc)
            {
                Services.Add(MakeShared<FJsonValueObject>(
                    SerializeBTNode(Svc, nullptr, nullptr, Depth + 1, Visited,
                                    InOutNodeCount, InOutExecutionNodeCount).ToSharedRef()));
            }
        }
        Obj->SetArrayField(TEXT("services"), Services);

        // RunBehavior / RunBehaviorDynamic: terminal subtree reference (do NOT recurse).
        if (UBTTask_RunBehavior* RunBT = Cast<UBTTask_RunBehavior>(Task))
        {
            if (UBehaviorTree* Sub = RunBT->GetSubtreeAsset())
            {
                Obj->SetStringField(TEXT("subTreeAsset"), Sub->GetPathName());
            }
            Obj->SetStringField(TEXT("subTreeSource"), TEXT("static"));
            Obj->SetStringField(TEXT("injectionTag"), TEXT(""));
            Obj->SetBoolField(TEXT("runtimeInjectable"), false);
        }
        else if (UBTTask_RunBehaviorDynamic* DynBT = Cast<UBTTask_RunBehaviorDynamic>(Task))
        {
            if (UBehaviorTree* Sub = DynBT->GetDefaultBehaviorAsset())
            {
                Obj->SetStringField(TEXT("subTreeAsset"), Sub->GetPathName());
            }
            Obj->SetStringField(TEXT("subTreeSource"), TEXT("dynamicDefault"));
            Obj->SetStringField(TEXT("injectionTag"), GetRunBehaviorDynamicInjectionTag(DynBT));
            Obj->SetBoolField(TEXT("runtimeInjectable"), true);
        }
    }
    // decorator/service: no children/services; keyProperties already emitted above.

    // Decorators on the edge INTO this node (from the parent's FBTCompositeChild).
    if (EntryDecorators && EntryDecorators->Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> EntryDecs;
        for (const TObjectPtr<UBTDecorator>& DecPtr : *EntryDecorators)
        {
            if (UBTDecorator* Dec = DecPtr)
            {
                EntryDecs.Add(MakeShared<FJsonValueObject>(
                    SerializeBTNode(Dec, nullptr, nullptr, Depth + 1, Visited,
                                    InOutNodeCount, InOutExecutionNodeCount).ToSharedRef()));
            }
        }
        Obj->SetArrayField(TEXT("entryDecorators"), EntryDecs);
        if (EntryDecoratorOps)
        {
            Obj->SetArrayField(TEXT("entryDecoratorOpsRaw"), SerializeDecoratorOpsRaw(*EntryDecoratorOps));
        }
    }

    return Obj;
}

void SerializeBehaviorTree(UBehaviorTree* BT, const TSharedRef<FJsonObject>& Out)
{
    if (!BT)
    {
        Out->SetBoolField(TEXT("hasRootNode"), false);
        Out->SetField(TEXT("rootNode"), MakeShared<FJsonValueNull>());
        Out->SetNumberField(TEXT("nodeCount"), 0);
        Out->SetNumberField(TEXT("executionNodeCount"), 0);
        Out->SetArrayField(TEXT("rootDecorators"), TArray<TSharedPtr<FJsonValue>>());
        Out->SetArrayField(TEXT("rootDecoratorOpsRaw"), TArray<TSharedPtr<FJsonValue>>());
        return;
    }

    Out->SetStringField(TEXT("assetPath"), BT->GetPathName());
    if (BT->BlackboardAsset)
    {
        Out->SetStringField(TEXT("blackboardAsset"), BT->BlackboardAsset->GetPathName());
    }
    else
    {
        Out->SetField(TEXT("blackboardAsset"), MakeShared<FJsonValueNull>());
    }

    UBTCompositeNode* Root = BT->RootNode;
    Out->SetBoolField(TEXT("hasRootNode"), Root != nullptr);

    int32 NodeCount = 0;
    int32 ExecNodeCount = 0;
    TSet<const UBTNode*> Visited;

    // Top-level (subtree) root decorators + their logic ops.
    TArray<TSharedPtr<FJsonValue>> RootDecs;
    for (const TObjectPtr<UBTDecorator>& DecPtr : BT->RootDecorators)
    {
        if (UBTDecorator* Dec = DecPtr)
        {
            RootDecs.Add(MakeShared<FJsonValueObject>(
                SerializeBTNode(Dec, nullptr, nullptr, 0, Visited, NodeCount, ExecNodeCount).ToSharedRef()));
        }
    }
    Out->SetArrayField(TEXT("rootDecorators"), RootDecs);
    Out->SetArrayField(TEXT("rootDecoratorOpsRaw"), SerializeDecoratorOpsRaw(BT->RootDecoratorOps));

    if (Root)
    {
        Out->SetObjectField(TEXT("rootNode"),
            SerializeBTNode(Root, nullptr, nullptr, 0, Visited, NodeCount, ExecNodeCount));
    }
    else
    {
        Out->SetField(TEXT("rootNode"), MakeShared<FJsonValueNull>());
    }

    Out->SetNumberField(TEXT("nodeCount"), NodeCount);
    Out->SetNumberField(TEXT("executionNodeCount"), ExecNodeCount);
}

} // namespace McpBehaviorTreeSerializers
