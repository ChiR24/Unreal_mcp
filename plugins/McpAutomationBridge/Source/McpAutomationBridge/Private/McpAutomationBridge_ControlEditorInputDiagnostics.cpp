#include "McpAutomationBridge_ControlEditorSupport.h"

#if WITH_EDITOR
void AddSimulatedInputDiagnosticsForMcp(const FString &Key,
                                        const TSharedPtr<FJsonObject> &Resp) {
  if (Key.IsEmpty() || !GEditor || !GEditor->PlayWorld) {
    return;
  }

  UWorld *PlayWorld = GEditor->PlayWorld.Get();
  if (!PlayWorld) {
    return;
  }

  APlayerController *PlayerController = PlayWorld->GetFirstPlayerController();
  if (!PlayerController) {
    return;
  }

  TSharedPtr<FJsonObject> InputDiagnostics = MakeShared<FJsonObject>();
  InputDiagnostics->SetStringField(TEXT("playerController"),
                                   PlayerController->GetName());
  if (APawn *Pawn = PlayerController->GetPawn()) {
    InputDiagnostics->SetStringField(TEXT("pawn"), Pawn->GetName());
    InputDiagnostics->SetStringField(
        TEXT("pendingInputVector"),
        Pawn->GetPendingMovementInputVector().ToString());
    InputDiagnostics->SetStringField(
        TEXT("lastInputVector"), Pawn->GetLastMovementInputVector().ToString());

    if (UInputComponent *PawnInput = Pawn->InputComponent) {
      TArray<TSharedPtr<FJsonValue>> AxisBindings;
      for (const FInputAxisBinding &AxisBinding : PawnInput->AxisBindings) {
        TSharedPtr<FJsonObject> AxisBindingObject = MakeShared<FJsonObject>();
        AxisBindingObject->SetStringField(TEXT("axisName"),
                                          AxisBinding.AxisName.ToString());
        AxisBindingObject->SetNumberField(TEXT("axisValue"),
                                          AxisBinding.AxisValue);
        AxisBindingObject->SetBoolField(TEXT("delegateBound"),
                                        AxisBinding.AxisDelegate.IsBound());
        AxisBindingObject->SetBoolField(TEXT("consumeInput"),
                                        AxisBinding.bConsumeInput);
        AxisBindingObject->SetBoolField(TEXT("executeWhenPaused"),
                                        AxisBinding.bExecuteWhenPaused);
        AxisBindings.Add(MakeShared<FJsonValueObject>(AxisBindingObject));
      }
      InputDiagnostics->SetStringField(TEXT("pawnInputComponent"),
                                       PawnInput->GetName());
      InputDiagnostics->SetStringField(TEXT("pawnInputComponentClass"),
                                       PawnInput->GetClass()->GetName());
      InputDiagnostics->SetNumberField(TEXT("pawnAxisBindingCount"),
                                       AxisBindings.Num());
      InputDiagnostics->SetArrayField(TEXT("pawnAxisBindings"), AxisBindings);
    }
  }

  if (PlayerController->PlayerInput) {
    const FKey DiagnosticKey(*Key);
    const TArray<FInputAxisKeyMapping> &MoveForwardKeys =
        PlayerController->PlayerInput->GetKeysForAxis(TEXT("MoveForward"));
    const TArray<FInputAxisKeyMapping> &MoveRightKeys =
        PlayerController->PlayerInput->GetKeysForAxis(TEXT("MoveRight"));
    InputDiagnostics->SetNumberField(
        TEXT("keyValue"), PlayerController->PlayerInput->GetKeyValue(DiagnosticKey));
    InputDiagnostics->SetNumberField(TEXT("moveForwardMappingCount"),
                                     MoveForwardKeys.Num());
    InputDiagnostics->SetNumberField(TEXT("moveRightMappingCount"),
                                     MoveRightKeys.Num());
  }

  Resp->SetObjectField(TEXT("inputDiagnostics"), InputDiagnostics);
}
#endif
