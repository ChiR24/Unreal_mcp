#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_FoliageHandlersPrivate.h"

#if WITH_EDITOR
namespace McpFoliageHandlers {

AInstancedFoliageActor* GetOrCreateFoliageActorForWorldSafe(UWorld* World, bool bCreateIfNone)
{
  if (!World) {
    return nullptr;
  }

  if (World->GetWorldPartition()) {
    if (UActorPartitionSubsystem* ActorPartitionSubsystem =
            World->GetSubsystem<UActorPartitionSubsystem>()) {
      if (ActorPartitionSubsystem->IsLevelPartition()) {
        return AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(
            World, bCreateIfNone);
      }
    }
  }

  TActorIterator<AInstancedFoliageActor> It(World);
  if (It) {
    return *It;
  }

  if (!bCreateIfNone) {
    return nullptr;
  }

  FActorSpawnParameters SpawnParams;
  SpawnParams.ObjectFlags |= RF_Transactional;
  SpawnParams.OverrideLevel = World->PersistentLevel;
  return World->SpawnActor<AInstancedFoliageActor>(SpawnParams);
}

}
#endif
