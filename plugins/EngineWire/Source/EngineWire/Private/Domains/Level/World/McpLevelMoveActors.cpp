// manage_level move_actors_to_level: placed actors moved into another loaded level of the open world (a sublevel, or the
// open level itself), as the Levels panel's Move Selection to Level does. UEditorLevelUtils::MoveActorsToLevel does it
// (the same call in every 5.x): it cuts and pastes the actors (keeping the user's clipboard), fixes soft references to
// them, and is one undo step of its own, which the reply's undo block names. Its two warning dialogs are turned off, so
// nothing waits on a person; every name and the level are checked before anything moves, and the move is all or none.

#include "Domains/Level/McpLevelActions.h"
#include "Domains/Level/Lifecycle/McpLevelPathSafety.h"
#include "Domains/Level/World/McpLevelWorldAccess.h"

#include "Editor.h"
#include "EditorLevelUtils.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsActionsPaths.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "GameFramework/Actor.h"
#include "LevelUtils.h"
#include "UObject/Package.h"

namespace McpLevelHandlers {
namespace {
FString LevelPackage(const ULevel* Level) {
  return Level ? Level->GetOutermost()->GetName() : FString();
}
} // namespace

bool HandleMoveActorsToLevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                                   TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const auto Refuse = [&](const FString& Message, const TSharedPtr<FJsonObject>& Detail, const TCHAR* Code) {
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false, Message + TEXT(" Nothing was moved."), Detail, Code);
    return true;
  };
  UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  if (!World) {
    return Refuse(TEXT("No level is open in the editor."), nullptr, TEXT("NO_WORLD"));
  }
  const FString Given = GetJsonStringField(Payload, TEXT("levelPath")).TrimStartAndEnd();
  const FString OpenLevel = LevelPackage(World->PersistentLevel);
  const FString Wanted = NormalizeLevelPackagePath(Given);
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  ULevel* Destination = nullptr;
  if (Wanted.Equals(OpenLevel, ESearchCase::IgnoreCase) || Given.Equals(TEXT("persistent"), ESearchCase::IgnoreCase)) {
    Destination = World->PersistentLevel;
  } else {
    const TArray<ULevelStreaming*> Matches = FindStreamingLevels(World, Wanted);
    TArray<FString> Loaded;
    for (const ULevel* Level : GetAllLevelsFromWorld(World)) {
      Loaded.Add(LevelPackage(Level));
    }
    // The loaded levels go with a refusal only: they name what levelPath can be.
    TSharedPtr<FJsonObject> Levels = McpHandlerUtils::CreateResultObject();
    Levels->SetArrayField(TEXT("levels"), McpHandlerUtils::ToJsonStringArray(Loaded));
    if (Matches.Num() != 1) {
      return Refuse(FString::Printf(TEXT("%s is %s of the open level %s (its loaded levels: %s)."), *Wanted,
                                    Matches.Num() > 1 ? TEXT("more than one sublevel") : TEXT("not a sublevel"), *OpenLevel,
                                    *FString::Join(Loaded, TEXT(", "))),
                    Levels, Matches.Num() > 1 ? TEXT("AMBIGUOUS_LEVEL") : TEXT("SUBLEVEL_NOT_FOUND"));
    }
    Destination = Matches[0]->GetLoadedLevel();
    if (!Destination) {
      return Refuse(FString::Printf(TEXT("%s is not loaded; stream it in first (manage_level stream)."), *Wanted), Levels,
                    TEXT("LEVEL_NOT_LOADED"));
    }
  }
  const FString DestinationPath = LevelPackage(Destination);
  // The engine answers a locked level by logging and moving nothing; say so first.
  if (FLevelUtils::IsLevelLocked(Destination)) {
    return Refuse(FString::Printf(TEXT("%s is locked; unlock it in the Levels panel first."), *DestinationPath), nullptr,
                  TEXT("LEVEL_LOCKED"));
  }
  TArray<AActor*> ToMove;
  TArray<FString> AlreadyThere;
  for (const FString& Name : McpHandlerUtils::GetStringArrayField(Payload, TEXT("actorNames"))) {
    AActor* Actor = FindActorByNameInWorldForMcp(World, Name, true);
    if (!Actor) {
      return Refuse(FString::Printf(TEXT("No actor is labelled or named %s."), *Name), nullptr, TEXT("ACTOR_NOT_FOUND"));
    }
    if (FLevelUtils::IsLevelLocked(Actor)) {
      return Refuse(FString::Printf(TEXT("%s is in the locked level %s; unlock it in the Levels panel first."),
                                    *Actor->GetActorLabel(), *LevelPackage(Actor->GetLevel())),
                    nullptr, TEXT("LEVEL_LOCKED"));
    }
    if (Actor->GetLevel() == Destination) {
      AlreadyThere.AddUnique(Actor->GetActorLabel());
    } else {
      ToMove.AddUnique(Actor);
    }
  }
  TArray<AActor*> Moved;
  if (ToMove.Num() > 0) {
    // No reference or renaming dialogs; all or nothing (bMoveAllOrFail).
    UEditorLevelUtils::MoveActorsToLevel(ToMove, Destination, false, false, true, &Moved);
    if (Moved.Num() == 0) {
      return Refuse(FString::Printf(TEXT("The editor moved none of the actors into %s (see the log)."), *DestinationPath),
                    nullptr, TEXT("MOVE_FAILED"));
    }
  }
  TArray<FString> MovedLabels;
  for (const AActor* Actor : Moved) {
    if (Actor) {
      MovedLabels.Add(Actor->GetActorLabel());
    }
  }
  Result->SetStringField(TEXT("levelPath"), DestinationPath);
  Result->SetArrayField(TEXT("moved"), McpHandlerUtils::ToJsonStringArray(MovedLabels));
  if (AlreadyThere.Num() > 0) {
    Result->SetArrayField(TEXT("alreadyThere"), McpHandlerUtils::ToJsonStringArray(AlreadyThere));
  }
  const FString There = AlreadyThere.Num() > 0 ? FString::Printf(TEXT("; %d already were there"), AlreadyThere.Num()) : FString();
  Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
      MovedLabels.Num() > 0
          ? FString::Printf(TEXT("Moved %d actor(s) into %s%s. Save both levels to keep the move."), MovedLabels.Num(), *DestinationPath, *There)
          : FString::Printf(TEXT("Nothing moved: the %d actor(s) already are in %s."), AlreadyThere.Num(), *DestinationPath),
      Result);
  return true;
}
} // namespace McpLevelHandlers
