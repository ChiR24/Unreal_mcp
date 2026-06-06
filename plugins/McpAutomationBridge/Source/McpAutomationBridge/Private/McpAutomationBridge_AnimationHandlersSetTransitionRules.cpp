#include "McpAutomationBridge_AnimationHandlersActionContext.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationSetTransitionRulesAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Set transition rules/conditions
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

    FString Condition;
    Payload->TryGetStringField(TEXT("condition"), Condition);

    if (BlueprintPath.IsEmpty() || SourceState.IsEmpty() || TargetState.IsEmpty()) {
      Message = TEXT("blueprintPath, sourceState, and targetState required for set_transition_rules");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      TArray<FString> Commands;
      Commands.Add(FString::Printf(TEXT("SetAnimTransitionRule %s %s %s %s %s"),
          *BlueprintPath, *MachineName, *SourceState, *TargetState, *Condition));

      FString CommandError;
      if (!Context.Bridge.ExecuteEditorCommands(Commands, CommandError)) {
        Message = CommandError.IsEmpty() ? TEXT("Failed to set transition rules") : CommandError;
        ErrorCode = TEXT("COMMAND_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        bSuccess = true;
        Message = FString::Printf(TEXT("Transition rule set for '%s' -> '%s'"), *SourceState, *TargetState);
        Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Resp->SetStringField(TEXT("machineName"), MachineName);
        Resp->SetStringField(TEXT("sourceState"), SourceState);
        Resp->SetStringField(TEXT("targetState"), TargetState);
        Resp->SetStringField(TEXT("condition"), Condition);
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
