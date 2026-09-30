#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"
#include "Domains/Foliage/McpAutomationBridge_FoliageHandlersPrivate.h"

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

UFoliageType* ResolveFoliageTypeOrMesh(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId,
                                       TSharedPtr<FMcpBridgeWebSocket> Socket, FString& InOutPath)
{
  UFoliageType* Type = McpAssetExists(InOutPath)
                           ? LoadObject<UFoliageType>(nullptr, *InOutPath) : nullptr;
  UStaticMesh* Mesh = Type ? nullptr : LoadObject<UStaticMesh>(nullptr, *InOutPath, nullptr, LOAD_NoWarn);
  if (Mesh) {
    // The asset shares its package's name, so the next call finds and reuses it.
    const FString AutoPath = FString::Printf(TEXT("/Game/Foliage/Auto_%s"), *FPaths::GetBaseFilename(InOutPath));
    Type = McpAssetExists(AutoPath) ? LoadObject<UFoliageType>(nullptr, *AutoPath) : nullptr;
    if (!Type) {
      UFoliageType_InstancedStaticMesh* AutoType = NewObject<UFoliageType_InstancedStaticMesh>(
          CreatePackage(*AutoPath), FName(*FPackageName::GetShortName(AutoPath)), RF_Public | RF_Standalone);
      AutoType->SetStaticMesh(Mesh);
      AutoType->Density = 100.0f;
      AutoType->ReapplyDensity = true;
      McpSafeAssetSave(AutoType);
      Type = AutoType;
    }
  }
  if (!Type) {
    Bridge.SendAutomationError(Socket, RequestId,
        FString::Printf(TEXT("Foliage type asset not found: %s (also tried as StaticMesh)"), *InOutPath),
        TEXT("ASSET_NOT_FOUND"));
    return nullptr;
  }
  InOutPath = Type->GetPathName();
  return Type;
}

AInstancedFoliageActor* RequireFoliageActor(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId,
                                            TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  UWorld* World = McpHandlerUtils::GetEditorWorld();
  if (!World) {
    Bridge.SendAutomationError(Socket, RequestId, TEXT("Editor world not available"), TEXT("EDITOR_NOT_AVAILABLE"));
    return nullptr;
  }
  AInstancedFoliageActor* IFA = GetOrCreateFoliageActorForWorldSafe(World, true);
  if (!IFA) {
    Bridge.SendAutomationError(Socket, RequestId, TEXT("Failed to get foliage actor"), TEXT("FOLIAGE_ACTOR_FAILED"));
  }
  return IFA;
}

}
