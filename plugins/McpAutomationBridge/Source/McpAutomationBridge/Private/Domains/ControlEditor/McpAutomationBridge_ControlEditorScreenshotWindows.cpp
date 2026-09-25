#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorScreenshotSupport.h"

#if WITH_EDITOR
#include "Editor/EditorPerformanceSettings.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace {
// Minimized windows used to be filtered out here. Windows parks a minimized
// window at -32000,-32000, so it is not on screen - but excluding it left the
// main editor frame absent from windows[] entirely, and a caller who asked for
// it by title got "No open editor window title contains 'X'. Open windows: ''".
// The list is what a caller navigates by, so list it and un-minimize it at
// capture time instead (RestoreWindowForCaptureForMcp).
bool IsCapturableSlateWindowForMcp(const TSharedPtr<SWindow> &Window) {
  return Window.IsValid() &&
         (Window->IsVisible() || Window->IsWindowMinimized());
}

void CollectSlateWindowsRecursiveForMcp(const TSharedRef<SWindow> &Window,
                                        TArray<TSharedRef<SWindow>> &Out) {
  if (IsCapturableSlateWindowForMcp(Window)) {
    Out.AddUnique(Window);
  }
  // An asset editor opened as a tab of another window is a CHILD window, not a
  // top-level one, so a top-level-only walk reports the main frame and nothing
  // else - which is exactly why a Widget Blueprint designer could not be
  // photographed while it was open next to the level editor.
  for (const TSharedRef<SWindow> &Child : Window->GetChildWindows()) {
    CollectSlateWindowsRecursiveForMcp(Child, Out);
  }
}
}  // namespace

void EnumerateEditorSlateWindowsForMcp(TArray<TSharedRef<SWindow>> &OutWindows) {
  OutWindows.Reset();
  if (!FSlateApplication::IsInitialized() ||
      !FSlateApplication::Get().CanDisplayWindows()) {
    return;
  }
  // Seed with the main frame. It is owned by the global tab manager rather than
  // tracked as an interactive top-level window, so enumerating only the latter
  // listed every floating asset editor but not the editor itself - leaving the
  // main window unaddressable by index or title.
  TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
  if (IsCapturableSlateWindowForMcp(RootWindow)) {
    CollectSlateWindowsRecursiveForMcp(RootWindow.ToSharedRef(), OutWindows);
  }
  // GetInteractiveTopLevelWindows() collapses to the topmost MODAL window while
  // one is open, so a single "import these content files?" dialog hid the main
  // frame and every asset editor: the list came back holding one 352x157 entry
  // and full_editor_window cheerfully photographed the dialog while reporting
  // success. Interactivity is about input routing; a screenshot wants whatever
  // is on screen.
  for (const TSharedRef<SWindow> &Window :
       FSlateApplication::Get().GetTopLevelWindows()) {
    CollectSlateWindowsRecursiveForMcp(Window, OutWindows);
  }
}

void AppendEditorWindowListForMcp(const TSharedPtr<FJsonObject> &Resp) {
  if (!Resp.IsValid()) {
    return;
  }
  TArray<TSharedRef<SWindow>> Windows;
  EnumerateEditorSlateWindowsForMcp(Windows);

  TArray<TSharedPtr<FJsonValue>> Entries;
  for (int32 Index = 0; Index < Windows.Num(); ++Index) {
    const TSharedRef<SWindow> &Window = Windows[Index];
    TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
    Entry->SetNumberField(TEXT("index"), Index);
    Entry->SetStringField(TEXT("title"), Window->GetTitle().ToString());
    const FVector2D Size = Window->GetSizeInScreen();
    Entry->SetNumberField(TEXT("width"), Size.X);
    Entry->SetNumberField(TEXT("height"), Size.Y);
    // simulate_input's mouse_click takes SCREEN coordinates, so a size with no
    // origin left a caller who could see a window unable to click anything in
    // it.
    const FVector2D Position = Window->GetPositionInScreen();
    Entry->SetNumberField(TEXT("x"), Position.X);
    Entry->SetNumberField(TEXT("y"), Position.Y);
    Entry->SetBoolField(TEXT("isActive"),
                        FSlateApplication::Get().GetActiveTopLevelWindow() == Window);
    // A minimized window is listed but sits off screen, so a click aimed at the
    // x/y above would hit nothing. Capturing it restores it first; say so
    // rather than letting a caller wonder why the coordinates are negative.
    Entry->SetBoolField(TEXT("isMinimized"), Window->IsWindowMinimized());
    // A modal dialog explains why the rest of the editor is ignoring
    // automation; say which window it is instead of leaving the caller to
    // guess from a surprise capture.
    Entry->SetBoolField(TEXT("isModal"),
                        FSlateApplication::Get().GetActiveModalWindow() == Window);
    Entries.Add(MakeShared<FJsonValueObject>(Entry));
  }
  Resp->SetArrayField(TEXT("windows"), Entries);
  Resp->SetNumberField(TEXT("windowCount"), Entries.Num());
}

