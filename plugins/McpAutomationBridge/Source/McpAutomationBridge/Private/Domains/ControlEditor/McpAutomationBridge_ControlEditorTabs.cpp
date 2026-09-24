// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Framework/Docking/TabManager.h"
#include "UObject/Package.h"
#include "Widgets/Docking/SDockTab.h"

/**
 * Fab registers no tab spawner: each open builds a fresh "Fab%d" tab. Its
 * Window-menu entry cannot be fired from code (a plain FUIAction, which
 * FToolMenuEntry keeps private and TryExecuteToolUIAction ignores), but Fab 5.8+
 * exposes UFabBrowserApi::OpenInNewTab, a UFUNCTION over the same
 * CreateNewFabTab. Reached by name, so this module takes no Fab dependency.
 */
static bool TryOpenFabByReflection(FString &OutDiagnostic) {
  UClass *ApiClass = FindObject<UClass>(nullptr, TEXT("/Script/Fab.FabBrowserApi"));
  UFunction *Open = ApiClass ? ApiClass->FindFunctionByName(TEXT("OpenInNewTab")) : nullptr;
  if (!Open) {
    OutDiagnostic = ApiClass ? TEXT("this Fab version has no OpenInNewTab (UE 5.7 or earlier)")
                             : TEXT("the Fab plugin is not loaded");
    return false;
  }
  UObject *Api = NewObject<UObject>(GetTransientPackage(), ApiClass);
  // Default-initialised parameters: an empty URL is Fab's own landing page.
  uint8 *Params = static_cast<uint8 *>(FMemory_Alloca(Open->ParmsSize));
  FMemory::Memzero(Params, Open->ParmsSize);
  for (TFieldIterator<FProperty> It(Open); It && It->HasAnyPropertyFlags(CPF_Parm); ++It) {
    It->InitializeValue_InContainer(Params);
  }
  Api->ProcessEvent(Open, Params);
  for (TFieldIterator<FProperty> It(Open); It && It->HasAnyPropertyFlags(CPF_Parm); ++It) {
    It->DestroyValue_InContainer(Params);
  }
  return true;
}

/**
 * Invokes a registered nomad tab by id.
 *
 * Content-source plugins register their windows with FGlobalTabmanager
 * (Bridge as "BridgeTab"), so opening one is a global-registry lookup rather
 * than a plugin dependency; Fab is the exception handled above. That matters
 * for sign-in: Bridge and Fab each own their own login flow and write their own
 * session state, so the correct way to authenticate is to open their window and
 * let them do it — never to reimplement a login here.
 */
bool UMcpAutomationBridgeSubsystem::HandleOpenEditorTab(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString TabId;
  if (!Payload->TryGetStringField(TEXT("tabId"), TabId) || TabId.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("'tabId' is required (for example 'BridgeTab' or 'Fab')."), nullptr,
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FName TabName(*TabId);
  if (!FGlobalTabmanager::Get()->HasTabSpawner(TabName)) {
    if (TabId.StartsWith(TEXT("Fab"))) {
      FString Diagnostic;
      const bool bOpened = TryOpenFabByReflection(Diagnostic);
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("tabId"), TabId);
      Result->SetBoolField(TEXT("opened"), bOpened);
      const FString Message = bOpened
          ? FString(TEXT("Fab opened in a new tab."))
          : FString::Printf(TEXT("Fab could not be opened from automation (%s); open it from Window > Fab."), *Diagnostic);
      SendAutomationResponse(Socket, RequestId, bOpened, Message, Result, bOpened ? TEXT("") : TEXT("TAB_INVOKE_FAILED"));
      return true;
    }
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("No registered tab spawner named '%s'. Tab ids are exact (for example "
                             "'BridgeTab'); Fab opens with tabId 'Fab'. The owning plugin may be disabled."),
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
