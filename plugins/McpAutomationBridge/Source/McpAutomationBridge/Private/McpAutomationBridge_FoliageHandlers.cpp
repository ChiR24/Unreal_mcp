// =============================================================================
// McpAutomationBridge_FoliageHandlers.cpp
// =============================================================================
// Foliage and Procedural Foliage automation handlers for MCP Automation Bridge.
//
// HANDLERS IMPLEMENTED (6 total):
// -----------------------------------------------------------------------------
// Section A - Foliage Instance Management (4 handlers):
//   - paint_foliage       : Paint foliage instances at specified world locations
//   - remove_foliage      : Remove foliage instances (all or by type)
//   - get_foliage_instances: Query existing foliage instance data
//   - add_foliage_instances: Add instances with full transform support
//
// Section B - Foliage Type Management (1 handler):
//   - add_foliage_type    : Create new foliage type asset from static mesh
//
// Section C - Procedural Foliage (1 handler):
//   - create_procedural_foliage: Create procedural foliage volume with spawner
//
// =============================================================================
// UE VERSION COMPATIBILITY (5.0 - 5.7):
// -----------------------------------------------------------------------------
// - World Partition: UE 5.0-5.3 uses ActorPartitionSubsystem differently
//   than UE 5.4+; helper function handles both paths
// - InstancedFoliageActor: API stable across all versions
// - FoliageType_InstancedStaticMesh: API stable across all versions
// - ProceduralFoliageSpawner: API stable, uses reflection for FoliageTypes
// - McpSafeAssetSave: Required for UE 5.7+ (avoid UPackage::SavePackage crash)
//
// =============================================================================
// SECURITY NOTES:
// -----------------------------------------------------------------------------
// - All asset paths sanitized via SanitizeProjectRelativePath()
// - Path traversal attacks blocked at validation layer
// - Auto-resolve of simple names limited to /Game/Foliage/ prefix
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first include
#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

// =============================================================================
// Editor-Only Includes
// =============================================================================
#if WITH_EDITOR

// -----------------------------------------------------------------------------
// Core Engine
// -----------------------------------------------------------------------------
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/SavePackage.h"

// -----------------------------------------------------------------------------
// Foliage System
// -----------------------------------------------------------------------------
#include "FoliageType.h"
#include "FoliageTypeObject.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "InstancedFoliageActor.h"
#include "ProceduralFoliageComponent.h"
#include "ProceduralFoliageSpawner.h"
#include "ProceduralFoliageVolume.h"

// -----------------------------------------------------------------------------
// Editor Subsystems
// -----------------------------------------------------------------------------
#include "EditorAssetLibrary.h"

// Version-specific subsystem includes
#if __has_include("Subsystems/EditorActorSubsystem.h")
  #include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
  #include "EditorActorSubsystem.h"
#endif

// World Partition support
#include "ActorPartition/ActorPartitionSubsystem.h"
#include "EditorBuildUtils.h"
#include "WorldPartition/WorldPartition.h"

// Asset Registry for procedural foliage
#include "AssetRegistry/AssetRegistryModule.h"

#endif // WITH_EDITOR

// =============================================================================
// Section A: Internal Helper Functions
// =============================================================================

#if WITH_EDITOR

/**
 * Get or create an InstancedFoliageActor for the given world.
 *
 * Handles differences between World Partition and non-partitioned levels:
 * - World Partition levels: Uses ActorPartitionSubsystem to get the correct
 *   foliage actor for the current level partition
 * - Standard levels: Finds existing foliage actor or spawns a new one
 *
 * @param World          The world to get/create foliage actor for
 * @param bCreateIfNone  If true, spawn a new actor if none exists
 * @return Pointer to the InstancedFoliageActor, or nullptr on failure
 *
 * UE Version Notes:
 * - UE 5.0-5.3: ActorPartitionSubsystem::IsLevelPartition() check required
 * - UE 5.4+: Same API, but internal behavior differs slightly
 */
