
#include "McpAutomationBridge_LevelHandlersWorldAccess.h"

#include "Engine/LevelStreaming.h"
#include "Engine/World.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
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
#endif
} // namespace McpLevelHandlers
