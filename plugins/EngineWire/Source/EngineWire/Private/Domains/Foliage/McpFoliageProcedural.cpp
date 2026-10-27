#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Foliage/McpFoliagePrivate.h"
#include "Domains/Volume/McpVolumeGeometry.h"

// The Resimulate button's own path (FoliageEdit's ProceduralFoliageEditorLibrary), which traces the generated
// instances onto the surfaces in the volume and adds them; its class is not exported, so it is called by name.
// ResimulateProceduralFoliage with a callback that adds nothing, as this used to run, placed nothing at all.
static bool McpResimulateInEditor(AProceduralFoliageVolume *Volume) {
  UClass *Library = FindObject<UClass>(nullptr, TEXT("/Script/FoliageEdit.ProceduralFoliageEditorLibrary"));
  UFunction *Function = Library ? Library->FindFunctionByName(TEXT("ResimulateProceduralFoliageVolumes")) : nullptr;
  FArrayProperty *Volumes = Function ? CastField<FArrayProperty>(Function->FindPropertyByName(TEXT("ProceduralFoliageVolumes"))) : nullptr;
  FObjectPropertyBase *Element = Volumes ? CastField<FObjectPropertyBase>(Volumes->Inner) : nullptr;
  if (!Element) {
    return false;
  }
  void *Params = FMemory::Malloc(Function->ParmsSize, Function->GetMinAlignment());
  Function->InitializeStruct(Params);
  FScriptArrayHelper Helper(Volumes, Volumes->ContainerPtrToValuePtr<void>(Params));
  Element->SetObjectPropertyValue(Helper.GetRawPtr(Helper.AddValue()), Volume);
  Library->GetDefaultObject()->ProcessEvent(Function, Params);
  Function->DestroyStruct(Params);
  FMemory::Free(Params);
  return true;
}