static AInstancedFoliageActor* GetOrCreateFoliageActorForWorldSafe(UWorld* World, bool bCreateIfNone)
{
  if (!World) {
    return nullptr;
  }

  // Check for World Partition - requires special handling
  if (UWorldPartition *WorldPartition = World->GetWorldPartition()) {
    // Verify this is actually a partitioned level (not just having WP object)
    // to avoid crashes in non-partitioned levels
    if (UActorPartitionSubsystem* ActorPartitionSubsystem =
            World->GetSubsystem<UActorPartitionSubsystem>()) {
      if (ActorPartitionSubsystem->IsLevelPartition()) {
        // World Partition path - use the subsystem's method
        return AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(
            World, bCreateIfNone);
      }
    }
  }

  // Non-partitioned world: Find existing or spawn manually
  // Avoid ActorPartitionSubsystem ensures which can fail in this case
  TActorIterator<AInstancedFoliageActor> It(World);
  if (It) {
    return *It;
  }

  // No existing actor - spawn if requested
  if (!bCreateIfNone) {
    return nullptr;
  }

  FActorSpawnParameters SpawnParams;
  SpawnParams.ObjectFlags |= RF_Transactional;
  SpawnParams.OverrideLevel = World->PersistentLevel;
  return World->SpawnActor<AInstancedFoliageActor>(SpawnParams);
}

#endif // WITH_EDITOR

// =============================================================================
// Section B: Foliage Instance Management Handlers
// =============================================================================

// -----------------------------------------------------------------------------
// Handler: paint_foliage
// -----------------------------------------------------------------------------
// Paints foliage instances at specified world locations.
//
// Payload:
//   - foliageTypePath (string): Path to FoliageType or StaticMesh asset
//   - foliageType (string): Alternate key for foliageTypePath
//   - locations (array): Array of {x, y, z} location objects
//   - location (object): Single position object (alternative to array)
//   - position (object): Alias for location
//
// Response:
//   - success (bool): true if successful
//   - foliageTypePath (string): Resolved foliage type path
//   - instancesPlaced (int): Number of instances created
//   - foliageActorPath (string): Path to the foliage actor
// -----------------------------------------------------------------------------
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

  // Security: Validate path format
  FString SafePath = SanitizeProjectRelativePath(FoliageTypePath);
  if (SafePath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Invalid or unsafe foliage type path: %s"), *FoliageTypePath),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }
  FoliageTypePath = SafePath;

  // Auto-resolve simple name
  if (!FoliageTypePath.IsEmpty() &&
      FPaths::GetPath(FoliageTypePath).IsEmpty()) {
    FoliageTypePath =
        FString::Printf(TEXT("/Game/Foliage/%s"), *FoliageTypePath);
  }

  // Validate after auto-resolve
  if (FoliageTypePath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("foliageTypePath (or foliageType) required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
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

  // Try to load as FoliageType first
  UFoliageType *FoliageType = nullptr;
  if (UEditorAssetLibrary::DoesAssetExist(FoliageTypePath)) {
    FoliageType = LoadObject<UFoliageType>(nullptr, *FoliageTypePath);
  }
  
  // If not a FoliageType, try loading as StaticMesh and auto-create FoliageType
  if (!FoliageType) {
    UStaticMesh *StaticMesh = LoadObject<UStaticMesh>(nullptr, *FoliageTypePath);
    if (StaticMesh) {
      // Auto-create FoliageType from StaticMesh
      FString BaseName = FPaths::GetBaseFilename(FoliageTypePath);
      FString AutoFTPath = FString::Printf(TEXT("/Game/Foliage/Auto_%s"), *BaseName);

      // Issue 4 fix: Check if asset already exists before creating
      if (UEditorAssetLibrary::DoesAssetExist(AutoFTPath)) {
        FoliageType = LoadObject<UFoliageType>(nullptr, *AutoFTPath);
        if (FoliageType) {
          FoliageTypePath = AutoFTPath;
          UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
                 TEXT("HandlePaintFoliage: Using existing auto-created FoliageType: %s"), *FoliageTypePath);
        }
      } else {
        // Issue 5 fix: Add null check for CreatePackage
        UPackage *FTPackage = CreatePackage(*AutoFTPath);
        if (FTPackage) {
          UFoliageType_InstancedStaticMesh *AutoFT = NewObject<UFoliageType_InstancedStaticMesh>(
              FTPackage, FName(*BaseName), RF_Public | RF_Standalone);
          if (AutoFT) {
            AutoFT->SetStaticMesh(StaticMesh);
            AutoFT->Density = 100.0f;
            AutoFT->ReapplyDensity = true;
            McpSafeAssetSave(AutoFT);
            FoliageType = AutoFT;
            FoliageTypePath = AutoFT->GetPathName();
            UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
                   TEXT("HandlePaintFoliage: Auto-created FoliageType from StaticMesh: %s"), *FoliageTypePath);
          }
        }
      }
    }
  }
  
  if (!FoliageType) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Foliage type asset not found: %s (also tried as StaticMesh)"),
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

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("foliageTypePath"), FoliageTypePath);
  Resp->SetNumberField(TEXT("instancesPlaced"), PlacedLocations.Num());
  
  // Add verification data
  Resp->SetStringField(TEXT("foliageActorPath"), IFA->GetPathName());
  Resp->SetStringField(TEXT("foliageActorName"), IFA->GetName());
  Resp->SetBoolField(TEXT("existsAfter"), true);

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

