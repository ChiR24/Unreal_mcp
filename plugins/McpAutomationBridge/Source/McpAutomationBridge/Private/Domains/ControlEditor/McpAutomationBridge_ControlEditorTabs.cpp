// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"

/**
 * Invokes a registered nomad tab by id.
 *
 * Content-source plugins register their windows with FGlobalTabmanager
 * (Bridge as "BridgeTab", Fab as "FabTab"), so opening one is a global-registry
 * lookup rather than a plugin dependency. That matters for sign-in: Bridge and
 * Fab each own their own login flow and write their own session state, so the
 * correct way to authenticate is to open their window and let them do it —
 * never to reimplement a login here.
 */
bool UMcpAutomationBridgeSubsystem::HandleOpenEditorTab(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString TabId;
  if (!Payload->TryGetStringField(TEXT("tabId"), TabId) || TabId.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("'tabId' is required (for example 'BridgeTab' or 'FabTab')."), nullptr,
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FName TabName(*TabId);
  if (!FGlobalTabmanager::Get()->HasTabSpawner(TabName)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("No registered tab spawner named '%s'. The owning plugin may be disabled."),
                        *TabId),
        nullptr, TEXT("NOT_FOUND"));
    return true;
  }

  const TSharedPtr<SDockTab> Tab = FGlobalTabmanager::Get()->TryInvokeTab(TabName);
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("tabId"), TabId);
  Result->SetBoolField(TEXT("opened"), Tab.IsValid());
  SendAutomationResponse(
      Socket, RequestId, Tab.IsValid(),
      Tab.IsValid() ? FString::Printf(TEXT("Tab '%s' invoked."), *TabId)
                    : FString::Printf(TEXT("Tab '%s' could not be invoked."), *TabId),
      Result, Tab.IsValid() ? TEXT("") : TEXT("TAB_INVOKE_FAILED"));
  return true;
}
#else
bool UMcpAutomationBridgeSubsystem::HandleOpenEditorTab(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  SendAutomationResponse(Socket, RequestId, false, TEXT("Editor required."), nullptr,
                         TEXT("EDITOR_ONLY"));
  return true;
}
#endif
