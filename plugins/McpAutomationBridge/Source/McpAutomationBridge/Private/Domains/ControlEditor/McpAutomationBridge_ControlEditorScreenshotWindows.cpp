#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorScreenshotSupport.h"

#if WITH_EDITOR
namespace {
bool IsCapturableSlateWindowForMcp(const TSharedPtr<SWindow> &Window) {
  return Window.IsValid() && Window->IsVisible() && !Window->IsWindowMinimized();
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
#endif
