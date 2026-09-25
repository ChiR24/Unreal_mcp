#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"

// A raw key reaches Enhanced Input only through a mapping context that maps
// it. An action no key is mapped to (or an analog value) has to be injected as
// the InputAction itself, which is what InjectInputForAction exists for. One
// inject lasts a single frame, so a hold has to re-inject every tick or the
// pawn twitches a few units and stops.
#if WITH_EDITOR && __has_include("EnhancedInputSubsystems.h") && \
    __has_include("InputAction.h")
#define MCP_HAS_ENHANCED_INPUT_INJECT 1
#include "Containers/Ticker.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#else
#define MCP_HAS_ENHANCED_INPUT_INJECT 0
#endif

#if WITH_EDITOR
#include "Containers/Ticker.h"
namespace {
// A raw key held for holdSeconds of GAME time, then released. key_down used to
// ignore holdSeconds entirely, so "hold D for 1.2 s" pressed D and never let
// go: the pawn ran on into the next pit. One pending release per key; a key_up
// or a newer hold of the same key replaces it.
TMap<FString, FTSTicker::FDelegateHandle> &McpKeyReleases() {
  static TMap<FString, FTSTicker::FDelegateHandle> Releases;
  return Releases;
}

void CancelKeyReleaseForMcp(const FString &Key) {
  if (FTSTicker::FDelegateHandle *Pending = McpKeyReleases().Find(Key)) {
    FTSTicker::GetCoreTicker().RemoveTicker(*Pending);
    McpKeyReleases().Remove(Key);
  }
}

// Game seconds in PIE, like the action hold; wall seconds for an editor key.
// A PIE world that reloaded or stopped releases at once instead of timing the
// hold against a fresh clock.
void ScheduleKeyReleaseForMcp(const FString &Key, double HoldSeconds,
                              const TSharedPtr<FJsonObject> &Payload) {
  CancelKeyReleaseForMcp(Key);
  UWorld *World = GEditor != nullptr ? GEditor->PlayWorld.Get() : nullptr;
  const TWeakObjectPtr<UWorld> HoldWorld(World);
  const bool bGameTime = World != nullptr;
  const double EndGameTime = bGameTime ? World->GetTimeSeconds() + HoldSeconds : 0.0;
  const double EndWallTime = FPlatformTime::Seconds() + HoldSeconds + (bGameTime ? 600.0 : 0.0);
  // Never release before the game has seen two frames with the key down: a
  // throttled 3 fps editor otherwise pressed and released a key_tap inside one
  // frame and the game never saw it.
  const uint64 MinReleaseFrame = GFrameCounter + 2;
  McpKeyReleases().Add(Key, FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateLambda([Key, Payload, HoldWorld, bGameTime, EndGameTime,
                                     EndWallTime, MinReleaseFrame](float) -> bool {
        UWorld *Live = GEditor != nullptr ? GEditor->PlayWorld.Get() : nullptr;
        const bool bWorldGone = bGameTime && (Live == nullptr || Live != HoldWorld.Get());
        const bool bHolding = !bWorldGone &&
            (GFrameCounter < MinReleaseFrame ||
             (FPlatformTime::Seconds() < EndWallTime &&
              (!bGameTime || Live->GetTimeSeconds() < EndGameTime)));
        if (bHolding) {
          return true;
        }
        McpKeyReleases().Remove(Key);
        bool bOk = false, bRouted = false, bPIE = false, bSlate = false;
        FString Unused;
        SimulateEditorInputForMcp(TEXT("key_up"), Key, Payload, bOk, bRouted, bPIE, bSlate, Unused);
        return false;
      }),
      0.0f));
}
} // namespace
#endif

