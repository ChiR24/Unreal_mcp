// manage_level set_current_level: the editor's current level, the one new actors are spawned and pasted into, made a
// loaded level of the open world (one of its sublevels, or the open level itself), as the Levels panel's Make Current
// does: the level is made current and, when hidden, shown. UEditorLevelUtils::MakeLevelCurrent is not called: it shows
// the level again even when it is visible, which Modify()s a sublevel's streaming entry and so left the open level
// unsaved after every call, and it answers a locked level with a modal dialog. The current level is editor state that
// nothing saves and undo does not take back, so the reply names the level that was current before.

#include "Domains/Level/McpLevelActions.h"
#include "Domains/Level/Lifecycle/McpLevelPathSafety.h"
#include "Domains/Level/World/McpLevelWorldAccess.h"

#include "Editor.h"
#include "EditorLevelUtils.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "LevelUtils.h"
#include "UObject/Package.h"

namespace McpLevelHandlers {
bool HandleSetCurrentLevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                                 TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const auto Refuse = [&](const FString& Message, const TSharedPtr<FJsonObject>& Detail, const TCHAR* Code) {
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false, Message + TEXT(" The current level is unchanged."),
                                     Detail, Code);
    return true;
  };
  UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  if (!World) {
    return Refuse(TEXT("No level is open in the editor."), nullptr, TEXT("NO_WORLD"));
  }
  // The Levels panel is read only while a game plays or simulates.
  if (GEditor->PlayWorld) {
    return Refuse(TEXT("Play In Editor is running: stop it first."), nullptr, TEXT("PIE_ACTIVE"));
  }
  const FString Given = GetJsonStringField(Payload, TEXT("levelPath")).TrimStartAndEnd();
  const FString Wanted = NormalizeLevelPackagePath(Given);
  const FString OpenLevel = World->PersistentLevel->GetOutermost()->GetName();
  ULevel* Level = World->PersistentLevel;
  if (!Wanted.Equals(OpenLevel, ESearchCase::IgnoreCase) && !Given.Equals(TEXT("persistent"), ESearchCase::IgnoreCase)) {
    // A sublevel is named as add_sublevel, stream and remove_sublevel name it; a refusal lists the loaded levels.
    const TArray<ULevelStreaming*> Matches = FindStreamingLevels(World, Wanted);
    TArray<FString> Loaded;
    for (const ULevel* Each : GetAllLevelsFromWorld(World)) {
      Loaded.Add(Each->GetOutermost()->GetName());
    }
    TSharedPtr<FJsonObject> Levels = McpHandlerUtils::CreateResultObject();
    Levels->SetArrayField(TEXT("levels"), McpHandlerUtils::ToJsonStringArray(Loaded));
    if (Matches.Num() != 1) {
      return Refuse(FString::Printf(TEXT("%s is %s of the open level %s (its loaded levels: %s)."), *Wanted,
                                    Matches.Num() > 1 ? TEXT("more than one sublevel") : TEXT("not a sublevel"), *OpenLevel,
                                    *FString::Join(Loaded, TEXT(", "))),
                    Levels, Matches.Num() > 1 ? TEXT("AMBIGUOUS_LEVEL") : TEXT("SUBLEVEL_NOT_FOUND"));
    }
    // Actors spawned into a Level Instance's level outside its edit session are never saved.
    if (!IsUserSublevel(Matches[0])) {
      return Refuse(FString::Printf(TEXT("%s is a Level Instance's level (or one streamed in at run time), not a sublevel of %s: "
                                         "edit its Level Instance to add actors to it."), *Wanted, *OpenLevel),
                    Levels, TEXT("NOT_A_SUBLEVEL"));
    }
    Level = Matches[0]->GetLoadedLevel();
    if (!Level) {
      return Refuse(FString::Printf(TEXT("%s is in the open level's streaming list but did not load in the editor (its map "
                                         "file may be missing or broken)."), *Wanted),
                    Levels, TEXT("LEVEL_NOT_LOADED"));
    }
  }
  const FString LevelPath = Level->GetOutermost()->GetName();
  const ULevel* Previous = World->GetCurrentLevel();
  const FString PreviousPath = Previous ? Previous->GetOutermost()->GetName() : FString();
  const bool bChanged = Level != Previous;
  const bool bShow = !Level->bIsVisible;
  if (bChanged || bShow) {
    // The editor never makes a locked level current.
    if (FLevelUtils::IsLevelLocked(Level)) {
      return Refuse(FString::Printf(TEXT("%s is locked; unlock it in the Levels panel first."), *LevelPath), nullptr,
                    TEXT("LEVEL_LOCKED"));
    }
    if (World->SetCurrentLevel(Level)) {
      FEditorDelegates::NewCurrentLevel.Broadcast();
    }
    // Only a hidden level is shown, as the Levels panel shows the level it makes current.
    if (bShow) {
      UEditorLevelUtils::SetLevelVisibility(Level, true, false);
    }
  }
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("previousLevel"), PreviousPath);
  Result->SetStringField(TEXT("currentLevel"), LevelPath);
  Result->SetBoolField(TEXT("shown"), bShow);
  const FString What = bChanged
      ? FString::Printf(TEXT("%s is the current level now, in place of %s: new actors are spawned and pasted into it"),
                        *LevelPath, *PreviousPath)
      : FString::Printf(TEXT("%s was the current level already"), *LevelPath);
  Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
      What + (bShow ? TEXT("; it was hidden and is shown now.") : bChanged ? TEXT(".") : TEXT("; nothing changed.")), Result);
  return true;
}
} // namespace McpLevelHandlers
