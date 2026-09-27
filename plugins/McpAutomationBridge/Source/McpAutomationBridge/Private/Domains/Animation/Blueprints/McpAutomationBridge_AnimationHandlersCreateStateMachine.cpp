#include "Domains/Animation/McpAutomationBridge_AnimationHandlersActionContext.h"
#include "Domains/AnimationAuthoring/McpAutomationBridge_AnimationAuthoringSupport.h"
#include "Safety/McpSafeOperations.h"

#include "Animation/AnimBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationStateMachineSchema.h"

namespace McpAnimationHandlers {
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
    // `name` is the blend tree's name in this family; reading it as the
    // blueprint path turned a missing blueprintPath into "AnimBlueprint not
    // found: <machine name>".
    FString BlueprintPath;
    Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);

    if (BlueprintPath.IsEmpty()) {
      Context.Fail(TEXT("INVALID_ARGUMENT"), TEXT("blueprintPath is required for create_state_machine"));
    } else {
      FString MachineName;
      Payload->TryGetStringField(TEXT("machineName"), MachineName);
      if (MachineName.IsEmpty()) {
        MachineName = TEXT("StateMachine");
      }

      // Load the AnimBlueprint
      UAnimBlueprint* AnimBP = LoadObject<UAnimBlueprint>(nullptr, *BlueprintPath);
      if (!AnimBP) {
        Context.Fail(TEXT("ASSET_NOT_FOUND"), FString::Printf(TEXT("AnimBlueprint not found: %s"), *BlueprintPath));
        Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
      } else {
        // Find the AnimGraph in the blueprint
        UEdGraph* AnimGraph = McpAnimationAuthoring::GetAnimGraphFromBlueprint(AnimBP);

        if (!AnimGraph) {
          Context.Fail(TEXT("GRAPH_NOT_FOUND"), TEXT("Could not find AnimGraph in blueprint"));
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
            }

            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
            McpSafeOperations::McpSafeAssetSave(AnimBP);

            bSuccess = true;
            Message = FString::Printf(TEXT("State machine '%s' created in %s"), *MachineName, *BlueprintPath);
            Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
            Resp->SetStringField(TEXT("machineName"), MachineName);
          }
        }
      }
    }
    return false;
}
} // namespace McpAnimationHandlers
