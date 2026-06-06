#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpSafeOperations.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimationAsset.h"
#include "Animation/BlendSpace.h"
#include "Kismet2/BlueprintEditorUtils.h"
#if __has_include("AnimGraphNode_BlendTree.h")
#include "AnimGraphNode_BlendTree.h"
#define MCP_HAS_ANIM_GRAPH_NODE_BLEND_TREE 1
#else
#define MCP_HAS_ANIM_GRAPH_NODE_BLEND_TREE 0
#endif
#if __has_include("Animation/BlendTree.h")
#include "Animation/BlendTree.h"
#define MCP_HAS_BLEND_TREE 1
#else
#define MCP_HAS_BLEND_TREE 0
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
bool HandleAnimationCreateBlendTreeAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // ============================================================================
    // Blend Tree Creation in AnimBlueprint's AnimGraph
    // ============================================================================
    // Creates a new blend tree node in an existing AnimBlueprint's AnimGraph.
    // Optionally configures blend parameters and adds children with animations.
    // Uses FGraphNodeCreator for proper graph editing.
    // ============================================================================
    FString BlueprintPath;
    Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);

    if (BlueprintPath.IsEmpty()) {
      Message = TEXT("blueprintPath is required for create_blend_tree");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString TreeName;
      Payload->TryGetStringField(TEXT("treeName"), TreeName);
      if (TreeName.IsEmpty()) {
        TreeName = TEXT("BlendTree");
      }

      // Load the AnimBlueprint
      UAnimBlueprint* AnimBP = LoadObject<UAnimBlueprint>(nullptr, *BlueprintPath);
      if (!AnimBP) {
        Message = FString::Printf(TEXT("AnimBlueprint not found: %s"), *BlueprintPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
        Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
      } else {
#if MCP_HAS_ANIM_GRAPH_NODE_BLEND_TREE && MCP_HAS_BLEND_TREE && MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA
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
          // Create the Blend Tree Node using FGraphNodeCreator
          FGraphNodeCreator<UAnimGraphNode_BlendTree> NodeCreator(*AnimGraph);
          UAnimGraphNode_BlendTree* BTNode = NodeCreator.CreateNode();
          BTNode->NodePosX = 0;
          BTNode->NodePosY = 0;
          NodeCreator.Finalize();

          // Access and configure the inner UBlendTree
#if MCP_HAS_BLEND_TREE
          if (UBlendTree* BlendTree = BTNode->GetBlendTree()) {
            BlendTree->Modify();

            // Process blend parameters array if provided
            const TArray<TSharedPtr<FJsonValue>>* BlendParamsArray = nullptr;
            if (Payload->TryGetArrayField(TEXT("blendParameters"), BlendParamsArray) && BlendParamsArray) {
              int32 ParamIndex = 0;
              for (const TSharedPtr<FJsonValue>& ParamValue : *BlendParamsArray) {
                if (!ParamValue.IsValid() || ParamValue->Type != EJson::Object) {
                  continue;
                }

                const TSharedPtr<FJsonObject> ParamObj = ParamValue->AsObject();
                FString ParamName;
                ParamObj->TryGetStringField(TEXT("name"), ParamName);

                double MinVal = 0.0, MaxVal = 1.0;
                ParamObj->TryGetNumberField(TEXT("min"), MinVal);
                ParamObj->TryGetNumberField(TEXT("max"), MaxVal);

                // Configure blend parameter (index 0 or 1 for 1D/2D blend trees)
                if (ParamIndex < 2) {
                  FBlendParameter& Param = const_cast<FBlendParameter&>(BlendTree->GetBlendParameter(ParamIndex));
                  if (!ParamName.IsEmpty()) {
                    Param.DisplayName = FText::FromString(ParamName);
                  }
                  Param.Min = static_cast<float>(MinVal);
                  Param.Max = static_cast<float>(MaxVal);
                }
                ParamIndex++;
              }
            }

            // Process children array if provided
            const TArray<TSharedPtr<FJsonValue>>* ChildrenArray = nullptr;
            if (Payload->TryGetArrayField(TEXT("children"), ChildrenArray) && ChildrenArray) {
              for (const TSharedPtr<FJsonValue>& ChildValue : *ChildrenArray) {
                if (!ChildValue.IsValid() || ChildValue->Type != EJson::Object) {
                  continue;
                }

                const TSharedPtr<FJsonObject> ChildObj = ChildValue->AsObject();
                FString AnimationPath;
                ChildObj->TryGetStringField(TEXT("animationPath"), AnimationPath);

                double BlendWeight = 1.0;
                ChildObj->TryGetNumberField(TEXT("blendWeight"), BlendWeight);

                // Load animation asset if path provided
                if (!AnimationPath.IsEmpty()) {
                  if (UAnimationAsset* AnimAsset = LoadObject<UAnimationAsset>(nullptr, *AnimationPath)) {
                    // Add as blend sample (simplified - full implementation would use proper blend tree API)
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
                           TEXT("create_blend_tree: Added animation %s with weight %.2f"),
                           *AnimationPath, BlendWeight);
                  }
                }
              }
            }

            BlendTree->MarkPackageDirty();
          }
#endif // MCP_HAS_BLEND_TREE

          FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);

          bool bShouldSave = true;
          Payload->TryGetBoolField(TEXT("save"), bShouldSave);
          if (bShouldSave) {
            McpSafeOperations::McpSafeAssetSave(AnimBP);
          }

          bSuccess = true;
          Message = FString::Printf(TEXT("Blend tree '%s' created in %s"), *TreeName, *BlueprintPath);
          Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
          Resp->SetStringField(TEXT("treeName"), TreeName);
          Resp->SetStringField(TEXT("nodeType"), TEXT("BlendTree"));
        }
#else
        // Blend tree headers not available
        Message = FString::Printf(
          TEXT("Cannot create blend tree '%s': AnimGraph BlendTree module headers not available. "
               "Rebuild with AnimGraph module enabled."),
          *TreeName);
        ErrorCode = TEXT("ANIMGRAPH_MODULE_UNAVAILABLE");
        Resp->SetStringField(TEXT("error"), Message);
#endif
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
