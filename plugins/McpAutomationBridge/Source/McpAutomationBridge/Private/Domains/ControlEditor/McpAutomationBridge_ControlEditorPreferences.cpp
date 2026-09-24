#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"

namespace {
// Editor Preferences and Project Settings live on config-backed UObject CDOs
// (UEditorPerformanceSettings, ULevelEditorPlaySettings, ...), never on console
// variables. set_preferences took a `category` and then ignored it, so the only
// things it could ever set were CVars that happen to share a name with a
// preference -- everything a caller would recognise from Editor Preferences
// answered PREFERENCES_NOT_APPLIED. Chief among them
// bThrottleCPUWhenNotForeground, which leaves the editor at ~3 fps whenever it
// is not the foreground window and makes any timing-sensitive automation
// (driving PIE, sampling a jump arc) impossible to run without stealing the
// person's focus.
UObject *ResolveSettingsObjectForMcp(const FString &Category) {
  if (Category.IsEmpty()) {
    return nullptr;
  }
  const FString Candidates[] = {Category, FString(TEXT("U")) + Category,
                                Category + TEXT("Settings"),
                                FString(TEXT("U")) + Category + TEXT("Settings")};
  for (const FString &Name : Candidates) {
    UClass *SettingsClass =
        FindFirstObject<UClass>(*Name, EFindFirstObjectOptions::NativeFirst);
    if (SettingsClass && SettingsClass->HasAnyClassFlags(CLASS_Config)) {
      return SettingsClass->GetDefaultObject();
    }
  }
  return nullptr;
}

// One preference by reflection. Booleans come off the JSON directly; everything
// else goes through the property's own text import, so numbers, enums, names,
// structs and arrays all work without a branch per type. The write is saved to
// the settings ini and announced with PostEditChangeProperty so whatever reads
// it each frame picks the new value up without an editor restart.
bool ApplySettingsPropertyForMcp(UObject *Settings, const FString &Name,
                                 const TSharedPtr<FJsonValue> &Value) {
  if (!Settings || !Value.IsValid()) {
    return false;
  }
  FProperty *Prop = Settings->GetClass()->FindPropertyByName(FName(*Name));
  if (!Prop) {
    return false;
  }
  void *Address = Prop->ContainerPtrToValuePtr<void>(Settings);
  bool BoolVal = false;
  if (FBoolProperty *BoolProp = CastField<FBoolProperty>(Prop)) {
    if (!Value->TryGetBool(BoolVal)) {
      return false;
    }
    BoolProp->SetPropertyValue(Address, BoolVal);
  } else {
    FString Text;
    double NumVal = 0.0;
    if (!McpHandlerUtils::TryGetJsonValueString(Value, Text)) {
      if (!Value->TryGetNumber(NumVal)) {
        return false;
      }
      Text = LexToString(NumVal);
    }
    if (Prop->ImportText_Direct(*Text, Address, Settings, PPF_None) == nullptr) {
      return false;
    }
  }
  FPropertyChangedEvent Changed(Prop, EPropertyChangeType::ValueSet);
  Settings->PostEditChangeProperty(Changed);
  Settings->SaveConfig();
  return true;
}
} // namespace
#endif

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

  UObject* SettingsObject = ResolveSettingsObjectForMcp(Category);

  const TSharedPtr<FJsonObject>* PrefsPtr = nullptr;
  if (Payload->TryGetObjectField(TEXT("preferences"), PrefsPtr) && PrefsPtr && (*PrefsPtr).IsValid()) {
    for (const auto& Pair : (*PrefsPtr)->Values) {
      const FString PreferenceName(*Pair.Key);
      // Try to set via console variable first
      IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*PreferenceName);
      if (CVar) {
        FString Value;
        if (McpHandlerUtils::TryGetJsonValueString(Pair.Value, Value)) {
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
      } else if (ApplySettingsPropertyForMcp(SettingsObject, PreferenceName, Pair.Value)) {
        AppliedSettings.Add(PreferenceName);
      } else {
        FailedSettings.Add(PreferenceName);
      }
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  if (SettingsObject) {
    Resp->SetStringField(TEXT("settingsClass"), SettingsObject->GetClass()->GetName());
  } else if (!Category.IsEmpty()) {
    Resp->SetStringField(TEXT("settingsClass"), FString());
  }
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

  FString ResponseMessage = TEXT("Preferences updated");
  if (!bPreferencesUpdated) {
    // Name the keys that were not applied (dogfood #142).
    // Name the keys that were not applied (dogfood #142), and say whether the
    // category resolved at all -- an unrecognised category is the difference
    // between "that property does not exist" and "I never looked".
    const FString CategoryHint =
        Category.IsEmpty()
            ? FString(TEXT("; pass category to reach an Editor Preferences or Project Settings class"))
            : (SettingsObject
                   ? FString()
                   : FString::Printf(TEXT("; category '%s' did not resolve to a config class"), *Category));
    ResponseMessage = FString::Printf(TEXT("%s (not applied: %s%s)"),
                                      bAnyPreferenceApplied ? TEXT("Preferences partially updated") : TEXT("No preferences updated"),
                                      *FString::Join(FailedSettings, TEXT(", ")), *CategoryHint);
  }
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

  if (!GEditor || !GEditor->PlayWorld) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NO_ACTIVE_SESSION"),
                              TEXT("set_fixed_delta_time steps a running game; start Play In Editor first (control_editor play)."), nullptr);
    return true;
  }
  if (DeltaTime < 0.0 || DeltaTime > 1.0) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("deltaTime must be between 0 (fixed stepping off) and 1 second"), nullptr);
    return true;
  }

  // There is no r.FixedDeltaTime console variable, so the console command this
  // used to run changed nothing. The engine tick reads the fixed step from FApp
  // (UEngine::UpdateTimeAndHandleMaxTickRate). It is switched off again when
  // PIE ends, so the editor never keeps running on a fixed clock.
  const bool bFixed = DeltaTime > 0.0;
  if (bFixed) {
    FApp::SetFixedDeltaTime(DeltaTime);
  }
  FApp::SetUseFixedTimeStep(bFixed);
  static FDelegateHandle ResetOnEndPie;
  if (bFixed && !ResetOnEndPie.IsValid()) {
    ResetOnEndPie = FEditorDelegates::EndPIE.AddLambda([](const bool) {
      FApp::SetUseFixedTimeStep(false);
      FEditorDelegates::EndPIE.Remove(ResetOnEndPie);
      ResetOnEndPie.Reset();
    });
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("fixedDeltaTime"), DeltaTime);
  Resp->SetBoolField(TEXT("fixedTimeStep"), FApp::UseFixedTimeStep());
  SendAutomationResponse(Socket, RequestId, true,
                         bFixed ? FString::Printf(TEXT("Every frame now advances the game by %g s, whatever the frame took, until PIE stops."), DeltaTime)
                                : FString(TEXT("Fixed stepping off; the game follows real time again.")),
                         Resp, FString());
  return true;
#else
  return false;
#endif
}
