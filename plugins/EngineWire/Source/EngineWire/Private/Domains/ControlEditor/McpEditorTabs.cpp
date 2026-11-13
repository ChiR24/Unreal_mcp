// Copyright (c) 2024 EngineWire Contributors

#include "Domains/ControlEditor/McpEditorSupport.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Reflection/McpReflectedInvoke.h"

#include "Dom/JsonObject.h"
#include "Editor/EditorPerformanceSettings.h"

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
  FMcpScopedParamBlock Params(Open);
  Api->ProcessEvent(Open, Params.Data());
  return true;
}

/** The level editor's own tab manager: the tabs it docks (Output Log, Outliner...) live there, not in the global one. */
static TSharedPtr<FTabManager> LevelEditorTabsForMcp() {
  return FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor"))
             ? FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor")).GetLevelEditorTabManager()
             : nullptr;
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
bool UEngineWireSubsystem::HandleOpenEditorTab(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString TabId;
  if (!Payload->TryGetStringField(TEXT("tabId"), TabId) || TabId.IsEmpty()) {
    SendAutomationError(
        Socket, RequestId,
        TEXT("'tabId' is required (for example 'BridgeTab' or 'Fab')."),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FName TabName(*TabId);
  // Through the level editor first: it puts a tab back where its layout keeps it closed and hands any other nomad tab
  // to the global manager. Invoked globally, a docked Output Log closed by id came back as a floating window.
  const TSharedPtr<FTabManager> LevelTabs = LevelEditorTabsForMcp();
  const bool bLevel = LevelTabs.IsValid() && LevelTabs->HasTabSpawner(TabName);
  if (!bLevel && !FGlobalTabmanager::Get()->HasTabSpawner(TabName)) {
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
    SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("No registered tab spawner named '%s'. Tab ids are exact (for example "
                             "'BridgeTab'); Fab opens with tabId 'Fab'. The owning plugin may be disabled."),
                        *TabId),
        TEXT("NOT_FOUND"));
    return true;
  }

  const FTabId Id = FTabId(TabName);
  const TSharedPtr<SDockTab> Tab = bLevel ? LevelTabs->TryInvokeTab(Id) : FGlobalTabmanager::Get()->TryInvokeTab(Id);
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

/**
 * Closes an open tab by id, as its own close button does. A floating window left
 * holding no tab closes with it, so the editor's saved layout stops reopening it:
 * a Message Log window floated once stayed in the layout and came back, over the
 * viewport, at every start.
 */
bool HandleCloseEditorTabForMcp(UEngineWireSubsystem *Self, const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload,
                                TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString TabId;
  if (!Payload->TryGetStringField(TEXT("tabId"), TabId) || TabId.IsEmpty()) {
    Self->SendAutomationError(Socket, RequestId,
                              TEXT("'tabId' is required (for example 'MessageLog' or 'OutputLog')."),
                              TEXT("INVALID_ARGUMENT"));
    return true;
  }
  // The global manager holds nomad and floating tabs only: one the level editor docked (the Output Log, opened by id)
  // lives in the level editor's own manager, which the global search never reaches.
  const FTabId Id = FTabId(FName(*TabId));
  TSharedPtr<SDockTab> Tab = FGlobalTabmanager::Get()->FindExistingLiveTab(Id);
  const TSharedPtr<FTabManager> LevelTabs = Tab.IsValid() ? nullptr : LevelEditorTabsForMcp();
  Tab = LevelTabs.IsValid() ? LevelTabs->FindExistingLiveTab(Id) : Tab;
  if (!Tab.IsValid()) {
    Self->SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("No open tab has the id '%s'. Tab ids are exact (for example 'MessageLog', "
                             "'OutputLog'); read_editor_ui lists the open windows."),
                        *TabId),
        TEXT("TAB_NOT_OPEN"));
    return true;
  }
  // The tab's own answer: an asset editor holding unsaved work, or a locked tab, may keep itself open.
  const bool bClosed = Tab->RequestCloseTab();
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("tabId"), TabId);
  Result->SetBoolField(TEXT("closed"), bClosed);
  Self->SendAutomationResponse(
      Socket, RequestId, bClosed,
      bClosed ? FString::Printf(TEXT("Tab '%s' closed."), *TabId)
              : FString::Printf(TEXT("Tab '%s' kept itself open (unsaved work or a locked tab)."), *TabId),
      Result, bClosed ? TEXT("") : TEXT("TAB_CLOSE_REFUSED"));
  return true;
}

// configure_editor window: restores the main frame, or with `minimize` puts it away, never
// taking focus. A minimized editor runs PIE at about 3 fps however the throttle preference
// is set, so timed tests need the frame back on screen; restoring it by hand from outside
// the editor is exactly the kind of side channel the tool exists to replace. The rest of the
// time a minimized, throttled editor costs the machine next to nothing, so `minimize` also
// turns the background throttle back on.
bool UEngineWireSubsystem::HandleControlEditorRestoreWindow(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const TSharedPtr<SWindow> Root = FGlobalTabmanager::Get()->GetRootWindow();
  const TSharedPtr<FGenericWindow> Native = Root.IsValid() ? Root->GetNativeWindow() : nullptr;
  if (!Native.IsValid()) {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("WINDOW_NOT_FOUND"),
                              TEXT("The main editor window is not open."), nullptr);
    return true;
  }
  const bool bWasMinimized = Native->IsMinimized();
  bool bMinimize = false;
  Payload->TryGetBoolField(TEXT("minimize"), bMinimize);
  bool bUnthrottle = true;
  Payload->TryGetBoolField(TEXT("unthrottle"), bUnthrottle);
  if (bMinimize) {
    MinimizeWindowForMcp(Root.ToSharedRef());
  } else {
    CountWindowRestoreRequestForMcp();
    RestoreWindowForCaptureForMcp(Root.ToSharedRef());
  }
  UEditorPerformanceSettings *Performance = GetMutableDefault<UEditorPerformanceSettings>();
  // minimize wants the throttle on and saves it; a restore with unthrottle turns it off for this editor
  // session only. Saved, it left every later launch running flat out in the background.
  if (Performance && (bMinimize || bUnthrottle) && Performance->bThrottleCPUWhenNotForeground != bMinimize) {
    Performance->bThrottleCPUWhenNotForeground = bMinimize;
    if (bMinimize) {
      Performance->SaveConfig();
    }
  }
  const bool bMinimizedNow = Native->IsMinimized();
  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetBoolField(TEXT("wasMinimized"), bWasMinimized);
  Data->SetBoolField(TEXT("minimized"), bMinimizedNow);
  Data->SetBoolField(TEXT("restored"), !bMinimizedNow);
  Data->SetBoolField(TEXT("throttleOff"), Performance && !Performance->bThrottleCPUWhenNotForeground);
  const TCHAR *Message = TEXT("Editor window was already on screen");
  if (bMinimize) {
    Message = !bMinimizedNow ? TEXT("Editor window could not be minimized without taking focus")
              : bWasMinimized ? TEXT("Editor window was already minimized")
                              : TEXT("Editor window minimized without taking focus");
  } else if (bWasMinimized) {
    Message = TEXT("Editor window restored without taking focus");
  }
  SendAutomationResponse(RequestingSocket, RequestId, true, Message, Data);
  return true;
}