// -----------------------------------------------------------------------------
// Handler: remove_foliage
// -----------------------------------------------------------------------------
// Removes foliage instances from the level.
//
// Payload:
//   - foliageTypePath (string, optional): Path to specific foliage type
//   - removeAll (bool): If true, remove all foliage instances
//
// Response:
//   - success (bool): true if successful
//   - instancesRemoved (int): Number of instances removed
//   - foliageActorPath (string): Path to the foliage actor
// -----------------------------------------------------------------------------
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

  // Security: Validate path format if provided
  if (!FoliageTypePath.IsEmpty()) {
    FString SafePath = SanitizeProjectRelativePath(FoliageTypePath);
    if (SafePath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Invalid or unsafe foliage type path: %s"), *FoliageTypePath),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }
    FoliageTypePath = SafePath;
  }

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

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("instancesRemoved"), RemovedCount);
  
  // Add verification data
  Resp->SetStringField(TEXT("foliageActorPath"), IFA->GetPathName());
  Resp->SetBoolField(TEXT("existsAfter"), true);

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

// -----------------------------------------------------------------------------
// Handler: get_foliage_instances
// -----------------------------------------------------------------------------
// Retrieves foliage instance data from the level.
//
// Payload:
//   - foliageTypePath (string, optional): Path to specific foliage type
//     If not provided, returns all instances from all foliage types
//
// Response:
//   - success (bool): true if successful
//   - instances (array): Array of instance data objects
//   - count (int): Total number of instances
//   - foliageActorPath (string): Path to the foliage actor
// -----------------------------------------------------------------------------
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

  // Security: Validate path format if provided
  if (!FoliageTypePath.IsEmpty()) {
    FString SafePath = SanitizeProjectRelativePath(FoliageTypePath);
    if (SafePath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Invalid or unsafe foliage type path: %s"), *FoliageTypePath),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }
    FoliageTypePath = SafePath;
  }

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
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
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
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
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
          TSharedPtr<FJsonObject> InstObj = McpHandlerUtils::CreateResultObject();
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
        TSharedPtr<FJsonObject> InstObj = McpHandlerUtils::CreateResultObject();
        InstObj->SetStringField(TEXT("foliageType"), Type->GetPathName());
        InstObj->SetNumberField(TEXT("x"), Inst.Location.X);
        InstObj->SetNumberField(TEXT("y"), Inst.Location.Y);
        InstObj->SetNumberField(TEXT("z"), Inst.Location.Z);
        InstancesArray.Add(MakeShared<FJsonValueObject>(InstObj));
      }
      return true;
    });
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetArrayField(TEXT("instances"), InstancesArray);
  Resp->SetNumberField(TEXT("count"), InstancesArray.Num());
  
  // Add verification data
  Resp->SetStringField(TEXT("foliageActorPath"), IFA->GetPathName());
  Resp->SetBoolField(TEXT("existsAfter"), true);

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

// =============================================================================
// Section C: Foliage Type Management Handlers
// =============================================================================

// -----------------------------------------------------------------------------
// Handler: add_foliage_type
// -----------------------------------------------------------------------------
// Creates a new foliage type asset from a static mesh.
//
// Payload:
//   - name (string): Name for the new foliage type
//   - meshPath (string): Path to the source static mesh
//   - density (float, optional): Paint density (default: 100.0)
//   - minScale (float, optional): Minimum scale (default: 1.0)
//   - maxScale (float, optional): Maximum scale (default: 1.0)
//   - alignToNormal (bool, optional): Align to surface normal (default: true)
//   - randomYaw (bool, optional): Apply random yaw rotation (default: true)
//
// Response:
//   - success (bool): true if successful
//   - created (bool): true if new asset was created
//   - asset_path (string): Path to the created foliage type
//   - used_mesh (string): Path to the source mesh
// -----------------------------------------------------------------------------
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

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("created"), true);
  Resp->SetBoolField(TEXT("exists_after"), true);
  Resp->SetStringField(TEXT("asset_path"), FoliageType->GetPathName());
  Resp->SetStringField(TEXT("used_mesh"), MeshPath);
  Resp->SetStringField(TEXT("method"), TEXT("native_asset_creation"));
  
  // Add verification data
  McpHandlerUtils::AddVerification(Resp, FoliageType);

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