bool UEngineWireSubsystem::HandleCreateProceduralFoliage(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  // volumeName labels the volume (name is the older spelling); it used to be ignored.
  FString Name = McpGetFirstStringField(Payload, {TEXT("volumeName"), TEXT("name")});
  if (Name.IsEmpty()) {
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

  // Every mesh loads before any asset exists: one that did not was skipped, and a spawner with no types placed nothing.
  TArray<TPair<UStaticMesh *, double>> Types;
  for (const TSharedPtr<FJsonValue> &Val : FoliageTypesArr ? *FoliageTypesArr : TArray<TSharedPtr<FJsonValue>>()) {
    const TSharedPtr<FJsonObject> *TypeObj = nullptr;
    FString MeshPath;
    if (Val->TryGetObject(TypeObj) && TypeObj) {
      (*TypeObj)->TryGetStringField(TEXT("meshPath"), MeshPath);
    }
    UStaticMesh *Mesh = MeshPath.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *MeshPath);
    if (!Mesh) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("foliageTypes[%d]: static mesh '%s' does not load. Nothing was created."),
                                          Types.Num(), *MeshPath),
                          TEXT("MESH_NOT_FOUND"));
      return true;
    }
    double Density = 10.0;
    (*TypeObj)->TryGetNumberField(TEXT("density"), Density);
    Types.Emplace(Mesh, Density);
  }
  if (Types.Num() == 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("foliageTypes needs at least one {meshPath, density}: a spawner with no foliage type places nothing."),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // The spawner and its foliage types go in `path`; it was hard-coded to /Game/ProceduralFoliage.
  const FString RequestedPath = GetJsonStringField(Payload, TEXT("path"));
  FString PackagePath = RequestedPath.IsEmpty() ? FString(TEXT("/Game/ProceduralFoliage"))
                                                : SanitizeProjectRelativePath(RequestedPath);
  if (PackagePath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        McpPathRefusalMessage(TEXT("path"), RequestedPath),
                        TEXT("INVALID_PATH"));
    return true;
  }
  PackagePath.RemoveFromEnd(TEXT("/"));
  FString AssetName = Name + TEXT("_Spawner");
  FString FullPackagePath =
      FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);

  FString CreateError, CreateCode;
  UPackage *Package = McpCreatePackageForNewAsset(FullPackagePath, CreateError, CreateCode);
  if (!Package) {
    SendAutomationError(RequestingSocket, RequestId, CreateError, CreateCode);
    return true;
  }
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

  // Each type is a FoliageTypeObject entry of the spawner (set by reflection: the array is protected).
  FArrayProperty *FoliageTypesProp = FindFProperty<FArrayProperty>(Spawner->GetClass(), TEXT("FoliageTypes"));
  UScriptStruct *TypeStruct = FFoliageTypeObject::StaticStruct();
  FObjectProperty *ObjProp = FindFProperty<FObjectProperty>(TypeStruct, TEXT("FoliageTypeObject"));
  FBoolProperty *BoolProp = FindFProperty<FBoolProperty>(TypeStruct, TEXT("bIsAsset"));
  int32 TypeIndex = 0;
  for (const TPair<UStaticMesh *, double> &Type : Types) {
    // The next free <spawner>_FT_<n>: an asset left under that name was rebuilt over before.
    FString FTName;
    FString FTPackagePath;
    do {
      FTName = FString::Printf(TEXT("%s_FT_%d"), *AssetName, TypeIndex++);
      FTPackagePath = FString::Printf(TEXT("%s/%s"), *PackagePath, *FTName);
    } while (McpAssetExists(FTPackagePath));
    UPackage *FTPackage = CreatePackage(*FTPackagePath);
    UFoliageType_InstancedStaticMesh *FT =
        NewObject<UFoliageType_InstancedStaticMesh>(FTPackage, FName(*FTName), RF_Public | RF_Standalone);
    FT->SetStaticMesh(Type.Key);
    FT->Density = static_cast<float>(Type.Value);
    FT->ReapplyDensity = true;
    // The simulation seeds from InitialSeedDensity (its square per 10 m x 10 m), never the painting Density this set alone.
    FT->InitialSeedDensity = static_cast<float>(FMath::Sqrt(FMath::Max(0.0, Type.Value)));
    FTPackage->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(FT);
    McpSafeAssetSave(FT);
    if (FoliageTypesProp) {
      FScriptArrayHelper Helper(FoliageTypesProp, FoliageTypesProp->ContainerPtrToValuePtr<void>(Spawner));
      void *RawData = Helper.GetRawPtr(Helper.AddValue());
      if (ObjProp) {
        ObjProp->SetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(RawData), FT);
      }
      if (BoolProp) {
        BoolProp->SetPropertyValue(BoolProp->ContainerPtrToValuePtr<void>(RawData), true);
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
  // A spawned volume has no brush: scaling it left bounds of zero, an area with no tiles to fill.
  VolumeHelpers::CreateBoxBrushForVolume(Volume, Size * 0.5);

  bool bResimulated = false;
  if (UProceduralFoliageComponent *ProcComp = Volume->ProceduralComponent) {
    ProcComp->FoliageSpawner = Spawner;
    ProcComp->TileOverlap = 0.0f;
    bResimulated = McpResimulateInEditor(Volume) && ProcComp->HasSpawnedAnyInstances();
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
  Resp->SetNumberField(TEXT("foliage_types_count"), Types.Num());
  Resp->SetBoolField(TEXT("resimulated"), bResimulated);
  Resp->SetBoolField(TEXT("proceduralComponentConfigured"), true);

  McpHandlerUtils::AddVerification(Resp, Volume);
  McpHandlerUtils::AddVerification(Resp, Spawner);

  // A simulation that failed placed nothing; it used to be reported as a success.
  SendAutomationResponse(RequestingSocket, RequestId, bResimulated,
                         bResimulated ? FString(TEXT("Procedural foliage created"))
                                      : FString(TEXT("The volume and spawner were created but the simulation placed nothing: the instances "
                                                     "land on landscape, static mesh and BSP surfaces inside the volume, so it needs one "
                                                     "there (check bounds and the foliage density)")),
                         Resp, bResimulated ? FString() : FString(TEXT("NOTHING_SPAWNED")));
  return true;
}
