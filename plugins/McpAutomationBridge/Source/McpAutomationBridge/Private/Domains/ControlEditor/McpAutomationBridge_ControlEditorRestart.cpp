// Restarts the editor process.
//
// enable_plugin, disable_plugin and several project settings all answer
// "restart the editor for it to take effect", and until now nothing could.
// An automated pipeline hit a wall there that only a human could clear: the
// tool could change the project and then had to stop and ask someone to
// close and reopen the editor. FUnrealEdMisc::RestartEditor relaunches with
// the same project and command line, so the bridge comes back on its own
// port and the caller simply reconnects.
//
// Two things make this safe to drive remotely.
//
// Unsaved packages are REFUSED by default. A restart discards them silently
// and "the editor restarted" reads identically whether or not an hour of
// work went with it, so the caller has to say which it wants: save first
// with control_editor.save_all, or pass discardUnsaved to accept the loss.
// The refusal names the packages, because a count alone does not tell you
// whether it is a scratch asset or the level.
//
// And the response is sent BEFORE the restart is requested. The restart
// tears down the WebSocket, so a caller that restarted first would never
// learn whether its own call was even accepted -- it would look identical to
// a crash. The delay gives the socket time to flush the receipt.

#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"

#if WITH_EDITOR
#include "Containers/Ticker.h"
#include "UnrealEdMisc.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleControlEditorRestart(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId,
                              TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  TArray<UPackage *> DirtyWorldPackages;
  TArray<UPackage *> DirtyContentPackages;
  FEditorFileUtils::GetDirtyWorldPackages(DirtyWorldPackages);
  FEditorFileUtils::GetDirtyContentPackages(DirtyContentPackages);

  TArray<FString> DirtyNames;
  for (const UPackage *Package : DirtyWorldPackages) {
    if (Package) {
      DirtyNames.Add(Package->GetPathName());
    }
  }
  for (const UPackage *Package : DirtyContentPackages) {
    if (Package) {
      DirtyNames.Add(Package->GetPathName());
    }
  }

  const bool bDiscardUnsaved =
      GetJsonBoolField(Payload, TEXT("discardUnsaved"), false);
  const bool bBlockedByUnsaved = DirtyNames.Num() > 0 && !bDiscardUnsaved;

  TArray<TSharedPtr<FJsonValue>> NameValues;
  for (const FString &Name : DirtyNames) {
    NameValues.Add(MakeShared<FJsonValueString>(Name));
  }

  double DelaySeconds = GetJsonNumberField(Payload, TEXT("delaySeconds"), 1.0);
  DelaySeconds = FMath::Clamp(DelaySeconds, 0.1, 30.0);

  // validateOnly answers "would a restart go through right now, and what
  // would it cost?" without performing one. A pipeline needs that: the only
  // other way to discover that a restart is blocked is to attempt one, and a
  // capability whose every successful call ends the session cannot be
  // exercised by a test suite at all.
  if (GetJsonBoolField(Payload, TEXT("validateOnly"), false)) {
    TSharedPtr<FJsonObject> Preview = McpHandlerUtils::CreateResultObject();
    Preview->SetBoolField(TEXT("restarting"), false);
    Preview->SetBoolField(TEXT("validateOnly"), true);
    Preview->SetBoolField(TEXT("wouldRestart"), !bBlockedByUnsaved);
    Preview->SetArrayField(TEXT("unsavedPackages"), NameValues);
    Preview->SetNumberField(TEXT("unsavedCount"), DirtyNames.Num());
    Preview->SetNumberField(TEXT("delaySeconds"), DelaySeconds);
    Preview->SetStringField(TEXT("projectPath"), FPaths::GetProjectFilePath());
    SendAutomationResponse(
        Socket, RequestId, true,
        bBlockedByUnsaved
            ? TEXT("Restart would be refused: unsaved packages would be lost.")
            : TEXT("Restart would proceed."),
        Preview, FString());
    return true;
  }

  if (bBlockedByUnsaved) {
    TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
    Details->SetArrayField(TEXT("unsavedPackages"), NameValues);
    Details->SetNumberField(TEXT("unsavedCount"), DirtyNames.Num());
    SendStandardErrorResponse(
        this, Socket, RequestId, TEXT("UNSAVED_CHANGES"),
        FString::Printf(
            TEXT("%d package(s) have unsaved changes that a restart would "
                 "discard. Save them with control_editor.save_all, or pass "
                 "discardUnsaved to restart anyway."),
            DirtyNames.Num()),
        Details);
    return true;
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("restarting"), true);
  Resp->SetNumberField(TEXT("delaySeconds"), DelaySeconds);
  Resp->SetNumberField(TEXT("discardedPackageCount"), DirtyNames.Num());
  Resp->SetStringField(TEXT("projectPath"), FPaths::GetProjectFilePath());
  SendAutomationResponse(
      Socket, RequestId, true,
      TEXT("Editor restart requested; the bridge will drop and come back on "
           "the same port once the editor has relaunched."),
      Resp, FString());

  // Requested off a ticker rather than inline so the receipt above reaches the
  // socket first. bWarn is false because the unsaved-package gate above has
  // already made that decision explicitly -- leaving it true would raise a
  // modal dialog that no remote caller can answer.
  FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateLambda([](float) {
        FUnrealEdMisc::Get().RestartEditor(/*bWarn=*/false);
        return false;
      }),
      static_cast<float>(DelaySeconds));
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_SUPPORTED"),
                            TEXT("restart_editor requires an editor build"),
                            nullptr);
  return true;
#endif
}