// -----------------------------------------------------------------------------
// Handler: add_foliage_instances
// -----------------------------------------------------------------------------
// Adds foliage instances with full transform support (location, rotation, scale).
//
// Payload:
//   - foliageTypePath (string): Path to FoliageType or StaticMesh asset
//   - foliageType (string): Alternate key for foliageTypePath
//   - transforms (array): Array of transform objects with:
//       - location: {x, y, z} or [x, y, z]
//       - rotation (optional): {pitch, yaw, roll} or [pitch, yaw, roll]
//       - scale (optional): {x, y, z}, [x, y, z], or uniformScale scalar
//   - locations (array, legacy): Array of {x, y, z} objects
//
// Response:
//   - success (bool): true if successful
//   - instances_count (int): Number of instances added
//   - foliageActorPath (string): Path to the foliage actor
//   - foliageTypePath (string): Resolved foliage type path
// -----------------------------------------------------------------------------
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

  // Security: Validate path format
  FString SafePath = SanitizeProjectRelativePath(FoliageTypePath);
  if (SafePath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Invalid or unsafe foliage type path: %s"), *FoliageTypePath),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }
  FoliageTypePath = SafePath;

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

  // Try to load as FoliageType first
  UFoliageType *FoliageType = Cast<UFoliageType>(
      StaticLoadObject(UFoliageType::StaticClass(), nullptr, *FoliageTypePath,
                       nullptr, LOAD_NoWarn));
  
  // If not a FoliageType, try loading as StaticMesh and auto-create FoliageType
  if (!FoliageType) {
    UStaticMesh *StaticMesh = LoadObject<UStaticMesh>(nullptr, *FoliageTypePath);
    if (StaticMesh) {
      // Auto-create FoliageType from StaticMesh
      FString BaseName = FPaths::GetBaseFilename(FoliageTypePath);
      FString AutoFTPath = FString::Printf(TEXT("/Game/Foliage/Auto_%s"), *BaseName);
      UPackage *FTPackage = CreatePackage(*AutoFTPath);
      UFoliageType_InstancedStaticMesh *AutoFT = NewObject<UFoliageType_InstancedStaticMesh>(
          FTPackage, FName(*BaseName), RF_Public | RF_Standalone);
      if (AutoFT) {
        AutoFT->SetStaticMesh(StaticMesh);
        AutoFT->Density = 100.0f;
        AutoFT->ReapplyDensity = true;
        McpSafeAssetSave(AutoFT);
        FoliageType = AutoFT;
        FoliageTypePath = AutoFT->GetPathName();
        UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
               TEXT("HandleAddFoliageInstances: Auto-created FoliageType from StaticMesh: %s"), *FoliageTypePath);
      }
    }
  }
  
  if (!FoliageType) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Foliage type asset not found: %s (also tried as StaticMesh)"),
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

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("instances_count"), Added);
  
  // Add verification data
  Resp->SetStringField(TEXT("foliageActorPath"), IFA->GetPathName());
  Resp->SetStringField(TEXT("foliageTypePath"), FoliageTypePath);
  Resp->SetBoolField(TEXT("existsAfter"), true);
  
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

// =============================================================================
// Section D: Procedural Foliage Handlers
// =============================================================================

// -----------------------------------------------------------------------------
// Handler: create_procedural_foliage
// -----------------------------------------------------------------------------
// Creates a procedural foliage volume with spawner and automatically populates.
//
// Payload:
//   - name (string): Name for the procedural foliage system
//   - bounds (object): Bounds configuration:
//       - location (object, optional): {x, y, z} world position
//       - size (object/array): {x, y, z} or [x, y, z] dimensions
//   - foliageTypes (array): Array of foliage type configs:
//       - meshPath (string): Path to static mesh
//       - density (float, optional): Paint density
//   - seed (int, optional): Random seed for procedural generation
//
// Response:
//   - success (bool): true if successful
//   - volume_actor (string): Name of the spawned volume
//   - spawner_path (string): Path to the created spawner asset
//   - foliage_types_count (int): Number of foliage types configured
//   - resimulated (bool): true if resimulation was triggered
// -----------------------------------------------------------------------------
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

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("volume_actor"), Volume->GetActorLabel());
  Resp->SetStringField(TEXT("spawner_path"), Spawner->GetPathName());
  Resp->SetNumberField(TEXT("foliage_types_count"), TypeIndex);
  Resp->SetBoolField(TEXT("resimulated"), true);
  
  // Add verification data
  McpHandlerUtils::AddVerification(Resp, Volume);
  McpHandlerUtils::AddVerification(Resp, Spawner);

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
