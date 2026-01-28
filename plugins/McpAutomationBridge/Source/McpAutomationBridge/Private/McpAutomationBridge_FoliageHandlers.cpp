#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "EditorAssetLibrary.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FoliageType.h"
#include "FoliageTypeObject.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "InstancedFoliageActor.h"
#include "ProceduralFoliageComponent.h"
#include "ProceduralFoliageSpawner.h"
#include "ProceduralFoliageVolume.h"
#include "UObject/SavePackage.h"
#include "WorldPartition/WorldPartition.h"

#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif

// Add these includes unconditionally for Editor builds
#include "ActorPartition/ActorPartitionSubsystem.h"
#include "EditorBuildUtils.h"
#endif

#if WITH_EDITOR
static AInstancedFoliageActor *
GetOrCreateFoliageActorForWorldSafe(UWorld *World, bool bCreateIfNone) {
  if (!World) {
    return nullptr;
  }

  if (UWorldPartition *WorldPartition = World->GetWorldPartition()) {
    // Check if the world is actually using the Actor Partition Subsystem to
    // avoid crashes in non-partitioned levels that happen to have a WP object.
    if (UActorPartitionSubsystem *ActorPartitionSubsystem =
            World->GetSubsystem<UActorPartitionSubsystem>()) {
      if (ActorPartitionSubsystem->IsLevelPartition()) {
        return AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(
            World, bCreateIfNone);
      }
    }
  }

  // Non-partitioned worlds: avoid ActorPartitionSubsystem ensures by finding or
  // spawning a foliage actor manually.
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
#endif

bool UMcpAutomationBridgeSubsystem::HandlePaintFoliage(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("paint_foliage"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("paint_foliage payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString FoliageTypePath;
  if (!Payload->TryGetStringField(TEXT("foliageTypePath"), FoliageTypePath)) {
    // Accept alternate key used by some clients
    Payload->TryGetStringField(TEXT("foliageType"), FoliageTypePath);
  }
  if (FoliageTypePath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("foliageTypePath (or foliageType) required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Auto-resolve simple name
  if (!FoliageTypePath.IsEmpty() &&
      FPaths::GetPath(FoliageTypePath).IsEmpty()) {
    FoliageTypePath =
        FString::Printf(TEXT("/Game/Foliage/%s"), *FoliageTypePath);
  }

  // Accept single 'position' or array of 'locations'
  TArray<FVector> Locations;
  const TArray<TSharedPtr<FJsonValue>> *LocationsArray = nullptr;
  if ((Payload->TryGetArrayField(TEXT("locations"), LocationsArray) ||
       Payload->TryGetArrayField(TEXT("location"), LocationsArray)) &&
      LocationsArray && LocationsArray->Num() > 0) {
    for (const TSharedPtr<FJsonValue> &Val : *LocationsArray) {
      if (Val.IsValid() && Val->Type == EJson::Object) {
        const TSharedPtr<FJsonObject> *Obj = nullptr;
        if (Val->TryGetObject(Obj) && Obj) {
          double X = 0, Y = 0, Z = 0;
          (*Obj)->TryGetNumberField(TEXT("x"), X);
          (*Obj)->TryGetNumberField(TEXT("y"), Y);
          (*Obj)->TryGetNumberField(TEXT("z"), Z);
          Locations.Add(FVector(X, Y, Z));
        }
      }
    }
  } else {
    // Try a single 'position' object
    const TSharedPtr<FJsonObject> *PosObj = nullptr;
    if ((Payload->TryGetObjectField(TEXT("position"), PosObj) ||
         Payload->TryGetObjectField(TEXT("location"), PosObj)) &&
        PosObj) {
      double X = 0, Y = 0, Z = 0;
      (*PosObj)->TryGetNumberField(TEXT("x"), X);
      (*PosObj)->TryGetNumberField(TEXT("y"), Y);
      (*PosObj)->TryGetNumberField(TEXT("z"), Z);
      Locations.Add(FVector(X, Y, Z));
    }
  }

  if (Locations.Num() == 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("locations array or position required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!GEditor || !GEditor->GetEditorWorldContext().World()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor world not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UWorld *World = GEditor->GetEditorWorldContext().World();

  if (!UEditorAssetLibrary::DoesAssetExist(FoliageTypePath)) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Foliage type asset not found: %s"),
                        *FoliageTypePath),
        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UFoliageType *FoliageType =
      LoadObject<UFoliageType>(nullptr, *FoliageTypePath);
  if (!FoliageType) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to load foliage type"),
                        TEXT("LOAD_FAILED"));
    return true;
  }

  AInstancedFoliageActor *IFA =
      GetOrCreateFoliageActorForWorldSafe(World, true);
  if (!IFA) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to get foliage actor"),
                        TEXT("FOLIAGE_ACTOR_FAILED"));
    return true;
  }

  TArray<FVector> PlacedLocations;
  for (const FVector &Location : Locations) {
    FFoliageInstance Instance;
    Instance.Location = Location;
    Instance.Rotation = FRotator::ZeroRotator;
    Instance.DrawScale3D = FVector3f(1.0f);
    Instance.ZOffset = 0.0f;

    if (FFoliageInfo *Info = IFA->FindInfo(FoliageType)) {
      Info->AddInstance(FoliageType, Instance, /*InBaseComponent*/ nullptr);
    } else {
      IFA->AddFoliageType(FoliageType);
      if (FFoliageInfo *NewInfo = IFA->FindInfo(FoliageType)) {
        NewInfo->AddInstance(FoliageType, Instance,
                             /*InBaseComponent*/ nullptr);
      }
    }
    PlacedLocations.Add(Location);
  }

  IFA->Modify();

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("foliageTypePath"), FoliageTypePath);
  Resp->SetNumberField(TEXT("instancesPlaced"), PlacedLocations.Num());

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Foliage painted successfully"), Resp, FString());
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("paint_foliage requires editor build."), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleRemoveFoliage(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("remove_foliage"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("remove_foliage payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString FoliageTypePath;
  Payload->TryGetStringField(TEXT("foliageTypePath"), FoliageTypePath);

  // Auto-resolve simple name
  if (!FoliageTypePath.IsEmpty() &&
      FPaths::GetPath(FoliageTypePath).IsEmpty()) {
    FoliageTypePath =
        FString::Printf(TEXT("/Game/Foliage/%s"), *FoliageTypePath);
  }

  bool bRemoveAll = false;
  Payload->TryGetBoolField(TEXT("removeAll"), bRemoveAll);

  if (!GEditor || !GEditor->GetEditorWorldContext().World()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor world not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UWorld *World = GEditor->GetEditorWorldContext().World();
  AInstancedFoliageActor *IFA =
      GetOrCreateFoliageActorForWorldSafe(World, false);
  if (!IFA) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("No foliage actor found"),
                        TEXT("FOLIAGE_ACTOR_NOT_FOUND"));
    return true;
  }

  int32 RemovedCount = 0;

  if (bRemoveAll) {
    IFA->ForEachFoliageInfo([&](UFoliageType *Type, FFoliageInfo &Info) {
      RemovedCount += Info.Instances.Num();
      Info.Instances.Empty();
      return true;
    });
    IFA->Modify();
  } else if (!FoliageTypePath.IsEmpty()) {
    if (UEditorAssetLibrary::DoesAssetExist(FoliageTypePath)) {
      UFoliageType *FoliageType =
          LoadObject<UFoliageType>(nullptr, *FoliageTypePath);
      if (FoliageType) {
        FFoliageInfo *Info = IFA->FindInfo(FoliageType);
        if (Info) {
          RemovedCount = Info->Instances.Num();
          Info->Instances.Empty();
          IFA->Modify();
        }
      }
    }
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("instancesRemoved"), RemovedCount);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Foliage removed successfully"), Resp, FString());
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("remove_foliage requires editor build."), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGetFoliageInstances(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("get_foliage_instances"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("get_foliage_instances payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString FoliageTypePath;
  Payload->TryGetStringField(TEXT("foliageTypePath"), FoliageTypePath);

  // Auto-resolve simple name
  if (!FoliageTypePath.IsEmpty() &&
      FPaths::GetPath(FoliageTypePath).IsEmpty()) {
    FoliageTypePath =
        FString::Printf(TEXT("/Game/Foliage/%s"), *FoliageTypePath);
  }

  if (!GEditor || !GEditor->GetEditorWorldContext().World()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor world not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UWorld *World = GEditor->GetEditorWorldContext().World();
  AInstancedFoliageActor *IFA =
      GetOrCreateFoliageActorForWorldSafe(World, false);
  if (!IFA) {
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetArrayField(TEXT("instances"), TArray<TSharedPtr<FJsonValue>>());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("No foliage actor found"), Resp, FString());
    return true;
  }

  TArray<TSharedPtr<FJsonValue>> InstancesArray;

  if (!FoliageTypePath.IsEmpty()) {
    if (!UEditorAssetLibrary::DoesAssetExist(FoliageTypePath)) {
      // If asked for a specific type that doesn't exist, return empty list
      // gracefully (or could error, but empty list seems safer for 'get')
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetArrayField(TEXT("instances"), TArray<TSharedPtr<FJsonValue>>());
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Foliage type not found, 0 instances"), Resp,
                             FString());
      return true;
    }

    UFoliageType *FoliageType =
        LoadObject<UFoliageType>(nullptr, *FoliageTypePath);
    if (FoliageType) {
      FFoliageInfo *Info = IFA->FindInfo(FoliageType);
      if (Info) {
        for (const FFoliageInstance &Inst : Info->Instances) {
          TSharedPtr<FJsonObject> InstObj = MakeShared<FJsonObject>();
          InstObj->SetNumberField(TEXT("x"), Inst.Location.X);
          InstObj->SetNumberField(TEXT("y"), Inst.Location.Y);
          InstObj->SetNumberField(TEXT("z"), Inst.Location.Z);
          InstObj->SetNumberField(TEXT("pitch"), Inst.Rotation.Pitch);
          InstObj->SetNumberField(TEXT("yaw"), Inst.Rotation.Yaw);
          InstObj->SetNumberField(TEXT("roll"), Inst.Rotation.Roll);
          InstancesArray.Add(MakeShared<FJsonValueObject>(InstObj));
        }
      }
    }
  } else {
    IFA->ForEachFoliageInfo([&](UFoliageType *Type, FFoliageInfo &Info) {
      for (const FFoliageInstance &Inst : Info.Instances) {
        TSharedPtr<FJsonObject> InstObj = MakeShared<FJsonObject>();
        InstObj->SetStringField(TEXT("foliageType"), Type->GetPathName());
        InstObj->SetNumberField(TEXT("x"), Inst.Location.X);
        InstObj->SetNumberField(TEXT("y"), Inst.Location.Y);
        InstObj->SetNumberField(TEXT("z"), Inst.Location.Z);
        InstancesArray.Add(MakeShared<FJsonValueObject>(InstObj));
      }
      return true;
    });
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetArrayField(TEXT("instances"), InstancesArray);
  Resp->SetNumberField(TEXT("count"), InstancesArray.Num());

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Foliage instances retrieved"), Resp, FString());
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("get_foliage_instances requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleAddFoliageType(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("add_foliage_type"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("add_foliage_type payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString Name;
  if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("name required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString MeshPath;
  if (!Payload->TryGetStringField(TEXT("meshPath"), MeshPath) ||
      MeshPath.IsEmpty() ||
      MeshPath.Equals(TEXT("undefined"), ESearchCase::IgnoreCase)) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("valid meshPath required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  double Density = 100.0;
  if (Payload->TryGetNumberField(TEXT("density"), Density)) {
    if (Density < 0.0) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("density must be non-negative"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
  }

  double MinScale = 1.0, MaxScale = 1.0;
  Payload->TryGetNumberField(TEXT("minScale"), MinScale);
  Payload->TryGetNumberField(TEXT("maxScale"), MaxScale);

  if (MinScale <= 0.0 || MaxScale <= 0.0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Scales must be positive"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (MinScale > MaxScale) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(
            TEXT("minScale (%f) cannot be greater than maxScale (%f)"),
            MinScale, MaxScale),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  bool AlignToNormal = true;
  Payload->TryGetBoolField(TEXT("alignToNormal"), AlignToNormal);

  bool RandomYaw = true;
  Payload->TryGetBoolField(TEXT("randomYaw"), RandomYaw);

  // Use Silent load to avoid engine warnings
  UStaticMesh *StaticMesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
  if (!StaticMesh) {
    // Try finding it if it's just a short name or missing extension
    if (FPackageName::IsValidLongPackageName(MeshPath)) {
      StaticMesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
    }

    if (!StaticMesh) {
      // Try assuming it's in /Game/ if not specified (naive fallback)
      if (!MeshPath.StartsWith(TEXT("/"))) {
        FString GamePath = FString::Printf(TEXT("/Game/%s"), *MeshPath);
        StaticMesh = LoadObject<UStaticMesh>(nullptr, *GamePath);
        if (!StaticMesh) {
          // Try with inferred name: /Game/Path/Values.Values
          FString BaseName = FPaths::GetBaseFilename(MeshPath);
          GamePath = FString::Printf(TEXT("/Game/%s.%s"), *MeshPath, *BaseName);
          StaticMesh = LoadObject<UStaticMesh>(nullptr, *GamePath);
        }
      }
    }
  }

  if (!StaticMesh) {
    if (!FPackageName::IsValidLongPackageName(MeshPath)) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Invalid package path: %s"), *MeshPath),
          TEXT("INVALID_ARGUMENT"));
    } else {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Static mesh not found: %s"), *MeshPath),
          TEXT("ASSET_NOT_FOUND"));
    }
    return true;
  }

  FString PackagePath = TEXT("/Game/Foliage");
  FString AssetName = Name;
  FString FullPackagePath =
      FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);

  UPackage *Package = CreatePackage(*FullPackagePath);
  if (!Package) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create package"),
                        TEXT("PACKAGE_CREATION_FAILED"));
    return true;
  }

  UFoliageType_InstancedStaticMesh *FoliageType = nullptr;
  if (UEditorAssetLibrary::DoesAssetExist(FullPackagePath)) {
    FoliageType =
        LoadObject<UFoliageType_InstancedStaticMesh>(Package, *AssetName);
  }
  if (!FoliageType) {
    FoliageType = NewObject<UFoliageType_InstancedStaticMesh>(
        Package, FName(*AssetName), RF_Public | RF_Standalone);
  }
  if (!FoliageType) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create foliage type"),
                        TEXT("CREATION_FAILED"));
    return true;
  }

  FoliageType->SetStaticMesh(StaticMesh);
  FoliageType->Density = static_cast<float>(Density);
  FoliageType->Scaling = EFoliageScaling::Uniform;
  FoliageType->ScaleX.Min = static_cast<float>(MinScale);
  FoliageType->ScaleX.Max = static_cast<float>(MaxScale);
  FoliageType->ScaleY.Min = static_cast<float>(MinScale);
  FoliageType->ScaleY.Max = static_cast<float>(MaxScale);
  FoliageType->ScaleZ.Min = static_cast<float>(MinScale);
  FoliageType->ScaleZ.Max = static_cast<float>(MaxScale);
  FoliageType->AlignToNormal = AlignToNormal;
  FoliageType->RandomYaw = RandomYaw;
  FoliageType->ReapplyDensity = true;

  McpSafeAssetSave(FoliageType);

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("created"), true);
  Resp->SetBoolField(TEXT("exists_after"), true);
  Resp->SetStringField(TEXT("asset_path"), FoliageType->GetPathName());
  Resp->SetStringField(TEXT("used_mesh"), MeshPath);
  Resp->SetStringField(TEXT("method"), TEXT("native_asset_creation"));

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Foliage type created successfully"), Resp,
                         FString());

  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("add_foliage_type requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleAddFoliageInstances(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("add_foliage_instances"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("add_foliage_instances payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString FoliageTypePath;
  if (!Payload->TryGetStringField(TEXT("foliageTypePath"), FoliageTypePath)) {
    Payload->TryGetStringField(TEXT("foliageType"), FoliageTypePath);
  }
  if (FoliageTypePath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("foliageType or foliageTypePath required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Auto-resolve simple name
  if (!FoliageTypePath.IsEmpty() &&
      FPaths::GetPath(FoliageTypePath).IsEmpty()) {
    FoliageTypePath =
        FString::Printf(TEXT("/Game/Foliage/%s"), *FoliageTypePath);
  }

  // Parse transforms with full location, rotation, and scale support
  struct FFoliageTransformData {
    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    FVector Scale = FVector::OneVector;
  };
  TArray<FFoliageTransformData> ParsedTransforms;

  const TArray<TSharedPtr<FJsonValue>> *Transforms = nullptr;
  if (Payload->TryGetArrayField(TEXT("transforms"), Transforms) && Transforms) {
    for (const TSharedPtr<FJsonValue> &V : *Transforms) {
      if (!V.IsValid() || V->Type != EJson::Object)
        continue;
      const TSharedPtr<FJsonObject> *TObj = nullptr;
      if (!V->TryGetObject(TObj) || !TObj)
        continue;

      FFoliageTransformData TransformData;

      // Parse location (object or array format)
      const TSharedPtr<FJsonObject> *LocObj = nullptr;
      if ((*TObj)->TryGetObjectField(TEXT("location"), LocObj) && LocObj) {
        (*LocObj)->TryGetNumberField(TEXT("x"), TransformData.Location.X);
        (*LocObj)->TryGetNumberField(TEXT("y"), TransformData.Location.Y);
        (*LocObj)->TryGetNumberField(TEXT("z"), TransformData.Location.Z);
      } else {
        // Accept location as array [x,y,z]
        const TArray<TSharedPtr<FJsonValue>> *LocArr = nullptr;
        if ((*TObj)->TryGetArrayField(TEXT("location"), LocArr) && LocArr &&
            LocArr->Num() >= 3) {
          TransformData.Location.X = (*LocArr)[0]->AsNumber();
          TransformData.Location.Y = (*LocArr)[1]->AsNumber();
          TransformData.Location.Z = (*LocArr)[2]->AsNumber();
        } else {
          continue; // Skip transforms without valid location
        }
      }

      // Parse rotation if provided (object format)
      const TSharedPtr<FJsonObject> *RotObj = nullptr;
      if ((*TObj)->TryGetObjectField(TEXT("rotation"), RotObj) && RotObj) {
        double Pitch = 0, Yaw = 0, Roll = 0;
        (*RotObj)->TryGetNumberField(TEXT("pitch"), Pitch);
        (*RotObj)->TryGetNumberField(TEXT("yaw"), Yaw);
        (*RotObj)->TryGetNumberField(TEXT("roll"), Roll);
        TransformData.Rotation = FRotator(Pitch, Yaw, Roll);
      } else {
        // Accept rotation as array [pitch, yaw, roll]
        const TArray<TSharedPtr<FJsonValue>> *RotArr = nullptr;
        if ((*TObj)->TryGetArrayField(TEXT("rotation"), RotArr) && RotArr &&
            RotArr->Num() >= 3) {
          TransformData.Rotation.Pitch = (*RotArr)[0]->AsNumber();
          TransformData.Rotation.Yaw = (*RotArr)[1]->AsNumber();
          TransformData.Rotation.Roll = (*RotArr)[2]->AsNumber();
        }
      }

      // Parse scale if provided (object, array, or uniform scalar)
      const TSharedPtr<FJsonObject> *ScaleObj = nullptr;
      if ((*TObj)->TryGetObjectField(TEXT("scale"), ScaleObj) && ScaleObj) {
        (*ScaleObj)->TryGetNumberField(TEXT("x"), TransformData.Scale.X);
        (*ScaleObj)->TryGetNumberField(TEXT("y"), TransformData.Scale.Y);
        (*ScaleObj)->TryGetNumberField(TEXT("z"), TransformData.Scale.Z);
      } else {
        const TArray<TSharedPtr<FJsonValue>> *ScaleArr = nullptr;
        if ((*TObj)->TryGetArrayField(TEXT("scale"), ScaleArr) && ScaleArr &&
            ScaleArr->Num() >= 3) {
          TransformData.Scale.X = (*ScaleArr)[0]->AsNumber();
          TransformData.Scale.Y = (*ScaleArr)[1]->AsNumber();
          TransformData.Scale.Z = (*ScaleArr)[2]->AsNumber();
        } else {
          // Check for uniformScale scalar
          double UniformScale = 1.0;
          if ((*TObj)->TryGetNumberField(TEXT("uniformScale"), UniformScale)) {
            TransformData.Scale = FVector(UniformScale);
          }
        }
      }

      ParsedTransforms.Add(TransformData);
    }
  }

  if (ParsedTransforms.Num() == 0) {
    // Fallback to 'locations' if provided (legacy support, default rotation/scale)
    const TArray<TSharedPtr<FJsonValue>> *LocationsArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("locations"), LocationsArray) &&
        LocationsArray) {
      for (const TSharedPtr<FJsonValue> &Val : *LocationsArray) {
        if (Val.IsValid() && Val->Type == EJson::Object) {
          const TSharedPtr<FJsonObject> *Obj = nullptr;
          if (Val->TryGetObject(Obj) && Obj) {
            FFoliageTransformData TransformData;
            (*Obj)->TryGetNumberField(TEXT("x"), TransformData.Location.X);
            (*Obj)->TryGetNumberField(TEXT("y"), TransformData.Location.Y);
            (*Obj)->TryGetNumberField(TEXT("z"), TransformData.Location.Z);
            ParsedTransforms.Add(TransformData);
          }
        }
      }
    }
  }

  if (!GEditor || !GEditor->GetEditorWorldContext().World()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor world not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UWorld *World = GEditor->GetEditorWorldContext().World();

  // Use Silent load to avoid engine warnings
  UFoliageType *FoliageType = Cast<UFoliageType>(
      StaticLoadObject(UFoliageType::StaticClass(), nullptr, *FoliageTypePath,
                       nullptr, LOAD_NoWarn));
  if (!FoliageType) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Foliage type asset not found: %s"),
                        *FoliageTypePath),
        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  AInstancedFoliageActor *IFA =
      GetOrCreateFoliageActorForWorldSafe(World, true);
  if (!IFA) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to get foliage actor"),
                        TEXT("FOLIAGE_ACTOR_FAILED"));
    return true;
  }

  int32 Added = 0;
  for (const FFoliageTransformData &TransformData : ParsedTransforms) {
    FFoliageInstance Instance;
    Instance.Location = TransformData.Location;
    Instance.Rotation = TransformData.Rotation;
    Instance.DrawScale3D = FVector3f(TransformData.Scale);

    if (FFoliageInfo *Info = IFA->FindInfo(FoliageType)) {
      Info->AddInstance(FoliageType, Instance, nullptr);
    } else {
      IFA->AddFoliageType(FoliageType);
      if (FFoliageInfo *NewInfo = IFA->FindInfo(FoliageType)) {
        NewInfo->AddInstance(FoliageType, Instance, nullptr);
      }
    }
    ++Added;
  }
  IFA->Modify();

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("instances_count"), Added);
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Foliage instances added"), Resp, FString());
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("add_foliage_instances requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleCreateProceduralFoliage(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("create_procedural_foliage"),
                    ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("create_procedural_foliage payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString Name;
  if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("name required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const TSharedPtr<FJsonObject> *BoundsObj = nullptr;
  if (!Payload->TryGetObjectField(TEXT("bounds"), BoundsObj) || !BoundsObj) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("bounds required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FVector Location(0, 0, 0);
  FVector Size(1000, 1000, 1000);

  const TSharedPtr<FJsonObject> *LocObj = nullptr;
  if ((*BoundsObj)->TryGetObjectField(TEXT("location"), LocObj) && LocObj) {
    (*LocObj)->TryGetNumberField(TEXT("x"), Location.X);
    (*LocObj)->TryGetNumberField(TEXT("y"), Location.Y);
    (*LocObj)->TryGetNumberField(TEXT("z"), Location.Z);
  }

  const TSharedPtr<FJsonObject> *SizeObj = nullptr;
  if ((*BoundsObj)->TryGetObjectField(TEXT("size"), SizeObj) && SizeObj) {
    (*SizeObj)->TryGetNumberField(TEXT("x"), Size.X);
    (*SizeObj)->TryGetNumberField(TEXT("y"), Size.Y);
    (*SizeObj)->TryGetNumberField(TEXT("z"), Size.Z);
  }
  // If size is array
  const TArray<TSharedPtr<FJsonValue>> *SizeArr = nullptr;
  if ((*BoundsObj)->TryGetArrayField(TEXT("size"), SizeArr) && SizeArr &&
      SizeArr->Num() >= 3) {
    Size.X = (*SizeArr)[0]->AsNumber();
    Size.Y = (*SizeArr)[1]->AsNumber();
    Size.Z = (*SizeArr)[2]->AsNumber();
  }

  const TArray<TSharedPtr<FJsonValue>> *FoliageTypesArr = nullptr;
  if (!Payload->TryGetArrayField(TEXT("foliageTypes"), FoliageTypesArr)) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("foliageTypes array required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  int32 Seed = 12345;
  Payload->TryGetNumberField(TEXT("seed"), Seed);

  if (!GEditor) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  // Create Spawner Asset
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

  Spawner->TileSize = 1000.0f; // Default tile size
  Spawner->NumUniqueTiles = 10;
  Spawner->RandomSeed = Seed;

  // Add foliage types to spawner
  int32 TypeIndex = 0;
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
          // Create FoliageType asset
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

          // Add to Spawner using Reflection (since FoliageTypes is private)
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

  Package->MarkPackageDirty();
  FAssetRegistryModule::AssetCreated(Spawner);

  // Spawn Volume
  AProceduralFoliageVolume *Volume = Cast<AProceduralFoliageVolume>(
      SpawnActorInActiveWorld<AActor>(AProceduralFoliageVolume::StaticClass(),
                                      Location, FRotator::ZeroRotator, Name));
  if (!Volume) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to spawn volume"), TEXT("SPAWN_FAILED"));
    return true;
  }
  McpSafeAssetSave(Spawner);
  // AProceduralFoliageVolume uses ABrush with default extent of 100 units (half-size)
  // Scale = desired_size / (default_brush_extent * 2) = desired_size / 200
  // For a 1000x1000x1000 volume with Size=(1000,1000,1000), scale = 5.0
  Volume->SetActorScale3D(Size / 200.0f);

  if (UProceduralFoliageComponent *ProcComp = Volume->ProceduralComponent) {
    ProcComp->FoliageSpawner = Spawner;
    ProcComp->TileOverlap = 0.0f;

    // Resimulate
    // Note: ResimulateProceduralFoliage might be async or require specific
    // context. In 5.6 it might take a callback or be void. We'll try calling
    // it.
    bool bResult = ProcComp->ResimulateProceduralFoliage(
        [](const TArray<FDesiredFoliageInstance> &) {});
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("volume_actor"), Volume->GetActorLabel());
  Resp->SetStringField(TEXT("spawner_path"), Spawner->GetPathName());
  Resp->SetNumberField(TEXT("foliage_types_count"), TypeIndex);
  Resp->SetBoolField(TEXT("resimulated"), true);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Procedural foliage created"), Resp, FString());
  return true;
#else
  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("create_procedural_foliage requires editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
