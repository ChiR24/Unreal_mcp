#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_UiHandlersPrivate.h"

#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"

#if WITH_EDITOR
namespace McpUiHandlers {

bool HandleEditorControlAction(const FString &LowerSub,
                               const TSharedPtr<FJsonObject> &Payload,
                               const TSharedPtr<FJsonObject> &Resp,
                               bool &bSuccess, FString &Message,
                               FString &ErrorCode) {
  if (LowerSub == TEXT("play_in_editor")) {
    if (GEditor && GEditor->PlayWorld) {
      Message = TEXT("Already playing in editor");
      ErrorCode = TEXT("ALREADY_PLAYING");
      Resp->SetStringField(TEXT("error"), Message);
      return true;
    }

    const bool bCommandSuccess = GEditor->Exec(nullptr, TEXT("Play In Editor"));
    if (bCommandSuccess) {
      bSuccess = true;
      Message = TEXT("Started play in editor");
      Resp->SetStringField(TEXT("status"), TEXT("playing"));
    } else {
      Message = TEXT("Failed to start play in editor");
      ErrorCode = TEXT("PLAY_FAILED");
      Resp->SetStringField(TEXT("error"), Message);
    }
    return true;
  }

  if (LowerSub == TEXT("stop_play")) {
    if (!(GEditor && GEditor->PlayWorld)) {
      Message = TEXT("Not currently playing in editor");
      ErrorCode = TEXT("NOT_PLAYING");
      Resp->SetStringField(TEXT("error"), Message);
      return true;
    }

    const bool bCommandSuccess =
        GEditor->Exec(nullptr, TEXT("Stop Play In Editor"));
    if (bCommandSuccess) {
      bSuccess = true;
      Message = TEXT("Stopped play in editor");
      Resp->SetStringField(TEXT("status"), TEXT("stopped"));
    } else {
      Message = TEXT("Failed to stop play in editor");
      ErrorCode = TEXT("STOP_FAILED");
      Resp->SetStringField(TEXT("error"), Message);
    }
    return true;
  }

  if (LowerSub == TEXT("save_all")) {
    const bool bCommandSuccess = GEditor->Exec(nullptr, TEXT("Asset Save All"));
    if (bCommandSuccess) {
      bSuccess = true;
      Message = TEXT("Saved all assets");
      Resp->SetStringField(TEXT("status"), TEXT("saved"));
    } else {
      Message = TEXT("Failed to save all assets");
      ErrorCode = TEXT("SAVE_FAILED");
      Resp->SetStringField(TEXT("error"), Message);
    }
    return true;
  }

  if (LowerSub != TEXT("simulate_input")) {
    return false;
  }

  FString KeyName;
  Payload->TryGetStringField(TEXT("keyName"), KeyName);
  if (KeyName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("key"), KeyName);
  }

  FString EventType;
  Payload->TryGetStringField(TEXT("eventType"), EventType);

  const FKey Key{FName(*KeyName)};
  if (!Key.IsValid()) {
    Message = FString::Printf(TEXT("Invalid key name: %s"), *KeyName);
    ErrorCode = TEXT("INVALID_KEY");
    Resp->SetStringField(TEXT("error"), Message);
    return true;
  }

  const uint32 CharacterCode = 0;
  const uint32 KeyCode = 0;
  const bool bIsRepeat = false;
  FModifierKeysState ModifierState;

  if (EventType == TEXT("KeyDown")) {
    FKeyEvent KeyEvent(Key, ModifierState,
                       FSlateApplication::Get().GetUserIndexForKeyboard(),
                       bIsRepeat, CharacterCode, KeyCode);
    FSlateApplication::Get().ProcessKeyDownEvent(KeyEvent);
  } else if (EventType == TEXT("KeyUp")) {
    FKeyEvent KeyEvent(Key, ModifierState,
                       FSlateApplication::Get().GetUserIndexForKeyboard(),
                       bIsRepeat, CharacterCode, KeyCode);
    FSlateApplication::Get().ProcessKeyUpEvent(KeyEvent);
  } else {
    FKeyEvent KeyDownEvent(Key, ModifierState,
                           FSlateApplication::Get().GetUserIndexForKeyboard(),
                           bIsRepeat, CharacterCode, KeyCode);
    FSlateApplication::Get().ProcessKeyDownEvent(KeyDownEvent);

    FKeyEvent KeyUpEvent(Key, ModifierState,
                         FSlateApplication::Get().GetUserIndexForKeyboard(),
                         bIsRepeat, CharacterCode, KeyCode);
    FSlateApplication::Get().ProcessKeyUpEvent(KeyUpEvent);
  }

  bSuccess = true;
  Message = FString::Printf(TEXT("Simulated input for key: %s"), *KeyName);
  return true;
}

}
#endif
