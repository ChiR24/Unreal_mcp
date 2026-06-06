#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpSafeOperations.h"

#include "Animation/AnimBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#if __has_include("AnimGraphNode_StateMachine.h")
#include "AnimGraphNode_StateMachine.h"
#endif
#if __has_include("AnimStateNode.h")
#include "AnimStateNode.h"
#endif
#if __has_include("AnimStateTransitionNode.h")
#include "AnimStateTransitionNode.h"
#define MCP_HAS_ANIM_STATE_TRANSITION 1
#else
#define MCP_HAS_ANIM_STATE_TRANSITION 0
#endif
#if __has_include("AnimationStateMachineGraph.h")
#include "AnimationStateMachineGraph.h"
#define MCP_HAS_ANIM_STATE_MACHINE_GRAPH 1
#else
#define MCP_HAS_ANIM_STATE_MACHINE_GRAPH 0
#endif
#if __has_include("AnimationStateMachineSchema.h")
#include "AnimationStateMachineSchema.h"
#define MCP_HAS_ANIM_STATE_MACHINE_SCHEMA 1
#else
#define MCP_HAS_ANIM_STATE_MACHINE_SCHEMA 0
#endif

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationCreateStateMachineAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;
  const FString &RequestId = Context.RequestId;
  TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;


    // ============================================================================
    // State Machine Creation using AnimGraph Editor API
    // ============================================================================
    // Creates a new state machine node in an existing AnimBlueprint's AnimGraph.
    // Optionally adds states and transitions if provided in the payload.
    // Uses FGraphNodeCreator and FBlueprintEditorUtils for proper graph editing.
    // ============================================================================
    FString BlueprintPath;
    Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
    if (BlueprintPath.IsEmpty()) {
      Payload->TryGetStringField(TEXT("name"), BlueprintPath);
    }

    if (BlueprintPath.IsEmpty()) {
      Message = TEXT("blueprintPath is required for create_state_machine");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString MachineName;
      Payload->TryGetStringField(TEXT("machineName"), MachineName);
      if (MachineName.IsEmpty()) {
        MachineName = TEXT("StateMachine");
      }

      // Load the AnimBlueprint
      UAnimBlueprint* AnimBP = LoadObject<UAnimBlueprint>(nullptr, *BlueprintPath);
      if (!AnimBP) {
        Message = FString::Printf(TEXT("AnimBlueprint not found: %s"), *BlueprintPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
        Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
      } else {
        // Check if AnimGraph headers are available
        #if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA
        // Find the AnimGraph in the blueprint
        UEdGraph* AnimGraph = nullptr;
        for (UEdGraph* Graph : AnimBP->FunctionGraphs) {
          if (Graph && Graph->GetName() == TEXT("AnimGraph")) {
            AnimGraph = Graph;
            break;
          }
        }

        if (!AnimGraph) {
          Message = TEXT("Could not find AnimGraph in blueprint");
          ErrorCode = TEXT("GRAPH_NOT_FOUND");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          // Check if a state machine with this name already exists
          bool bAlreadyExists = false;
          for (UEdGraphNode* Node : AnimGraph->Nodes) {
            if (UAnimGraphNode_StateMachine* ExistingSM = Cast<UAnimGraphNode_StateMachine>(Node)) {
              if (ExistingSM->GetStateMachineName() == MachineName) {
                bAlreadyExists = true;
                break;
              }
            }
          }

          if (bAlreadyExists) {
            bSuccess = true;
            Message = FString::Printf(TEXT("State machine '%s' already exists in %s"), *MachineName, *BlueprintPath);
            Resp->SetBoolField(TEXT("existingAsset"), true);
          } else {
            // Create the State Machine Node using FGraphNodeCreator
            FGraphNodeCreator<UAnimGraphNode_StateMachine> NodeCreator(*AnimGraph);
            UAnimGraphNode_StateMachine* SMNode = NodeCreator.CreateNode();
            SMNode->NodePosX = 0;
            SMNode->NodePosY = 0;
            NodeCreator.Finalize();

            // Create the internal State Machine Graph
            UAnimationStateMachineGraph* InnerGraph = Cast<UAnimationStateMachineGraph>(
              FBlueprintEditorUtils::CreateNewGraph(
                AnimBP,
                FName(*MachineName),
                UAnimationStateMachineGraph::StaticClass(),
                UAnimationStateMachineSchema::StaticClass()
              )
            );
            if (!InnerGraph) {
              Context.Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Failed to create animation state machine graph"),
                TEXT("CREATE_GRAPH_FAILED"));
              return true;
            }

            // Link the State Machine Node to its internal graph
            SMNode->EditorStateMachineGraph = InnerGraph;
            InnerGraph->OwnerAnimGraphNode = SMNode;

            // Initialize Entry Node (required for State Machines)
            const UAnimationStateMachineSchema* Schema = Cast<UAnimationStateMachineSchema>(InnerGraph->GetSchema());
            if (!Schema) {
              Context.Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Animation state machine graph has an invalid schema"),
                TEXT("INVALID_SCHEMA"));
              return true;
            }
            Schema->CreateDefaultNodesForGraph(*InnerGraph);

            // Process states array if provided
            const TArray<TSharedPtr<FJsonValue>>* StatesArray = nullptr;
            if (Payload->TryGetArrayField(TEXT("states"), StatesArray) && StatesArray) {
              int32 StatePosX = 200;
              for (const TSharedPtr<FJsonValue>& StateValue : *StatesArray) {
                if (!StateValue.IsValid() || StateValue->Type != EJson::Object) {
                  continue;
                }

                const TSharedPtr<FJsonObject> StateObj = StateValue->AsObject();
                FString StateName;
                StateObj->TryGetStringField(TEXT("name"), StateName);
                if (StateName.IsEmpty()) {
                  continue;
                }

                // Create the State Node
                FGraphNodeCreator<UAnimStateNode> StateCreator(*InnerGraph);
                UAnimStateNode* StateNode = StateCreator.CreateNode();
                StateNode->NodePosX = StatePosX;
                StateNode->NodePosY = 0;
                StateCreator.Finalize();

                // Rename the state's bound graph to set the state name
                if (StateNode->BoundGraph) {
                  FBlueprintEditorUtils::RenameGraph(StateNode->BoundGraph, *StateName);
                }

                StatePosX += 200;
              }
            }

            // Process transitions array if provided
            const TArray<TSharedPtr<FJsonValue>>* TransitionsArray = nullptr;
            if (Payload->TryGetArrayField(TEXT("transitions"), TransitionsArray) && TransitionsArray) {
              #if MCP_HAS_ANIM_STATE_TRANSITION
              for (const TSharedPtr<FJsonValue>& TransitionValue : *TransitionsArray) {
                if (!TransitionValue.IsValid() || TransitionValue->Type != EJson::Object) {
                  continue;
                }

                const TSharedPtr<FJsonObject> TransitionObj = TransitionValue->AsObject();
                FString SourceState;
                FString TargetState;
                TransitionObj->TryGetStringField(TEXT("sourceState"), SourceState);
                TransitionObj->TryGetStringField(TEXT("targetState"), TargetState);

                if (SourceState.IsEmpty() || TargetState.IsEmpty()) {
                  continue;
                }

                // Find the source and target states
                UAnimStateNode* FromNode = nullptr;
                UAnimStateNode* ToNode = nullptr;
                for (UEdGraphNode* Node : InnerGraph->Nodes) {
                  if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(Node)) {
                    FString NodeName = StateNode->GetStateName();
                    if (NodeName == SourceState) FromNode = StateNode;
                    if (NodeName == TargetState) ToNode = StateNode;
                  }
                }

                if (FromNode && ToNode) {
                  // Create the Transition Node
                  FGraphNodeCreator<UAnimStateTransitionNode> TransCreator(*InnerGraph);
                  UAnimStateTransitionNode* TransNode = TransCreator.CreateNode();
                  TransCreator.Finalize();

                  // Establish the connection between states
                  TransNode->CreateConnections(FromNode, ToNode);

                  // Configure crossfade duration
                  double CrossfadeDuration = 0.2;
                  TransitionObj->TryGetNumberField(TEXT("crossfadeDuration"), CrossfadeDuration);
                  TransNode->CrossfadeDuration = static_cast<float>(CrossfadeDuration);
                }
              }
              #endif // MCP_HAS_ANIM_STATE_TRANSITION
            }

            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
            McpSafeOperations::McpSafeAssetSave(AnimBP);

            bSuccess = true;
            Message = FString::Printf(TEXT("State machine '%s' created in %s"), *MachineName, *BlueprintPath);
            Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
            Resp->SetStringField(TEXT("machineName"), MachineName);
          }
        }
        #else
        // AnimGraph headers not available
        Message = FString::Printf(
          TEXT("Cannot create state machine '%s': AnimGraph module headers not available. "
               "Rebuild with AnimGraph module enabled or use add_state_machine action."),
          *MachineName);
        ErrorCode = TEXT("ANIMGRAPH_MODULE_UNAVAILABLE");
        Resp->SetStringField(TEXT("error"), Message);
        #endif
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