#if MCP_HAS_ENHANCED_INPUT_INJECT
namespace {
// Wall-clock headroom on top of the requested game seconds. Slowing the clock
// stretches a hold in real time, so this has to be generous enough not to cut a
// legitimate dilated hold short while still ending one whose world stopped
// ticking; a paused world advances neither clock, and the grace is what frees
// the ticker then.
constexpr double McpHoldRealTimeGraceSeconds = 600.0;

// One live hold per action, so a second call for the same action replaces the
// first instead of stacking two injectors that fight over the same axis.
TMap<FString, FTSTicker::FDelegateHandle> &McpActionHolds() {
  static TMap<FString, FTSTicker::FDelegateHandle> Holds;
  return Holds;
}

void StopActionHoldForMcp(const FString &ActionPath) {
  if (FTSTicker::FDelegateHandle *Existing = McpActionHolds().Find(ActionPath)) {
    FTSTicker::GetCoreTicker().RemoveTicker(*Existing);
    McpActionHolds().Remove(ActionPath);
  }
}

UEnhancedInputLocalPlayerSubsystem *ResolveInputSubsystemForMcp() {
  UWorld *PlayWorld = GEditor != nullptr ? GEditor->PlayWorld.Get() : nullptr;
  if (PlayWorld == nullptr) {
    return nullptr;
  }
  APlayerController *Controller = PlayWorld->GetFirstPlayerController();
  ULocalPlayer *Player =
      Controller != nullptr ? Controller->GetLocalPlayer() : nullptr;
  return Player != nullptr
             ? Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()
             : nullptr;
}

// The action declares what shape its value is; injecting a float into an
// Axis2D action reads as zero on the Y axis rather than failing, so build the
// value the action actually asked for.
FInputActionValue BuildActionValueForMcp(const UInputAction *Action,
                                         const TSharedPtr<FJsonObject> &Payload,
                                         double Scalar) {
  double X = Scalar;
  double Y = 0.0;
  double Z = 0.0;
  Payload->TryGetNumberField(TEXT("x"), X);
  Payload->TryGetNumberField(TEXT("y"), Y);
  Payload->TryGetNumberField(TEXT("z"), Z);
  switch (Action->ValueType) {
  case EInputActionValueType::Boolean:
    return FInputActionValue(!FMath::IsNearlyZero(Scalar));
  case EInputActionValueType::Axis2D:
    return FInputActionValue(FVector2D(X, Y));
  case EInputActionValueType::Axis3D:
    return FInputActionValue(FVector(X, Y, Z));
  default:
    return FInputActionValue(static_cast<float>(Scalar));
  }
}

void InjectActionForMcp(const TSharedPtr<FJsonObject> &Payload,
                        const FString &InputType, const FString &ActionPath,
                        UInputAction *Action, bool &bSuccess,
                        bool &bRoutedToPIE, bool &bHandledByPIE,
                        FString &Message) {
  UEnhancedInputLocalPlayerSubsystem *Subsystem = ResolveInputSubsystemForMcp();
  if (Subsystem == nullptr) {
    Message = TEXT("No Enhanced Input subsystem - start PIE first");
    return;
  }
  bRoutedToPIE = true;

  StopActionHoldForMcp(ActionPath);
  const bool bRelease =
      InputType == TEXT("key_up") || InputType == TEXT("keyup");
  double Scalar = 1.0;
  Payload->TryGetNumberField(TEXT("value"), Scalar);
  if (bRelease) {
    Scalar = 0.0;
  }
  const FInputActionValue Value = BuildActionValueForMcp(Action, Payload, Scalar);
  Subsystem->InjectInputForAction(Action, Value, {}, {});
  bSuccess = true;
  bHandledByPIE = true;

  double HoldSeconds = 0.0;
  Payload->TryGetNumberField(TEXT("holdSeconds"), HoldSeconds);
  if (bRelease || HoldSeconds <= 0.0) {
    Message = FString::Printf(TEXT("Injected %s = %s"), *Action->GetName(),
                              *Value.ToString());
    return;
  }

  // holdSeconds counts GAME seconds, not wall seconds. A caller driving
  // gameplay has usually slowed the clock with set_game_speed to get a usable
  // sampling rate, and a wall-clock hold would then be as short as the dilation
  // -- 2 s asked for becomes 0.1 s of game at 0.05. UWorld::GetTimeSeconds is
  // dilated; RealTimeSeconds is not, so it also backstops a paused world, where
  // game time stops and the hold would otherwise never expire.
  //
  // Weak on both sides: stopping PIE tears down the subsystem, and an injector
  // that kept a hard reference would hold a dead world alive and keep pushing
  // input into it.
  TWeakObjectPtr<UInputAction> WeakAction(Action);
  UWorld *HoldWorld = GEditor != nullptr ? GEditor->PlayWorld.Get() : nullptr;
  const double EndGameTime =
      (HoldWorld != nullptr ? HoldWorld->GetTimeSeconds() : 0.0) + HoldSeconds;
  const double EndRealTime =
      (HoldWorld != nullptr ? HoldWorld->GetRealTimeSeconds() : 0.0) +
      HoldSeconds + McpHoldRealTimeGraceSeconds;
  const FTSTicker::FDelegateHandle Handle = FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateLambda([WeakAction, Value, EndGameTime,
                                     EndRealTime, ActionPath](float) -> bool {
        UEnhancedInputLocalPlayerSubsystem *Live = ResolveInputSubsystemForMcp();
        UWorld *LiveWorld = GEditor != nullptr ? GEditor->PlayWorld.Get() : nullptr;
        const bool bExpired =
            LiveWorld == nullptr ||
            LiveWorld->GetTimeSeconds() >= EndGameTime ||
            LiveWorld->GetRealTimeSeconds() >= EndRealTime;
        if (Live == nullptr || !WeakAction.IsValid() || bExpired) {
          McpActionHolds().Remove(ActionPath);
          return false;
        }
        Live->InjectInputForAction(WeakAction.Get(), Value, {}, {});
        return true;
      }),
      0.0f);
  McpActionHolds().Add(ActionPath, Handle);
  Message = FString::Printf(TEXT("Injecting %s = %s for %.2fs of game time"),
                            *Action->GetName(), *Value.ToString(), HoldSeconds);
}
} // namespace
#endif

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSimulateInput(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  const FString InputType = NormalizeSimulatedInputTypeForMcp(Payload);
  if (InputType == TEXT("widget_list") || InputType == TEXT("widget_click")) {
    TSharedPtr<FJsonObject> WidgetResp = McpHandlerUtils::CreateResultObject();
    FString WidgetMessage;
    const bool bDriven =
        SimulateLiveWidgetInputForMcp(InputType, Payload, WidgetResp, WidgetMessage);
    WidgetResp->SetBoolField(TEXT("success"), bDriven);
    WidgetResp->SetStringField(TEXT("type"), InputType);
    WidgetResp->SetStringField(TEXT("message"), WidgetMessage);
    if (bDriven) {
      SendAutomationResponse(Socket, RequestId, true, WidgetMessage, WidgetResp, FString());
    } else {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("WIDGET_INPUT_FAILED"),
                                WidgetMessage, WidgetResp);
    }
    return true;
  }
  FString Key;
  Payload->TryGetStringField(TEXT("key"), Key);

  bool bSuccess = false;
  bool bRoutedToPIE = false;
  bool bHandledByPIE = false;
  bool bHandledBySlate = false;
  FString Message;
  FString InjectedAction;

  FString ActionPath;
  Payload->TryGetStringField(TEXT("inputAction"), ActionPath);
