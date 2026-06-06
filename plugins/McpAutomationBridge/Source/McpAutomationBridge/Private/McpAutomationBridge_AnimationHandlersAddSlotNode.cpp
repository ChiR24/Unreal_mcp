#include "McpAutomationBridge_AnimationHandlersActionContext.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationAddSlotNodeAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Add a slot node to an animation blueprint
    FString BlueprintPath;
    Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);

    FString SlotName;
    Payload->TryGetStringField(TEXT("slotName"), SlotName);
    if (SlotName.IsEmpty()) {
      SlotName = TEXT("DefaultSlot");
    }

    if (BlueprintPath.IsEmpty()) {
      Message = TEXT("blueprintPath required for add_slot_node");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      TArray<FString> Commands;
      Commands.Add(FString::Printf(TEXT("AddAnimSlotNode %s %s"), *BlueprintPath, *SlotName));

      FString CommandError;
      if (!Context.Bridge.ExecuteEditorCommands(Commands, CommandError)) {
        Message = CommandError.IsEmpty() ? TEXT("Failed to add slot node") : CommandError;
        ErrorCode = TEXT("COMMAND_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        bSuccess = true;
        Message = FString::Printf(TEXT("Slot node '%s' added"), *SlotName);
        Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Resp->SetStringField(TEXT("slotName"), SlotName);
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
