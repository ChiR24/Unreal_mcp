#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/BehaviorTree/McpBTPrivate.h"

#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTreeGraphNode.h"

namespace McpBehaviorTreeHandlers {

void ApplyNodeProperties(UObject* Node, UBehaviorTree* Tree, const FJsonObject& Properties, TArray<FString>& OutSet,
                         TArray<FString>& OutSkipped)
{
  UBlackboardData* Blackboard = Tree->BlackboardAsset;
  for (const auto& Pair : Properties.Values) {
    const FString Name(*Pair.Key);
    FProperty* Property = Node ? Node->GetClass()->FindPropertyByName(FName(*Name)) : nullptr;
    if (!Property || !Pair.Value.IsValid()) {
      OutSkipped.Add(FString::Printf(TEXT("%s: no such property on %s"), *Name,
                                     Node ? *Node->GetClass()->GetName() : TEXT("this node")));
      continue;
    }
    // A Blackboard key selector is set by key name, the way the editor picks it; the generic struct import only knows
    // JSON objects and export text, so a plain "TargetActor" failed.
    FStructProperty* StructProp = CastField<FStructProperty>(Property);
    if (StructProp && StructProp->Struct == FBlackboardKeySelector::StaticStruct() && Pair.Value->Type == EJson::String) {
      FBlackboardKeySelector* Selector = StructProp->ContainerPtrToValuePtr<FBlackboardKeySelector>(Node);
      const FName KeyName(*Pair.Value->AsString());
      if (Blackboard) {
        const FBlackboardEntry* Entry = Blackboard->GetKey(Blackboard->GetKeyID(KeyName));
        if (!Entry || !Entry->KeyType) {
          OutSkipped.Add(FString::Printf(TEXT("%s: key '%s' is not in Blackboard %s"), *Name, *KeyName.ToString(),
                                         *Blackboard->GetPathName()));
          continue;
        }
        // The node's own filter (e.g. MoveTo takes only Object or Vector keys).
        bool bAllowed = Selector->AllowedTypes.Num() == 0;
        for (UBlackboardKeyType* Filter : Selector->AllowedTypes) {
          bAllowed |= Filter && Entry->KeyType->IsAllowedByFilter(Filter);
        }
        if (!bAllowed) {
          OutSkipped.Add(FString::Printf(TEXT("%s: key '%s' is a %s, a type this selector does not accept"), *Name,
                                         *KeyName.ToString(), *Entry->KeyType->GetClass()->GetName()));
          continue;
        }
      }
      Selector->SelectedKeyName = KeyName;
      if (Blackboard) {
        Selector->ResolveSelectedKey(*Blackboard);
      } else {
        UE_LOG(LogEngineWireSubsystem, Warning, TEXT("Behavior Tree %s has no Blackboard: %s names key '%s', unresolved."),
               *Tree->GetPathName(), *Name, *KeyName.ToString());
      }
      OutSet.Add(Name);
      continue;
    }
    // Shared reflection import (scalars, enums, structs incl. FValueOrBBKey_*, objects, arrays).
    FString ApplyError;
    if (!ApplyJsonValueToProperty(Node, Property, Pair.Value, ApplyError)) {
      OutSkipped.Add(FString::Printf(TEXT("%s: %s"), *Name, *ApplyError));
      continue;
    }
    OutSet.Add(Name);
  }
}

bool HandleSetNodeProperties(UEngineWireSubsystem* Subsystem,
                             const FRequestContext& Context,
                             const FGraphContext& GraphContext)
{
#if !MCP_HAS_BEHAVIOR_TREE_GRAPH
  Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                 TEXT("Behavior Tree graph editing requires UE 5.3+"),
                                 TEXT("NOT_SUPPORTED"));
  return true;
#else
  FString NodeId;
  Context.Payload->TryGetStringField(TEXT("nodeId"), NodeId);
  UEdGraphNode* TargetNode =
      FindGraphNodeByIdOrName(GraphContext.Graph, NodeId);
  if (!TargetNode) {
    Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                   TEXT("Node not found."),
                                   TEXT("NODE_NOT_FOUND"));
    return true;
  }

  UBehaviorTreeGraphNode* BTNode = AsBehaviorTreeGraphNode(TargetNode);
  UObject* Instance = nullptr;
  if (BTNode) {
    Instance = BTNode->NodeInstance;
  }

  FString Comment;
  const bool bComment = Context.Payload->TryGetStringField(TEXT("comment"), Comment);
  const TSharedPtr<FJsonObject>* Props = nullptr;
  TArray<FString> Set;
  TArray<FString> Skipped;
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  {
    // One undo step: the graph node (its comment), its node instance (the properties) and the tree the graph rebuilds.
    FMcpAssetEdit Edit(TEXT("Set Behavior Tree Node Properties"),
                       BehaviorTreeEditObjects(GraphContext, {TargetNode, Instance}), Result);
    if (bComment) {
      TargetNode->NodeComment = Comment;
    }
    if (Context.Payload->TryGetObjectField(TEXT("properties"), Props)) {
      ApplyNodeProperties(Instance, GraphContext.BehaviorTree, **Props, Set, Skipped);
    }
    if (bComment || Set.Num() > 0) {
      UpdateBehaviorTreeAsset(GraphContext);
    } else {
      // Nothing changed, known only once tried: undo keeps no empty step.
      Edit.Transaction.Cancel();
    }
  }
  // Nothing set (no comment, and no property set) answered success.
  if (!bComment && Set.Num() == 0) {
    Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                   Skipped.Num() > 0 ? TEXT("Nothing was set: ") + FString::Join(Skipped, TEXT("; "))
                                   : FString(TEXT("Nothing was set: pass comment, or properties the node has, each with a value of its type.")),
                                   TEXT("PROPERTY_NOT_FOUND"));
    return true;
  }
  if (Skipped.Num() > 0) {
    Result->SetArrayField(TEXT("skippedProperties"), McpHandlerUtils::ToJsonStringArray(Skipped));
  }
  McpSafeAssetSave(GraphContext.BehaviorTree);
  McpHandlerUtils::AddVerification(Result, GraphContext.BehaviorTree);
  Subsystem->SendAutomationResponse(Context.RequestingSocket,
                                    Context.RequestId, true,
                                    TEXT("Node properties updated."), Result);
  return true;
#endif
}

}
