// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Domains/AssetWorkflow/Fab/McpAutomationBridge_FabImportJson.h"
#include "McpFabProvider.h"

#include "Dom/JsonObject.h"

/**
 * Stops one Fab import that the add started, where Fab gives a way to.
 *
 * Fab offers three handles and no more, and the adapter uses exactly those: an add still waiting in the
 * queue is dropped, a unreal-engine pack download is stopped by pressing the Cancel button on Fab's own
 * download notification, and an import Interchange is translating is told to cancel its tasks. A download
 * of a source format has no Cancel anywhere in Fab, so it is refused with NOT_CANCELLABLE and the reason.
 * The call can only be served while the game thread is free; during Fab's mesh build the editor is held
 * and every call answers EDITOR_BLOCKED, this one included.
 */
bool UMcpAutomationBridgeSubsystem::HandleCancelFabImport(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  IMcpFabProvider *Provider = GetMcpFabProvider();
  if (Provider == nullptr || !Provider->IsFabAvailable()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("Fab support is not loaded in this editor, so there is no import to cancel."),
        nullptr, TEXT("NOT_SUPPORTED"));
    return true;
  }

  FString OperationId;
  Payload->TryGetStringField(TEXT("operationId"), OperationId);
  if (!McpFabImportJson::IsPlainKey(OperationId)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("'operationId' (from the add's reply) is required: [A-Za-z0-9_-], 64 characters at most."),
        nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString Message;
  FString ErrorCode;
  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("operationId"), OperationId);
  if (!Provider->CancelImport(OperationId, Message, ErrorCode)) {
    // Whatever the reason, the import is still there to read, so the way to read it is offered.
    if (ErrorCode != TEXT("NOT_FOUND")) {
      Data->SetObjectField(TEXT("nextCall"), McpFabImportJson::MakeStatusNextCall(OperationId));
    }
    SendAutomationResponse(Socket, RequestId, false, Message, Data, ErrorCode);
    return true;
  }

  FMcpFabImportStatus Status;
  if (Provider->GetImportStatus(OperationId, Status)) {
    Data->SetStringField(TEXT("phase"), Status.Phase);
  }
  Data->SetBoolField(TEXT("cancelled"), true);
  Data->SetStringField(TEXT("note"), Message);
  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Cancel accepted for Fab import %s."), *OperationId), Data);
  return true;
}
