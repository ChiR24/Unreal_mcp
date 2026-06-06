#include "McpAutomationBridge_ControlEditorSupport.h"

#if WITH_EDITOR
namespace {
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

    FPointerEvent MouseDownEvent(0, Position, Position, FVector2D(0.0f, 0.0f),
                                 PressedButtons, FModifierKeysState());
    SlateApp.ProcessMouseButtonDownEvent(nullptr, MouseDownEvent);

    TSet<FKey> ReleasedButtons;
    FPointerEvent MouseUpEvent(0, Position, Position, FVector2D(0.0f, 0.0f),
                               ReleasedButtons, FModifierKeysState());
    SlateApp.ProcessMouseButtonUpEvent(MouseUpEvent);
    bHandledBySlate = true;
    bSuccess = true;
    Message = FString::Printf(TEXT("Mouse click at (%f, %f)"), X, Y);
  } else if (InputType == TEXT("mouse_move") || InputType == TEXT("move")) {
    double X = 0;
    double Y = 0;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

    FSlateApplication::Get().SetCursorPos(
        FVector2D(static_cast<float>(X), static_cast<float>(Y)));
    bHandledBySlate = true;
    bSuccess = true;
    Message = FString::Printf(TEXT("Mouse moved to (%f, %f)"), X, Y);
  } else {
    Message = FString::Printf(
        TEXT("Unknown input type: %s. Supported: key_down, key_up, mouse_click, mouse_move"),
        *InputType);
  }
}
#endif
