#include "McpAutomationBridge_AnimationHandlersActionContext.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationAddStateAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Add a state to a state machine
    FString BlueprintPath;
    Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);

    FString MachineName;
    Payload->TryGetStringField(TEXT("machineName"), MachineName);
    if (MachineName.IsEmpty()) {
      MachineName = TEXT("StateMachine");
    }

    FString StateName;
    Payload->TryGetStringField(TEXT("stateName"), StateName);

    FString AnimationPath;
    Payload->TryGetStringField(TEXT("animationPath"), AnimationPath);

    if (BlueprintPath.IsEmpty() || StateName.IsEmpty()) {
      Message = TEXT("blueprintPath and stateName required for add_state");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      TArray<FString> Commands;
      Commands.Add(FString::Printf(TEXT("AddAnimState %s %s %s %s"),
          *BlueprintPath, *MachineName, *StateName, *AnimationPath));

      bool bIsEntry = false;
      bool bIsExit = false;
      Payload->TryGetBoolField(TEXT("isEntry"), bIsEntry);
      Payload->TryGetBoolField(TEXT("isExit"), bIsExit);

      if (bIsEntry) {
        Commands.Add(FString::Printf(TEXT("SetAnimStateEntry %s %s %s"),
            *BlueprintPath, *MachineName, *StateName));
      }
      if (bIsExit) {
        Commands.Add(FString::Printf(TEXT("SetAnimStateExit %s %s %s"),
            *BlueprintPath, *MachineName, *StateName));
      }

      FString CommandError;
      if (!Context.Bridge.ExecuteEditorCommands(Commands, CommandError)) {
        Message = CommandError.IsEmpty() ? TEXT("Failed to add state") : CommandError;
        ErrorCode = TEXT("COMMAND_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        bSuccess = true;
        Message = FString::Printf(TEXT("State '%s' added to %s"), *StateName, *MachineName);
        Resp->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Resp->SetStringField(TEXT("machineName"), MachineName);
        Resp->SetStringField(TEXT("stateName"), StateName);
        if (!AnimationPath.IsEmpty()) {
          Resp->SetStringField(TEXT("animationPath"), AnimationPath);
        }
        Resp->SetBoolField(TEXT("isEntry"), bIsEntry);
        Resp->SetBoolField(TEXT("isExit"), bIsExit);
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
