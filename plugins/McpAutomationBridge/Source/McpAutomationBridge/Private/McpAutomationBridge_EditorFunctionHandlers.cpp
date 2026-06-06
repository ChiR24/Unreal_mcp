#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_EditorFunctionHandlersShared.h"

bool UMcpAutomationBridgeSubsystem::HandleExecuteEditorFunction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("execute_editor_function"), ESearchCase::IgnoreCase) &&
      !Lower.Contains(TEXT("execute_editor_function")) &&
      !Lower.Equals(TEXT("execute_console_command")) &&
      !Lower.Contains(TEXT("execute_console_command")) &&
      !Lower.Equals(TEXT("batch_console_commands")) &&
      !Lower.Contains(TEXT("batch_console_commands")))
    return false;

  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("execute_editor_function payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  if (Lower.Equals(TEXT("execute_console_command")) ||
      Lower.Contains(TEXT("execute_console_command"))) {
    TSharedPtr<FJsonObject> RoutedPayload = Payload;
    FString NestedCommand;
    FString TopLevelCommand;
    if (!Payload->TryGetStringField(TEXT("command"), TopLevelCommand)) {
      const TSharedPtr<FJsonObject> *ParamsPtr = nullptr;
      if (Payload->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr &&
          (*ParamsPtr).IsValid()) {
        (*ParamsPtr)->TryGetStringField(TEXT("command"), NestedCommand);
      }
    }

    if (!NestedCommand.IsEmpty()) {
      RoutedPayload = MakeShared<FJsonObject>();
      RoutedPayload->SetStringField(TEXT("command"), NestedCommand);
    }

    return HandleConsoleCommandAction(RequestId, TEXT("console_command"),
                                      RoutedPayload, RequestingSocket);
  }

  if (Lower.Equals(TEXT("batch_console_commands")) ||
      Lower.Contains(TEXT("batch_console_commands"))) {
    return HandleConsoleCommandAction(RequestId, TEXT("batch_console_commands"),
                                      Payload, RequestingSocket);
  }

  FString FunctionName;
  Payload->TryGetStringField(TEXT("functionName"), FunctionName);
  if (FunctionName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("functionName required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FString FN = FunctionName.ToUpper();
#if WITH_EDITOR
  using namespace McpEditorFunctionHandlers;

  if (HandleActorFunctions(*this, RequestId, FN, Payload, RequestingSocket))
    return true;
  if (HandleAssetQueryFunctions(*this, RequestId, FN, Payload, RequestingSocket))
    return true;
  if (HandleEditorControlFunctions(*this, RequestId, FN, Payload, RequestingSocket))
    return true;
  if (HandleBlueprintComponentFunction(*this, RequestId, FN, Payload, RequestingSocket))
    return true;
  if (HandleAssetCreationFunction(*this, RequestId, FN, Payload, RequestingSocket))
    return true;
  if (HandleRuntimeFunctions(*this, RequestId, FN, Payload, RequestingSocket))
    return true;
  if (HandleSystemFunctions(*this, RequestId, FN, Payload, RequestingSocket))
    return true;
#endif

  return false;
}
