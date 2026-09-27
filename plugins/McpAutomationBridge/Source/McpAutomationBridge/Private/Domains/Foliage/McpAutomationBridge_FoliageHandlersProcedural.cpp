#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Foliage/McpAutomationBridge_FoliageHandlersPrivate.h"

bool UMcpAutomationBridgeSubsystem::HandleCreateProceduralFoliage(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  FString Name;
  if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
    Name = FString::Printf(TEXT("ProceduralFoliage_%lld"), FDateTime::UtcNow().GetTicks());
  }

  const TSharedPtr<FJsonObject> *BoundsObj = nullptr;
  const TSharedPtr<FJsonObject> Bounds =
      Payload->TryGetObjectField(TEXT("bounds"), BoundsObj) && BoundsObj ? *BoundsObj : nullptr;
  const FVector Location = ExtractVectorField(Bounds, TEXT("location"), FVector::ZeroVector);
  const FVector Size = ExtractVectorField(Bounds, TEXT("size"), FVector(1000.0));

  const TArray<TSharedPtr<FJsonValue>> *FoliageTypesArr = nullptr;
  if (!Payload->TryGetArrayField(TEXT("foliageTypes"), FoliageTypesArr)) {
    Payload->TryGetArrayField(TEXT("types"), FoliageTypesArr);
  }

  int32 Seed = 12345;
  Payload->TryGetNumberField(TEXT("seed"), Seed);

  if (!GEditor) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  FString PackagePath = TEXT("/Game/ProceduralFoliage");
  FString AssetName = Name + TEXT("_Spawner");
  FString FullPackagePath =
      FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);

  UPackage *Package = CreatePackage(*FullPackagePath);
  UProceduralFoliageSpawner *Spawner = NewObject<UProceduralFoliageSpawner>(
      Package, FName(*AssetName), RF_Public | RF_Standalone);
  if (!Spawner) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create spawner asset"),
                        TEXT("CREATION_FAILED"));
    return true;
  }

  double TileSize = 1000.0;
  Payload->TryGetNumberField(TEXT("tileSize"), TileSize);
  Spawner->TileSize = static_cast<float>(FMath::Max(1.0, TileSize));
  Spawner->NumUniqueTiles = 10;
  Spawner->RandomSeed = Seed;

  int32 TypeIndex = 0;
  if (FoliageTypesArr) {
    for (const TSharedPtr<FJsonValue> &Val : *FoliageTypesArr) {
      const TSharedPtr<FJsonObject> *TypeObj = nullptr;
      if (Val->TryGetObject(TypeObj) && TypeObj) {
        FString MeshPath;
        (*TypeObj)->TryGetStringField(TEXT("meshPath"), MeshPath);
        double Density = 10.0;
        (*TypeObj)->TryGetNumberField(TEXT("density"), Density);

        if (!MeshPath.IsEmpty()) {
          UStaticMesh *Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
          if (Mesh) {
            FString FTName =
                FString::Printf(TEXT("%s_FT_%d"), *AssetName, TypeIndex++);
            FString FTPackagePath =
                FString::Printf(TEXT("%s/%s"), *PackagePath, *FTName);
            UPackage *FTPackage = CreatePackage(*FTPackagePath);
            UFoliageType_InstancedStaticMesh *FT =
                NewObject<UFoliageType_InstancedStaticMesh>(
                    FTPackage, FName(*FTName), RF_Public | RF_Standalone);
            FT->SetStaticMesh(Mesh);
            FT->Density = (float)Density;
            FT->ReapplyDensity = true;

            FTPackage->MarkPackageDirty();
            FAssetRegistryModule::AssetCreated(FT);

            FArrayProperty *FoliageTypesProp = FindFProperty<FArrayProperty>(
                Spawner->GetClass(), TEXT("FoliageTypes"));
            if (FoliageTypesProp) {
              FScriptArrayHelper Helper(
                  FoliageTypesProp,
                  FoliageTypesProp->ContainerPtrToValuePtr<void>(Spawner));
              int32 Index = Helper.AddValue();
              void* RawData = Helper.GetRawPtr(Index);
              McpSafeAssetSave(FT);
              UScriptStruct *Struct = FFoliageTypeObject::StaticStruct();

              FObjectProperty *ObjProp = FindFProperty<FObjectProperty>(
                  Struct, TEXT("FoliageTypeObject"));
              if (ObjProp) {
                ObjProp->SetObjectPropertyValue(
                    ObjProp->ContainerPtrToValuePtr<void>(RawData), FT);
              }

              FBoolProperty *BoolProp =
                  FindFProperty<FBoolProperty>(Struct, TEXT("bIsAsset"));
              if (BoolProp) {
                BoolProp->SetPropertyValue(
                    BoolProp->ContainerPtrToValuePtr<void>(RawData), true);
              }
            }
          }
        }
      }
    }
  }

  Package->MarkPackageDirty();
  FAssetRegistryModule::AssetCreated(Spawner);

  AProceduralFoliageVolume *Volume = Cast<AProceduralFoliageVolume>(
      SpawnActorInActiveWorld<AActor>(AProceduralFoliageVolume::StaticClass(),
                                      Location, FRotator::ZeroRotator, Name));
  if (!Volume) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to spawn volume"), TEXT("SPAWN_FAILED"));
    return true;
  }
  McpSafeAssetSave(Spawner);
  Volume->SetActorScale3D(Size / 200.0f);

  bool bResimulated = false;
  if (UProceduralFoliageComponent *ProcComp = Volume->ProceduralComponent) {
    ProcComp->FoliageSpawner = Spawner;
    ProcComp->TileOverlap = 0.0f;
    bResimulated = ProcComp->ResimulateProceduralFoliage(
        [](const TArray<FDesiredFoliageInstance> &) {});
  } else {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Procedural foliage component not available on spawned volume"),
                        TEXT("COMPONENT_NOT_FOUND"));
    return true;
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("volume_actor"), Volume->GetActorLabel());
  Resp->SetStringField(TEXT("spawner_path"), Spawner->GetPathName());
  Resp->SetNumberField(TEXT("foliage_types_count"), TypeIndex);
  Resp->SetBoolField(TEXT("resimulated"), bResimulated);
  Resp->SetBoolField(TEXT("proceduralComponentConfigured"), true);

  McpHandlerUtils::AddVerification(Resp, Volume);
  McpHandlerUtils::AddVerification(Resp, Spawner);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Procedural foliage created"), Resp, FString());
  return true;
}