TSharedPtr<SWindow> FindEditorSlateWindowForMcp(const FString &Query,
                                                FString &OutResolvedTitle,
                                                FString &OutError) {
  TArray<TSharedRef<SWindow>> Windows;
  EnumerateEditorSlateWindowsForMcp(Windows);
  if (Windows.Num() == 0) {
    OutError = TEXT("No visible editor windows to capture");
    return nullptr;
  }

  const FString Trimmed = Query.TrimStartAndEnd();

  // A bare number addresses the list this action reports, so a caller can go
  // straight from the windows[] it just read to the capture.
  if (Trimmed.IsNumeric()) {
    const int32 Index = FCString::Atoi(*Trimmed);
    if (!Windows.IsValidIndex(Index)) {
      OutError = FString::Printf(
          TEXT("Window index %d is out of range; %d window(s) are open"), Index,
          Windows.Num());
      return nullptr;
    }
    OutResolvedTitle = Windows[Index]->GetTitle().ToString();
    return Windows[Index];
  }

  for (const TSharedRef<SWindow> &Window : Windows) {
    if (Window->GetTitle().ToString().Contains(Trimmed, ESearchCase::IgnoreCase)) {
      OutResolvedTitle = Window->GetTitle().ToString();
      return Window;
    }
  }

  FString Available;
  for (const TSharedRef<SWindow> &Window : Windows) {
    if (!Available.IsEmpty()) {
      Available += TEXT(", ");
    }
    Available += FString::Printf(TEXT("'%s'"), *Window->GetTitle().ToString());
  }
  OutError = FString::Printf(
      TEXT("No open editor window title contains '%s'. Open windows: %s"),
      *Trimmed, *Available);
  return nullptr;
}

bool RestoreWindowForCaptureForMcp(const TSharedRef<SWindow> &Window) {
  if (!Window->IsWindowMinimized()) {
    return false;
  }
#if PLATFORM_WINDOWS
  TSharedPtr<FGenericWindow> Native = Window->GetNativeWindow();
  void *Handle = Native.IsValid() ? Native->GetOSWindowHandle() : nullptr;
  if (Handle != nullptr) {
    // SW_SHOWNOACTIVATE, not SW_RESTORE: the editor is being driven by
    // automation next to a human, and restoring it must not pull focus or the
    // cursor away from whatever they are doing. Applied through the window
    // placement: ShowWindow(SW_SHOWNOACTIVATE) on a minimized window left it
    // parked at -32000,-32000, while the placement form restores it to its
    // normal rectangle without activating it.
    const HWND Hwnd = static_cast<HWND>(Handle);
    WINDOWPLACEMENT Placement = {};
    Placement.length = sizeof(WINDOWPLACEMENT);
    if (::GetWindowPlacement(Hwnd, &Placement)) {
      Placement.showCmd = SW_SHOWNOACTIVATE;
      ::SetWindowPlacement(Hwnd, &Placement);
    }
    // Slate composites on the game thread; one tick gives the restored window a
    // frame to draw before ReadPixels runs, otherwise the capture is blank.
    FSlateApplication::Get().Tick();
    return true;
  }
#endif
  // Every other platform: fall back to Slate's own restore. It activates, which
  // is worse than not capturing at all is.
  Window->Restore();
  FSlateApplication::Get().Tick();
  return true;
}
#endif

// restore_editor_window: a minimized editor runs PIE at about 3 fps however the
// throttle preference is set, so timed tests need the frame back on screen.
// Restoring it by hand from outside the editor is exactly the kind of side
// channel the tool exists to replace; this does it without taking focus.
bool UMcpAutomationBridgeSubsystem::HandleControlEditorRestoreWindow(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  const TSharedPtr<SWindow> Root = FGlobalTabmanager::Get()->GetRootWindow();
  const TSharedPtr<FGenericWindow> Native = Root.IsValid() ? Root->GetNativeWindow() : nullptr;
  if (!Native.IsValid()) {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("WINDOW_NOT_FOUND"),
                              TEXT("The main editor window is not open."), nullptr);
    return true;
  }
  const bool bWasMinimized = Native->IsMinimized();
  RestoreWindowForCaptureForMcp(Root.ToSharedRef());
  bool bUnthrottle = true;
  Payload->TryGetBoolField(TEXT("unthrottle"), bUnthrottle);
  UEditorPerformanceSettings *Performance = GetMutableDefault<UEditorPerformanceSettings>();
  if (bUnthrottle && Performance && Performance->bThrottleCPUWhenNotForeground) {
    Performance->bThrottleCPUWhenNotForeground = false;
    Performance->SaveConfig();
  }
  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetBoolField(TEXT("wasMinimized"), bWasMinimized);
  Data->SetBoolField(TEXT("restored"), !Native->IsMinimized());
  Data->SetBoolField(TEXT("throttleOff"), Performance && !Performance->bThrottleCPUWhenNotForeground);
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         bWasMinimized ? TEXT("Editor window restored without taking focus")
                                       : TEXT("Editor window was already on screen"),
                         Data);
  return true;
#else
  return false;
#endif
}
