#include "Domains/Level/World/McpLevelWorldAccess.h"

#include "Engine/LevelStreaming.h"
#include "Engine/World.h"

namespace McpLevelHandlers {
TArray<ULevel*> GetAllLevelsFromWorld(UWorld* World) {
  TArray<ULevel*> Levels;
  if (!World) {
    return Levels;
  }

  if (World->PersistentLevel) {
    Levels.Add(World->PersistentLevel);
  }

  for (const ULevelStreaming* StreamingLevel : World->GetStreamingLevels()) {
    if (StreamingLevel) {
      ULevel* LoadedLevel = StreamingLevel->GetLoadedLevel();
      if (LoadedLevel) {
        Levels.Add(LoadedLevel);
      }
    }
  }
  return Levels;
}

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
} // namespace McpLevelHandlers
