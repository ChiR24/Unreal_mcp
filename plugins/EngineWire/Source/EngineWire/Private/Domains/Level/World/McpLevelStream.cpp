#include "Domains/Level/McpLevelActions.h"
#include "Domains/Level/Lifecycle/McpLevelPathSafety.h"

#include "Editor.h"
#include "EditorLevelUtils.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/McpScopedEditorTransaction.h"

namespace McpLevelHandlers {
namespace {
// The streaming levels whose package (PIE prefix removed) is Name or ends in /Name; an exact path wins outright.
// A bare suffix used to match too, so "Sub01" could stream "BigSub01" when that one came first.
TArray<ULevelStreaming*> FindStreamingLevels(UWorld* World, const FString& Name) {
  TArray<ULevelStreaming*> Matches;
  for (ULevelStreaming* Streaming : World->GetStreamingLevels()) {
    const FString Package = Streaming ? UWorld::RemovePIEPrefix(Streaming->GetWorldAssetPackageName()) : FString();
    if (!Package.IsEmpty() && Package.Equals(Name, ESearchCase::IgnoreCase)) {
      return {Streaming};
    }
    if (!Package.IsEmpty() && Package.EndsWith(TEXT("/") + Name, ESearchCase::IgnoreCase)) {
      Matches.Add(Streaming);
    }
  }
  return Matches;
}
} // namespace

bool HandleStreamLevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, bool bForceStreamUnload) {
    // An object path or a .umap name reads as its package, as open_level and add_sublevel take it.
    const FString LevelName = NormalizeLevelPackagePath(
        McpGetFirstStringField(Payload, {TEXT("levelName"), TEXT("levelPath")}).TrimStartAndEnd());
    bool bLoad = !bForceStreamUnload;
    bool bVisible = !bForceStreamUnload;
    if (Payload.IsValid() && !bForceStreamUnload) {
      Payload->TryGetBoolField(TEXT("shouldBeLoaded"), bLoad);
      Payload->TryGetBoolField(TEXT("shouldBeVisible"), bVisible);
    }
    bVisible = bVisible && bLoad; // a level streamed out is not drawn
    if (LevelName.IsEmpty()) {
      Subsystem.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("stream_level requires levelName or levelPath"), nullptr,
          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!GEditor) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor not available"), nullptr,
                             TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    // Streaming state means something only in a game world: the editor world keeps every sublevel loaded and
    // draws it by its editor visibility, so SetShouldBeLoaded and SetShouldBeVisible there changed nothing while
    // the reply said the level was updated. During Play the level streams in the running game (the Load and
    // Unload Stream Level nodes); otherwise it is shown or hidden in the editor, as the Levels panel does.
    UWorld* PlayWorld = GEditor->PlayWorld;
    UWorld* World = PlayWorld ? PlayWorld : GEditor->GetEditorWorldContext().World();
    if (!World) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No world loaded"), nullptr,
                             TEXT("NO_WORLD"));
      return true;
    }
    const TCHAR* WorldLabel = PlayWorld ? TEXT("play") : TEXT("editor");

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("levelName"), LevelName);
    Result->SetStringField(TEXT("world"), WorldLabel);
    const TArray<ULevelStreaming*> Matches = FindStreamingLevels(World, LevelName);
    if (Matches.Num() != 1) {
      TArray<FString> Paths;
      for (const ULevelStreaming* Match : Matches) {
        Paths.Add(UWorld::RemovePIEPrefix(Match->GetWorldAssetPackageName()));
      }
      if (Paths.Num() > 0) {
        Result->SetArrayField(TEXT("matches"), McpHandlerUtils::ToJsonStringArray(Paths));
      }
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
          Paths.Num() > 1
              ? FString::Printf(TEXT("%s names %d streaming levels (%s); pass the full package path"),
                                *LevelName, Paths.Num(), *FString::Join(Paths, TEXT(", ")))
              : FString::Printf(TEXT("Streaming level not found in the %s world: %s (add it with add_sublevel first)"),
                                WorldLabel, *LevelName),
          Result, Paths.Num() > 1 ? TEXT("AMBIGUOUS_LEVEL") : TEXT("STREAMING_LEVEL_NOT_FOUND"));
      return true;
    }
    ULevelStreaming* Target = Matches[0];
    const FString TargetPath = UWorld::RemovePIEPrefix(Target->GetWorldAssetPackageName());
    Result->SetStringField(TEXT("levelPath"), TargetPath);

    TArray<FString> Warnings;
    bool bChanged = false;
    if (PlayWorld) {
      bChanged = Target->IsLevelLoaded() != bLoad || Target->IsLevelVisible() != bVisible;
      Target->SetShouldBeLoaded(bLoad);
      Target->SetShouldBeVisible(bVisible);
      // Settle now, so the reply says where the level ended up and the next call already sees its actors.
      World->FlushLevelStreaming();
      if (!bLoad && Target->ShouldBeLoaded()) {
        Warnings.Add(FString::Printf(TEXT("%s streams with AlwaysLoaded, so it stays loaded in the game; it is hidden instead."), *TargetPath));
      }
    } else {
      ULevel* Level = Target->GetLoadedLevel();
      if (!Level) {
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
            FString::Printf(TEXT("%s is in the open level's streaming list but did not load in the editor (its map file may be missing or broken)"), *TargetPath),
            Result, TEXT("LEVEL_NOT_LOADED"));
        return true;
      }
      // Only a real change opens the transaction: a no-op used to mark the open level unsaved all the same.
      bChanged = Target->IsLevelVisible() != bVisible;
      if (bChanged) {
        FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Set Level Visibility")),
                                                EMcpMutationDurability::EditorStateOnly, {Target});
        UEditorLevelUtils::SetLevelVisibility(Level, bVisible, false);
      }
      if (!bLoad) {
        Warnings.Add(FString::Printf(TEXT("The editor keeps every sublevel loaded, so %s is hidden in the editor instead. It streams out only in a running game: call this during Play."), *TargetPath));
      }
    }

    const bool bLoaded = Target->IsLevelLoaded();
    const bool bShown = Target->IsLevelVisible();
    Result->SetBoolField(TEXT("loaded"), bLoaded);
    Result->SetBoolField(TEXT("visible"), bShown);
    Result->SetStringField(TEXT("streamingState"),
        Target->IsStreamingStatePending() ? TEXT("Pending") : bLoaded ? TEXT("Loaded") : TEXT("Unloaded"));
    if (Warnings.Num() > 0) {
      Result->SetArrayField(TEXT("warnings"), McpHandlerUtils::ToJsonStringArray(Warnings));
    }
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("%s in the %s world: %s, %s%s"), *TargetPath, WorldLabel,
                        bLoaded ? TEXT("loaded") : TEXT("unloaded"), bShown ? TEXT("visible") : TEXT("hidden"),
                        bChanged ? TEXT("") : TEXT(" (already so; nothing changed)")),
        Result);
    return true;
}
} // namespace McpLevelHandlers
