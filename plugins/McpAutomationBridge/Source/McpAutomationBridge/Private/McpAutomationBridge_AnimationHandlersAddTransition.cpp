#include "McpAutomationBridge_AnimationHandlersActionContext.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationAddTransitionAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Add a transition between states
    FString BlueprintPath;
    Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);

    FString MachineName;
    Payload->TryGetStringField(TEXT("machineName"), MachineName);
    if (MachineName.IsEmpty()) {
      MachineName = TEXT("StateMachine");
    }

    FString SourceState;
    Payload->TryGetStringField(TEXT("sourceState"), SourceState);

    FString TargetState;
    Payload->TryGetStringField(TEXT("targetState"), TargetState);

    if (BlueprintPath.IsEmpty() || SourceState.IsEmpty() || TargetState.IsEmpty()) {
      Message = TEXT("blueprintPath, sourceState, and targetState required for add_transition");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      TArray<FString> Commands;
      Commands.Add(FString::Printf(TEXT("AddAnimTransition %s %s %s %s"),
          *BlueprintPath, *MachineName, *SourceState, *TargetState));

      FString CommandError;
      if (!Context.Bridge.ExecuteEditorCommands(Commands, CommandError)) {
        Message = CommandError.IsEmpty() ? TEXT("Failed to add transition") : CommandError;
        ErrorCode = TEXT("COMMAND_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        bSuccess = true;
        Message = FString::Printf(TEXT("Transition '%s' -> '%s' added"), *SourceState, *TargetState);
        Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Resp->SetStringField(TEXT("machineName"), MachineName);
        Resp->SetStringField(TEXT("sourceState"), SourceState);
        Resp->SetStringField(TEXT("targetState"), TargetState);
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
