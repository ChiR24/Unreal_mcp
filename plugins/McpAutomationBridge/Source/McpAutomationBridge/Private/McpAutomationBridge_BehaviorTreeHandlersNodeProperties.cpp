#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_BehaviorTreeHandlersPrivate.h"

#if WITH_EDITOR
#include "McpAutomationBridgeGlobals.h"
#include "McpHandlerUtils.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTreeGraphNode.h"

namespace McpBehaviorTreeHandlers {

bool HandleSetNodeProperties(UMcpAutomationBridgeSubsystem* Subsystem,
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

  bool bModified = false;
  FString Comment;
  if (Context.Payload->TryGetStringField(TEXT("comment"), Comment)) {
    TargetNode->NodeComment = Comment;
    bModified = true;
  }

  UBehaviorTreeGraphNode* BTNode = nullptr;
  UClass* BTNodeClass =
      FindObject<UClass>(nullptr, TEXT("/Script/BehaviorTreeEditor.BehaviorTreeGraphNode"));
  if (BTNodeClass && TargetNode->GetClass()->IsChildOf(BTNodeClass)) {
    BTNode = static_cast<UBehaviorTreeGraphNode*>(TargetNode);
  }

  const TSharedPtr<FJsonObject>* Props = nullptr;
  if (BTNode && BTNode->NodeInstance &&
      Context.Payload->TryGetObjectField(TEXT("properties"), Props)) {
    for (const auto& Pair : (*Props)->Values) {
      FProperty* Prop =
          BTNode->NodeInstance->GetClass()->FindPropertyByName(*Pair.Key);
      if (!Prop) continue;
      if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop)) {
        if (Pair.Value->Type == EJson::Number) {
          FloatProp->SetPropertyValue_InContainer(
              BTNode->NodeInstance, (float)Pair.Value->AsNumber());
          bModified = true;
        }
      } else if (FDoubleProperty* DoubleProp =
                     CastField<FDoubleProperty>(Prop)) {
        if (Pair.Value->Type == EJson::Number) {
          DoubleProp->SetPropertyValue_InContainer(BTNode->NodeInstance,
                                                   Pair.Value->AsNumber());
          bModified = true;
        }
      } else if (FIntProperty* IntProp = CastField<FIntProperty>(Prop)) {
        if (Pair.Value->Type == EJson::Number) {
          IntProp->SetPropertyValue_InContainer(
              BTNode->NodeInstance, (int32)Pair.Value->AsNumber());
          bModified = true;
        }
      } else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop)) {
        if (Pair.Value->Type == EJson::Boolean) {
          BoolProp->SetPropertyValue_InContainer(BTNode->NodeInstance,
                                                 Pair.Value->AsBool());
          bModified = true;
        }
      } else if (FStrProperty* StrProp = CastField<FStrProperty>(Prop)) {
        if (Pair.Value->Type == EJson::String) {
          StrProp->SetPropertyValue_InContainer(BTNode->NodeInstance,
                                                Pair.Value->AsString());
          bModified = true;
        }
      } else if (FNameProperty* NameProp = CastField<FNameProperty>(Prop)) {
        if (Pair.Value->Type == EJson::String) {
          NameProp->SetPropertyValue_InContainer(
              BTNode->NodeInstance, FName(*Pair.Value->AsString()));
          bModified = true;
        }
      } else if (FStructProperty* StructProp =
                     CastField<FStructProperty>(Prop)) {
        if (StructProp->Struct == FBlackboardKeySelector::StaticStruct() &&
            Pair.Value->Type == EJson::String) {
          void* StructPtr =
              StructProp->ContainerPtrToValuePtr<void>(BTNode->NodeInstance);
          FBlackboardKeySelector* Selector =
              static_cast<FBlackboardKeySelector*>(StructPtr);
          Selector->SelectedKeyName = FName(*Pair.Value->AsString());
          if (UBlackboardData* Blackboard =
                  GraphContext.BehaviorTree->BlackboardAsset) {
            Selector->ResolveSelectedKey(*Blackboard);
            if (!Selector->IsSet()) {
              Subsystem->SendAutomationError(
                  Context.RequestingSocket,
                  Context.RequestId,
                  FString::Printf(TEXT("BlackboardKey '%s' not found in BT's assigned BB '%s' (typo or missing add_blackboard_key call?)"),
                                  *Pair.Value->AsString(),
                                  *Blackboard->GetPathName()),
                  TEXT("BB_KEY_NOT_FOUND"));
              return true;
            }
          } else {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                   TEXT("set_node_properties: BT '%s' has no BlackboardAsset assigned; BlackboardKey selector name set but not resolved."),
                   *GraphContext.BehaviorTree->GetPathName());
          }
          bModified = true;
        } else {
          UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
                 TEXT("set_node_properties: unsupported struct property '%s' on node (only FBlackboardKeySelector supported)"),
                 *Pair.Key);
        }
      }
    }
  }

  if (bModified) {
    GraphContext.BehaviorTree->Modify();
    GraphContext.Graph->Modify();
    TargetNode->Modify();
    UpdateBehaviorTreeAsset(GraphContext);
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Result, GraphContext.BehaviorTree);
  Subsystem->SendAutomationResponse(Context.RequestingSocket,
                                    Context.RequestId, true,
                                    TEXT("Node properties updated."), Result);
  return true;
#endif
}

}
#endif
