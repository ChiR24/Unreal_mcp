#include "Domains/Level/McpLevelActions.h"
#include "Domains/Level/Lifecycle/McpLevelPathSafety.h"
#include "Domains/Level/World/McpLevelWorldAccess.h"

#include "Editor.h"
#include "Editor/Transactor.h"
#include "EditorLevelUtils.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "GameFramework/Actor.h"
#include "LevelUtils.h"
#include "UObject/Package.h"

namespace McpLevelHandlers {
namespace {
// What a removal would drop unsaved: the level's own package and, for a level that saves its actors one file each,
// theirs (an actor's package is the level's when it has none of its own).
TArray<FString> UnsavedPackagesOf(ULevel* Level) {
  TArray<FString> Out;
  auto Note = [&Out](const UPackage* Package) {
    if (Package && Package->IsDirty()) {
      Out.AddUnique(Package->GetName());
    }
  };
  Note(Level->GetOutermost());
  for (AActor* Actor : Level->Actors) {
    if (Actor) {
      Note(Actor->GetPackage());
    }
  }
  return Out;
}

// A Level Instance's level (or one loaded at run time) streams through a transient entry the open level never saves:
// the Levels panel leaves it out, and removing it would pull the level from under its Level Instance actor.
bool IsUserSublevel(const ULevelStreaming* Streaming) {
  return !Streaming->HasAnyFlags(RF_Transient) && !Streaming->GetClass()->HasAnyClassFlags(CLASS_Transient);
}
} // namespace

bool HandleRemoveSublevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Given = McpGetFirstStringField(Payload, {TEXT("subLevelPath"), TEXT("levelPath")}).TrimStartAndEnd();
  if (Given.IsEmpty()) {
    Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("subLevelPath required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  // An object path or a .umap name reads as its package, as stream takes it; a bare name finds the one sublevel whose
  // path ends with it. Only the open level's own sublevels can match, so nothing on disk is reached by it.
  const FString SubLevelPath = NormalizeLevelPackagePath(Given);
  UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  if (!World) {
    Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("No level is open in the editor"), TEXT("NO_WORLD"));
    return true;
  }
  const FString OpenLevel = World->GetOutermost()->GetName();
  if (SubLevelPath.Equals(OpenLevel, ESearchCase::IgnoreCase)) {
    Subsystem.SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("%s is the open level itself, not one of its sublevels; load another level to leave it"), *OpenLevel),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("world"), World->GetName());
  const TArray<ULevelStreaming*> Matches = FindStreamingLevels(World, SubLevelPath);
  if (Matches.Num() != 1) {
    TArray<FString> Paths;
    for (const ULevelStreaming* Streaming : Matches) {
      Paths.Add(Streaming->GetWorldAssetPackageName());
    }
    if (Matches.Num() == 0) {
      for (const ULevelStreaming* Streaming : World->GetStreamingLevels()) {
        if (Streaming && IsUserSublevel(Streaming)) {
          Paths.Add(Streaming->GetWorldAssetPackageName());
        }
      }
    }
    Result->SetArrayField(Matches.Num() > 1 ? TEXT("matches") : TEXT("sublevels"), McpHandlerUtils::ToJsonStringArray(Paths));
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
        Matches.Num() > 1
            ? FString::Printf(TEXT("%s names %d sublevels (%s); pass the full package path"), *SubLevelPath, Paths.Num(),
                              *FString::Join(Paths, TEXT(", ")))
            : FString::Printf(TEXT("%s is not a sublevel of the open level %s (its sublevels: %s)"), *SubLevelPath, *OpenLevel,
                              Paths.Num() > 0 ? *FString::Join(Paths, TEXT(", ")) : TEXT("none")),
        Result, Matches.Num() > 1 ? TEXT("AMBIGUOUS_LEVEL") : TEXT("SUBLEVEL_NOT_FOUND"));
    return true;
  }
  ULevelStreaming* Streaming = Matches[0];
  const FString Path = Streaming->GetWorldAssetPackageName();
  Result->SetStringField(TEXT("subLevelPath"), Path);
  if (!IsUserSublevel(Streaming)) {
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
        FString::Printf(TEXT("%s is streamed in by a Level Instance (or at run time), not a sublevel of %s; delete its Level "
                             "Instance actor instead. Nothing was removed."), *Path, *OpenLevel),
        Result, TEXT("NOT_A_SUBLEVEL"));
    return true;
  }
  ULevel* Level = Streaming->GetLoadedLevel();

  TArray<FString> Unsaved;
  if (Level) {
    // The engine answers a locked level with a modal dialog, which would hold the editor until someone closes it.
    if (FLevelUtils::IsLevelLocked(Level)) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
          FString::Printf(TEXT("%s is locked; unlock it in the Levels panel first. Nothing was removed."), *Path),
          Result, TEXT("LEVEL_LOCKED"));
      return true;
    }
    Unsaved = UnsavedPackagesOf(Level);
    bool bDiscardUnsaved = false;
    Payload->TryGetBoolField(TEXT("discardUnsaved"), bDiscardUnsaved);
    if (Unsaved.Num() > 0 && !bDiscardUnsaved) {
      Result->SetArrayField(TEXT("unsavedPackages"), McpHandlerUtils::ToJsonStringArray(Unsaved));
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
          FString::Printf(TEXT("%s has unsaved changes (%s) a removal would lose; save them (control_editor save_all) or pass "
                               "discardUnsaved true. Nothing was removed."), *Path, *FString::Join(Unsaved, TEXT(", "))),
          Result, TEXT("DIRTY_PACKAGES"));
      return true;
    }
  }

  // bResetTransBuffer false: with true, a reference still held to the removed world is Fatal (an undo step that
  // moved one of its actors is one); with false the engine clears the undo history only when it must and logs what
  // held it. Never undoable, as in the Levels panel.
  const int32 QueueBefore = GEditor->Trans ? GEditor->Trans->GetQueueLength() : 0;
  const bool bRemoved = Level ? UEditorLevelUtils::RemoveLevelFromWorld(Level, true, false)
                              : UEditorLevelUtils::RemoveInvalidLevelFromWorld(Streaming);
  if (!bRemoved || FindStreamingLevels(World, Path).Num() > 0) {
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
        FString::Printf(TEXT("The editor did not remove %s from %s (see the log)"), *Path, *OpenLevel), Result,
        TEXT("REMOVE_FAILED"));
    return true;
  }
  // The open level's list of sublevels changed: it has to be saved for the removal to last.
  World->MarkPackageDirty();
  Result->SetBoolField(TEXT("wasLoaded"), Level != nullptr);
  if (Unsaved.Num() > 0) {
    Result->SetArrayField(TEXT("discardedPackages"), McpHandlerUtils::ToJsonStringArray(Unsaved));
  }
  // When the editor had to clear its undo history to let the level go, the undo ledger says so (UNDO_HISTORY_CLEARED).
  if (!GEditor->Trans || GEditor->Trans->GetQueueLength() >= QueueBefore) {
    TSharedPtr<FJsonObject> Undo = MakeShared<FJsonObject>();
    Undo->SetBoolField(TEXT("undoable"), false);
    Undo->SetStringField(TEXT("reasonCode"), TEXT("UNDO_UNAVAILABLE_LEVEL_REMOVAL"));
    Undo->SetStringField(TEXT("reason"), TEXT("The editor removes a level outside its undo history, as the Levels panel "
                                              "does; add_sublevel adds it back from its saved map."));
    Result->SetObjectField(TEXT("undo"), Undo);
  }
  Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
      FString::Printf(TEXT("Removed %s from %s; its map file is kept. Save the open level (manage_level save) to keep the removal."),
                      *Path, *OpenLevel),
      Result);
  return true;
}
} // namespace McpLevelHandlers