#if MCP_HAS_ENHANCED_INPUT_INJECT
  // Only an inputAction that names a real asset takes this path; the field has
  // long doubled as an alias for `type`, and those spellings must keep working.
  UInputAction *Action =
      ActionPath.IsEmpty() ? nullptr
                           : LoadObject<UInputAction>(nullptr, *ActionPath);
  if (Action != nullptr) {
    InjectedAction = Action->GetPathName();
    InjectActionForMcp(Payload, InputType, ActionPath, Action, bSuccess,
                       bRoutedToPIE, bHandledByPIE, Message);
  } else
#endif
  {
    // key_tap presses and lets go after holdSeconds (default a tenth of a game
    // second: a press and release inside one frame never reaches an action).
    const bool bTap = InputType == TEXT("key_tap");
    const bool bPress = bTap || InputType == TEXT("key_down") || InputType == TEXT("keydown");
    double HoldSeconds = 0.0;
    Payload->TryGetNumberField(TEXT("holdSeconds"), HoldSeconds);
    if (bTap && HoldSeconds <= 0.0) {
      HoldSeconds = 0.1;
    }
    if (!bPress) {
      CancelKeyReleaseForMcp(Key);
    }
    SimulateEditorInputForMcp(bTap ? FString(TEXT("key_down")) : InputType, Key, Payload,
                              bSuccess, bRoutedToPIE, bHandledByPIE, bHandledBySlate, Message);
    if (bSuccess && bPress && HoldSeconds > 0.0) {
      ScheduleKeyReleaseForMcp(Key, HoldSeconds, Payload);
      Message += FString::Printf(TEXT(", released after %.2fs"), HoldSeconds);
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), bSuccess);
  Resp->SetStringField(TEXT("type"), InputType);
  Resp->SetStringField(TEXT("message"), Message);
  Resp->SetBoolField(TEXT("routedToPIE"), bRoutedToPIE);
  Resp->SetBoolField(TEXT("handledByPIE"), bHandledByPIE);
  Resp->SetBoolField(TEXT("handledBySlate"), bHandledBySlate);
  if (!InjectedAction.IsEmpty()) {
    Resp->SetStringField(TEXT("injectedAction"), InjectedAction);
  } else if (!ActionPath.IsEmpty()) {
    Resp->SetStringField(
        TEXT("warning"),
        FString::Printf(TEXT("inputAction '%s' is not an InputAction asset; "
                             "sent the raw key instead, which reaches the "
                             "game only if a mapping context maps it."),
                        *ActionPath));
  }
  AddSimulatedInputDiagnosticsForMcp(Key, Resp);

  if (bSuccess) {
    SendAutomationResponse(Socket, RequestId, true, Message, Resp, FString());
  } else {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INPUT_FAILED"),
                              Message, Resp);
  }
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                            TEXT("Simulate input requires editor build."), nullptr);
  return true;
#endif
}

#if WITH_EDITOR
void StopAllEnhancedInputHoldsForMcp() {
  for (const TPair<FString, FTSTicker::FDelegateHandle> &Release : McpKeyReleases()) {
    FTSTicker::GetCoreTicker().RemoveTicker(Release.Value);
  }
  McpKeyReleases().Empty();
#if MCP_HAS_ENHANCED_INPUT_INJECT
  for (const TPair<FString, FTSTicker::FDelegateHandle> &Hold :
       McpActionHolds()) {
    FTSTicker::GetCoreTicker().RemoveTicker(Hold.Value);
  }
  McpActionHolds().Empty();
#endif
}
#endif
