#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"

// A raw Slate key is not input as far as Enhanced Input is concerned: the key
// reaches the PlayerController, nothing is bound to it directly, and the call
// answers "delivered to PIE" while the pawn never moves. Driving such a game
// means injecting the InputAction itself, which is what InjectInputForAction
// exists for. One inject lasts a single frame, so a hold has to re-inject every
// tick or the pawn twitches a few units and stops.
#if WITH_EDITOR && __has_include("EnhancedInputSubsystems.h") && \
    __has_include("InputAction.h")
#define MCP_HAS_ENHANCED_INPUT_INJECT 1
#include "Containers/Ticker.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#else
#define MCP_HAS_ENHANCED_INPUT_INJECT 0
#endif

#if MCP_HAS_ENHANCED_INPUT_INJECT
namespace {
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

  // Weak on both sides: stopping PIE tears down the subsystem, and an injector
  // that kept a hard reference would hold a dead world alive and keep pushing
  // input into it.
  TWeakObjectPtr<UInputAction> WeakAction(Action);
  const double EndTime = FPlatformTime::Seconds() + HoldSeconds;
  const FTSTicker::FDelegateHandle Handle = FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateLambda([WeakAction, Value, EndTime,
                                     ActionPath](float) -> bool {
        UEnhancedInputLocalPlayerSubsystem *Live = ResolveInputSubsystemForMcp();
        if (Live == nullptr || !WeakAction.IsValid() ||
            FPlatformTime::Seconds() >= EndTime) {
          McpActionHolds().Remove(ActionPath);
          return false;
        }
        Live->InjectInputForAction(WeakAction.Get(), Value, {}, {});
        return true;
      }),
      0.0f);
  McpActionHolds().Add(ActionPath, Handle);
  Message = FString::Printf(TEXT("Injecting %s = %s for %.2fs"),
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
    SimulateEditorInputForMcp(InputType, Key, Payload, bSuccess, bRoutedToPIE,
                              bHandledByPIE, bHandledBySlate, Message);
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
                             "sent the raw key instead, which an Enhanced "
                             "Input game ignores."),
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
