#include "McpAutomationBridge_AnimationHandlersActionContext.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationAddStateMachineAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Add a state machine to an animation blueprint (delegate to create_state_machine)
    FString BlueprintPath;
    Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);

    FString MachineName;
    Payload->TryGetStringField(TEXT("machineName"), MachineName);
    if (MachineName.IsEmpty()) {
      MachineName = TEXT("StateMachine");
    }

    if (BlueprintPath.IsEmpty()) {
      Message = TEXT("blueprintPath required for add_state_machine");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      TArray<FString> Commands;
      Commands.Add(FString::Printf(TEXT("AddAnimStateMachine %s %s"), *BlueprintPath, *MachineName));

      FString CommandError;
      if (!Context.Bridge.ExecuteEditorCommands(Commands, CommandError)) {
        Message = CommandError.IsEmpty() ? TEXT("Failed to add state machine") : CommandError;
        ErrorCode = TEXT("COMMAND_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        bSuccess = true;
        Message = FString::Printf(TEXT("State machine '%s' added to %s"), *MachineName, *BlueprintPath);
        Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Resp->SetStringField(TEXT("machineName"), MachineName);
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
