#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"

#if WITH_EDITOR
#include "Framework/Application/SlateUser.h"
#include "GenericPlatform/ICursor.h"
#include "Layout/WidgetPath.h"
#endif

#if WITH_EDITOR
namespace {
// Where the LAST synthetic move left the virtual pointer. Kept here rather than
// read back from FSlateApplication because the real cursor is deliberately no
// longer moved, so its position says nothing about this pointer's travel and a
// drag would compute its delta from whatever the person was doing elsewhere.
FVector2D &SyntheticCursorPosForMcp() {
  static FVector2D Position(0.0, 0.0);
  return Position;
}

// The innermost widgets Slate hit-tests at a screen point, outermost-last, so a
// click that went somewhere unintended says where it went.
FString DescribeWidgetsUnderPointForMcp(const FVector2D &ScreenPosition) {
  FSlateApplication &SlateApp = FSlateApplication::Get();
  FWidgetPath PathUnderPoint = SlateApp.LocateWindowUnderMouse(
      ScreenPosition, SlateApp.GetInteractiveTopLevelWindows());
  if (!PathUnderPoint.IsValid() || PathUnderPoint.Widgets.Num() == 0) {
    return TEXT("nothing (no interactive Slate window covers that point - the "
                "editor window may be minimised or off-screen)");
  }
  TArray<FString> Names;
  const int32 Depth = FMath::Min(PathUnderPoint.Widgets.Num(), 4);
  for (int32 Index = 0; Index < Depth; ++Index) {
    const int32 FromLeaf = PathUnderPoint.Widgets.Num() - 1 - Index;
    Names.Add(PathUnderPoint.Widgets[FromLeaf].Widget->GetTypeAsString());
  }
  return FString::Join(Names, TEXT(" < "));
}

bool RouteKeyToPIEForMcp(const FKey &InputKey, const EInputEvent InputEvent,
                         bool &bOutHandledByPIE) {
  bOutHandledByPIE = false;
  if (!GEditor || !GEditor->PlayWorld || !GEngine) {
    return false;
  }

  UWorld *PlayWorld = GEditor->PlayWorld.Get();
  if (!PlayWorld) {
    return false;
  }

  APlayerController *PlayerController = PlayWorld->GetFirstPlayerController();
  if (PlayerController && PlayerController->PlayerInput) {
    PlayerController->PlayerInput->ForceRebuildingKeyMaps(true);
  }

  const float AmountDepressed = InputEvent == IE_Released ? 0.0f : 1.0f;
#if MCP_CONTROL_HAS_INPUT_DEVICE_ID
  const FInputDeviceId InputDevice =
      IPlatformInputDeviceMapper::Get().GetDefaultInputDevice();
#else
  const int32 InputControllerId = 0;
#endif

  auto RouteKeyToPlayerController = [&]() -> bool {
    if (!PlayerController) {
      return false;
    }

#if MCP_CONTROL_HAS_SIMULATED_INPUT_EVENT_ARGS
    bOutHandledByPIE = PlayerController->InputKey(
        FInputKeyEventArgs::CreateSimulated(InputKey, InputEvent,
                                            AmountDepressed, 1, InputDevice));
#elif MCP_CONTROL_HAS_INPUT_DEVICE_ID
    bOutHandledByPIE = PlayerController->InputKey(FInputKeyParams(
        InputKey, InputEvent, static_cast<double>(AmountDepressed),
        InputKey.IsGamepadKey(), InputDevice));
#else
    bOutHandledByPIE = PlayerController->InputKey(FInputKeyParams(
        InputKey, InputEvent, static_cast<double>(AmountDepressed),
        InputKey.IsGamepadKey()));
#endif
    return true;
  };

  if (UGameViewportClient *GameViewportClient = PlayWorld->GetGameViewport()) {
    FViewport *GameViewport = GameViewportClient->GetGameViewport();
    if (GameViewport) {
#if MCP_CONTROL_HAS_INPUT_DEVICE_ID
      ULocalPlayer *TargetPlayer =
          GEngine->GetLocalPlayerFromInputDevice(GameViewportClient,
                                                 InputDevice);
#else
      ULocalPlayer *TargetPlayer =
          GEngine->GetLocalPlayerFromControllerId(GameViewportClient,
                                                  InputControllerId);
#endif
      const bool bViewportHadTargetController =
          TargetPlayer && TargetPlayer->PlayerController;
      FScopedConditionalWorldSwitcher WorldSwitcher(GameViewportClient);
#if MCP_CONTROL_HAS_SIMULATED_INPUT_EVENT_ARGS
      FInputKeyEventArgs KeyArgs(GameViewport, InputDevice, InputKey,
                                 InputEvent, AmountDepressed, false,
                                 FPlatformTime::Cycles64());
#elif MCP_CONTROL_HAS_INPUT_DEVICE_ID
      FInputKeyEventArgs KeyArgs(GameViewport, InputDevice, InputKey,
                                 InputEvent, AmountDepressed, false);
#else
      FInputKeyEventArgs KeyArgs(GameViewport, InputControllerId, InputKey,
                                 InputEvent, AmountDepressed, false);
#endif
      bOutHandledByPIE = GameViewportClient->InputKey(KeyArgs);
      if (bOutHandledByPIE || bViewportHadTargetController) {
        return true;
      }

      return RouteKeyToPlayerController();
    }
  }

  return RouteKeyToPlayerController();
}

void SimulateKeyInputForMcp(const FString &Key, const EInputEvent InputEvent,
                            const TCHAR *Verb, bool &bSuccess,
                            bool &bRoutedToPIE, bool &bHandledByPIE,
                            bool &bHandledBySlate, FString &Message) {
  if (Key.IsEmpty()) {
    Message =
        InputEvent == IE_Released ? TEXT("Key parameter required for key_up")
                                  : TEXT("Key parameter required for key_down");
    return;
  }

  FKey InputKey(*Key);
  if (!InputKey.IsValid()) {
    Message = FString::Printf(TEXT("Invalid key: %s"), *Key);
    return;
  }

  bRoutedToPIE = RouteKeyToPIEForMcp(InputKey, InputEvent, bHandledByPIE);
  if (!bRoutedToPIE) {
    FSlateApplication &SlateApp = FSlateApplication::Get();
    FKeyEvent KeyEvent(InputKey, FModifierKeysState(), 0, false, 0, 0);
    bHandledBySlate = InputEvent == IE_Released
                          ? SlateApp.ProcessKeyUpEvent(KeyEvent)
                          : SlateApp.ProcessKeyDownEvent(KeyEvent);
  }
  bSuccess = true;
  Message = bRoutedToPIE
                ? FString::Printf(TEXT("%s: %s (delivered to PIE)"), Verb, *Key)
                : FString::Printf(TEXT("%s: %s"), Verb, *Key);
}
}

