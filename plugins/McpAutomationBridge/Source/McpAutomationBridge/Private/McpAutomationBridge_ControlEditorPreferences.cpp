#include "McpAutomationBridge_ControlEditorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetPreferences(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  TArray<FString> AppliedSettings;
  TArray<FString> FailedSettings;
  FString Category;
  Payload->TryGetStringField(TEXT("category"), Category);

  // Get preferences object from payload
  const TSharedPtr<FJsonObject>* PrefsPtr = nullptr;
  if (Payload->TryGetObjectField(TEXT("preferences"), PrefsPtr) && PrefsPtr && (*PrefsPtr).IsValid()) {
    for (const auto& Pair : (*PrefsPtr)->Values) {
      const FString PreferenceName(*Pair.Key);
      // Try to set via console variable first
      IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*PreferenceName);
      if (CVar) {
        FString Value;
        if (Pair.Value->TryGetString(Value)) {
          CVar->Set(*Value);
          AppliedSettings.Add(PreferenceName);
        } else {
          double NumVal;
          if (Pair.Value->TryGetNumber(NumVal)) {
            CVar->Set((float)NumVal);
            AppliedSettings.Add(PreferenceName);
          } else {
            bool BoolVal;
            if (Pair.Value->TryGetBool(BoolVal)) {
              CVar->Set(BoolVal ? 1 : 0);
              AppliedSettings.Add(PreferenceName);
            } else {
              FailedSettings.Add(PreferenceName);
            }
          }
        }
      } else if (Category.Equals(TEXT("LevelEditor"), ESearchCase::IgnoreCase) && PreferenceName.Equals(TEXT("RealtimeAudio"), ESearchCase::IgnoreCase)) {
        bool BoolVal;
        if (Pair.Value->TryGetBool(BoolVal)) {
          GEditor->MuteRealTimeAudio(!BoolVal);
          AppliedSettings.Add(PreferenceName);
        } else {
          FailedSettings.Add(PreferenceName);
        }
      } else {
        FailedSettings.Add(PreferenceName);
      }
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  const bool bAnyPreferenceApplied = AppliedSettings.Num() > 0;
  const bool bPreferencesUpdated = bAnyPreferenceApplied && FailedSettings.Num() == 0;
  Resp->SetBoolField(TEXT("success"), bPreferencesUpdated);
  Resp->SetNumberField(TEXT("appliedCount"), AppliedSettings.Num());

  if (AppliedSettings.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> AppliedArray;
    for (const FString& Name : AppliedSettings)
      AppliedArray.Add(MakeShared<FJsonValueString>(Name));
    Resp->SetArrayField(TEXT("applied"), AppliedArray);
  }

  if (FailedSettings.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> FailedArray;
    for (const FString& Name : FailedSettings)
      FailedArray.Add(MakeShared<FJsonValueString>(Name));
    Resp->SetArrayField(TEXT("failed"), FailedArray);
  }

  const FString ResponseMessage = bPreferencesUpdated
      ? TEXT("Preferences updated")
      : (bAnyPreferenceApplied ? TEXT("Preferences partially updated") : TEXT("No preferences updated"));
  const FString ResponseErrorCode = bPreferencesUpdated
      ? FString()
      : (bAnyPreferenceApplied ? FString(TEXT("PREFERENCES_PARTIALLY_APPLIED")) : FString(TEXT("PREFERENCES_NOT_APPLIED")));
  SendAutomationResponse(Socket, RequestId, bPreferencesUpdated,
                         ResponseMessage, Resp, ResponseErrorCode);
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Preferences require editor build."), nullptr);
  return true;
#endif
}


bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetEditorMode(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString Mode;
  Payload->TryGetStringField(TEXT("mode"), Mode);
  if (Mode.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("mode required"), nullptr);
    return true;
  }

  if (!IsSafeConsoleArgumentToken(Mode)) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("Invalid editor mode"), nullptr);
    return true;
  }

  // Execute editor mode command via console
  FString Command = FString::Printf(TEXT("mode %s"), *Mode);
  GEditor->Exec(GEditor->GetEditorWorldContext().World(), *Command);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("mode"), Mode);
  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Editor mode set to %s"), *Mode), Resp, FString());
  return true;
#else
  return false;
#endif
}


bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetFixedDeltaTime(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  double DeltaTime = 0.01667; // Default ~60fps
  if (Payload->HasField(TEXT("deltaTime"))) {
    TSharedPtr<FJsonValue> Value = Payload->TryGetField(TEXT("deltaTime"));
    if (Value.IsValid() && Value->Type == EJson::Number) {
      DeltaTime = Value->AsNumber();
    }
  }

  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  // Set fixed delta time via console
  FString Command = FString::Printf(TEXT("r.FixedDeltaTime %f"), DeltaTime);
  GEditor->Exec(GEditor->GetEditorWorldContext().World(), *Command);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("fixedDeltaTime"), DeltaTime);
  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Fixed delta time set to %f"), DeltaTime), Resp, FString());
  return true;
#else
  return false;
#endif
}
