#include "McpAutomationBridge_AnimationHandlersActionContext.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationAddBlendNodeAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Add a blend node to an animation blueprint
    FString BlueprintPath;
    Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);

    FString NodeType;
    Payload->TryGetStringField(TEXT("nodeType"), NodeType);
    if (NodeType.IsEmpty()) {
      NodeType = TEXT("BlendByBool");
    }

    FString NodeName;
    Payload->TryGetStringField(TEXT("nodeName"), NodeName);

    if (BlueprintPath.IsEmpty()) {
      Message = TEXT("blueprintPath required for add_blend_node");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      TArray<FString> Commands;
      Commands.Add(FString::Printf(TEXT("AddAnimBlendNode %s %s %s"),
          *BlueprintPath, *NodeType, *NodeName));

      FString CommandError;
      if (!Context.Bridge.ExecuteEditorCommands(Commands, CommandError)) {
        Message = CommandError.IsEmpty() ? TEXT("Failed to add blend node") : CommandError;
        ErrorCode = TEXT("COMMAND_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        bSuccess = true;
        Message = FString::Printf(TEXT("Blend node '%s' of type '%s' added"), *NodeName, *NodeType);
        Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Resp->SetStringField(TEXT("nodeType"), NodeType);
        Resp->SetStringField(TEXT("nodeName"), NodeName);
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