FString NormalizeSimulatedInputTypeForMcp(const TSharedPtr<FJsonObject> &Payload) {
  FString InputType;
  Payload->TryGetStringField(TEXT("type"), InputType);
  if (InputType.IsEmpty()) {
    Payload->TryGetStringField(TEXT("inputType"), InputType);
  }
  FString InputAction;
  Payload->TryGetStringField(TEXT("inputAction"), InputAction);
  if (InputType.IsEmpty()) {
    InputType = InputAction;
  }

  InputType = InputType.ToLower();
  InputAction = InputAction.ToLower();
  if ((InputType == TEXT("key") || InputType == TEXT("keyboard")) &&
      !InputAction.IsEmpty()) {
    InputType = InputAction;
  }

  if (InputType == TEXT("press") || InputType == TEXT("pressed") ||
      InputType == TEXT("down")) {
    return TEXT("key_down");
  }
  if (InputType == TEXT("release") || InputType == TEXT("released") ||
      InputType == TEXT("up")) {
    return TEXT("key_up");
  }
  if (InputType == TEXT("click")) {
    return TEXT("mouse_click");
  }
  if (InputType == TEXT("move")) {
    return TEXT("mouse_move");
  }
  return InputType;
}

void SimulateEditorInputForMcp(const FString &InputType, const FString &Key,
                               const TSharedPtr<FJsonObject> &Payload,
                               bool &bSuccess, bool &bRoutedToPIE,
                               bool &bHandledByPIE, bool &bHandledBySlate,
                               FString &Message) {
  if (InputType == TEXT("key_down") || InputType == TEXT("keydown")) {
    SimulateKeyInputForMcp(Key, IE_Pressed, TEXT("Key down"), bSuccess,
                           bRoutedToPIE, bHandledByPIE, bHandledBySlate,
                           Message);
  } else if (InputType == TEXT("key_up") || InputType == TEXT("keyup")) {
    SimulateKeyInputForMcp(Key, IE_Released, TEXT("Key up"), bSuccess,
                           bRoutedToPIE, bHandledByPIE, bHandledBySlate,
                           Message);
  } else if (InputType == TEXT("mouse_click") || InputType == TEXT("click")) {
    double X = 0;
    double Y = 0;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

    FString Button = TEXT("left");
    Payload->TryGetStringField(TEXT("button"), Button);

    FKey MouseButtonKey = EKeys::LeftMouseButton;
    if (Button.ToLower() == TEXT("right")) {
      MouseButtonKey = EKeys::RightMouseButton;
    } else if (Button.ToLower() == TEXT("middle")) {
      MouseButtonKey = EKeys::MiddleMouseButton;
    }

    FSlateApplication &SlateApp = FSlateApplication::Get();
    FVector2D Position(static_cast<float>(X), static_cast<float>(Y));
    TSet<FKey> PressedButtons;
    PressedButtons.Add(MouseButtonKey);

    // Slate widgets (SButton, SComboBox, ...) gate clicks on
    // GetEffectingButton(); the EffectingButton-less FPointerEvent
    // constructor leaves it as an invalid FKey, so synthetic clicks never
    // trigger any editor UI. Use the EffectingButton-aware constructor and
    // mirror platform behaviour: down carries the button in PressedButtons,
    // up carries an empty set.
    // Name what is actually under (x, y) BEFORE dispatching. A synthetic click
    // that lands on a full-screen scrim, a stale overlay or the wrong panel
    // answers handledBySlate:true exactly like a click that pressed the button
    // the caller meant, so without this the only way to tell them apart is to
    // screenshot and guess at coordinates.
    const FString HitWidgetSummary = DescribeWidgetsUnderPointForMcp(Position);

    // Carry our own move INTO the click, in this same dispatch. Slate
    // synthesizes a mouse-move from the REAL cursor position every frame, so a
    // hover established by a separate simulate_input(mouse_move) call is wiped
    // out long before the caller's next request arrives -- and SButton only
    // fires OnClicked when the release lands on a widget it still considers
    // hovered. That is why a move+click pair used to work only while the
    // hardware cursor happened to be sitting on the button. Moving first here
    // makes the click land where it says it lands, wherever the real mouse is.
    // ...and borrow the hardware cursor for the duration of the click only.
    // A purely synthetic move is not enough: SButton fires OnClicked only when
    // the release lands on a widget Slate still considers hovered, and Slate
    // recomputes hover every frame from the REAL cursor, so the hover is gone
    // before the release. Parking the cursor on the target permanently is what
    // made automation unusable alongside other work, so put it back where the
    // person left it as soon as the release has been routed: they see at most a
    // single-frame blip instead of losing their pointer.
    TSharedPtr<ICursor> PlatformCursor =
        SlateApp.GetPlatformCursor().IsValid() ? SlateApp.GetPlatformCursor() : nullptr;
    const FVector2D RestoreCursorTo =
        PlatformCursor.IsValid() ? FVector2D(PlatformCursor->GetPosition()) : Position;
    if (PlatformCursor.IsValid()) {
      PlatformCursor->SetPosition(static_cast<int32>(Position.X),
                                  static_cast<int32>(Position.Y));
    }
    TSet<FKey> NoButtons;
    FPointerEvent PreClickMove(0, Position, SyntheticCursorPosForMcp(),
                               NoButtons, EKeys::Invalid, 0.0f,
                               FModifierKeysState());
    SlateApp.ProcessMouseMoveEvent(PreClickMove);
    SyntheticCursorPosForMcp() = Position;

    FPointerEvent MouseDownEvent(0, Position, Position, PressedButtons,
                                 MouseButtonKey, 0.0f, FModifierKeysState());
    const bool bDownHandled =
        SlateApp.ProcessMouseButtonDownEvent(nullptr, MouseDownEvent);

    TSet<FKey> ReleasedButtons;
    FPointerEvent MouseUpEvent(0, Position, Position, ReleasedButtons,
                               MouseButtonKey, 0.0f, FModifierKeysState());
    const bool bUpHandled = SlateApp.ProcessMouseButtonUpEvent(MouseUpEvent);
    if (PlatformCursor.IsValid()) {
      PlatformCursor->SetPosition(static_cast<int32>(RestoreCursorTo.X),
                                  static_cast<int32>(RestoreCursorTo.Y));
    }
    bHandledBySlate = bDownHandled || bUpHandled;
    bSuccess = true;

    // A click can be "handled" and still never reach the widget at (x, y):
    // while something holds mouse capture, Slate delivers straight to the
    // captor and skips hit-testing entirely. The usual case is a PIE session
    // whose viewport still owns the mouse because the game never called
    // SetInputMode_UIOnly/GameAndUI -- every click then answers
    // handledBySlate:true while the on-screen button is never pressed. Name the
    // captor so that is visible here instead of at the next screenshot.
    FString CaptorName;
    if (TSharedPtr<FSlateUser> CursorUser = SlateApp.GetCursorUser()) {
      if (TSharedPtr<SWidget> Captor = CursorUser->GetCursorCaptor()) {
        CaptorName = Captor->GetTypeAsString();
      }
    }
    Message = CaptorName.IsEmpty()
                  ? FString::Printf(TEXT("Mouse click at (%f, %f) reached %s"),
                                    X, Y, *HitWidgetSummary)
                  : FString::Printf(
                        TEXT("Mouse click at (%f, %f) was delivered to the "
                             "widget holding mouse capture (%s), NOT to "
                             "whatever is drawn at those coordinates."),
                        X, Y, *CaptorName);
  } else if (InputType == TEXT("mouse_move") || InputType == TEXT("move")) {
    double X = 0;
    double Y = 0;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

    // SetCursorPos drives the REAL system cursor: it yanked the pointer out of
    // whatever the person was doing on another window, and the editor grabbing
    // focus to receive it made automation unusable alongside any other work.
    // Slate routes a pointer event by the position carried ON the event, not by
    // where the hardware cursor happens to be, so a synthetic move updates
    // hover and drag state for the widgets under (x, y) while the person's
    // actual mouse stays exactly where they left it.
    FSlateApplication &SlateApp = FSlateApplication::Get();
    const FVector2D Position(static_cast<float>(X), static_cast<float>(Y));
    TSet<FKey> NoButtons;
    FPointerEvent MoveEvent(0, Position, SyntheticCursorPosForMcp(),
                            NoButtons, EKeys::Invalid, 0.0f,
                            FModifierKeysState());
    bHandledBySlate = SlateApp.ProcessMouseMoveEvent(MoveEvent);
    SyntheticCursorPosForMcp() = Position;
    bSuccess = true;
    Message = FString::Printf(
        TEXT("Mouse moved to (%f, %f) synthetically; the system cursor was NOT "
             "moved and no window was focused."),
        X, Y);
  } else {
    Message = FString::Printf(
        TEXT("Unknown input type: %s. Supported: key_down, key_up, mouse_click, mouse_move"),
        *InputType);
  }
}
#endif
