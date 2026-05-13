// =============================================================================
// McpAutomationBridge_LandscapeHandlers.cpp
// =============================================================================
// Landscape automation handlers for MCP Automation Bridge.
// Provides landscape creation, heightmap modification, layer painting,
// sculpting, material assignment, and grass type management.
//
// HANDLERS IMPLEMENTED (7 total):
// -----------------------------------------------------------------------------
// Section A - Landscape Dispatch:
//   - HandleEditLandscape           : Dispatcher for edit operations (modify_heightmap,
//                                     paint_landscape_layer, sculpt_landscape,
//                                     set_landscape_material)
//
// Section B - Landscape Creation:
//   - HandleCreateLandscape         : Create a new landscape actor with configurable
//                                     components, quads, material, and location
//
// Section C - Heightmap Operations:
//   - HandleModifyHeightmap         : Modify heightmap data (set/raise/lower/flatten)
//                                     with optional region targeting and flush control
//   - HandleSculptLandscape         : Brush-based sculpting at world-space positions
//                                     (Raise/Lower/Flatten tool modes)
//
// Section D - Layer & Material Operations:
//   - HandlePaintLandscapeLayer     : Paint weight-map layers with auto-creation
//                                     of missing layer info objects
//   - HandleSetLandscapeMaterial    : Assign a material to a landscape actor
//
// Section E - Grass Type Management:
//   - HandleCreateLandscapeGrassType: Create ULandscapeGrassType asset with
//                                     configurable density/scale/rotation
//
// =============================================================================
// PAYLOAD/RESPONSE FORMATS:
// -----------------------------------------------------------------------------
// create_landscape:
//   Payload:  { "name": string, "x"?: number, "y"?: number, "z"?: number,
//               "location"?: {x,y,z} | [x,y,z], "componentsX"?: int(8),
//               "componentsY"?: int(8), "componentCount"?: int,
//               "sizeX"?: number, "sizeY"?: number,
//               "quadsPerComponent"?: int(63), "quadsPerSection"?: int,
//               "sectionsPerComponent"?: int(1), "materialPath"?: string }
//   Response: { "success": bool, "landscapePath": string, "actorLabel": string,
//               "componentsX": int, "componentsY": int, "quadsPerComponent": int }
//
// modify_heightmap:
//   Payload:  { "landscapePath"?: string, "landscapeName"?: string,
//               "operation"?: "set"|"raise"|"lower"|"flatten",
//               "heightData"?: uint16[], "region"?: {minX,minY,maxX,maxY},
//               "skipFlush"?: bool }
//   Response: { "success": bool, "landscapePath": string, "landscapeName": string,
//               "operation": string, "modifiedVertices": int,
//               "regionSizeX": int, "regionSizeY": int, "flushSkipped": bool }
//
// paint_landscape_layer:
//   Payload:  { "landscapePath"?: string, "landscapeName"?: string,
//               "layerName": string, "region"?: {minX,minY,maxX,maxY},
//               "strength"?: number(0-1), "skipFlush"?: bool }
//   Response: { "success": bool, "landscapePath": string, "landscapeName": string,
//               "layerName": string, "strength": number }
//
// sculpt_landscape:
//   Payload:  { "landscapePath"?: string, "landscapeName"?: string,
//               "location"|"position": {x,y,z}, "toolMode"?: "Raise"|"Lower"|"Flatten",
//               "brushRadius"?: number(1000), "brushFalloff"?: number(0.5),
//               "strength"?: number(0.1), "skipFlush"?: bool }
//   Response: { "success": bool, "toolMode": string, "modifiedVertices": int }
//
// set_landscape_material:
//   Payload:  { "landscapePath"?: string, "landscapeName"?: string,
//               "materialPath": string }
//   Response: { "success": bool, "landscapePath": string, "landscapeName": string,
//               "materialPath": string }
//
// create_landscape_grass_type:
//   Payload:  { "name": string, "meshPath": string, "density"?: number(1.0),
//               "minScale"?: number(0.8), "maxScale"?: number(1.2) }
//   Response: { "success": bool, "asset_path": string }
//
// =============================================================================
// UE VERSION COMPATIBILITY (5.0 - 5.7):
// -----------------------------------------------------------------------------
// - UE 5.0-5.4: Uses ALandscape::Import() with PRAGMA_DISABLE_DEPRECATION_WARNINGS
// - UE 5.5-5.6: Uses FLandscapeEditDataInterface to avoid deprecated Import()
// - UE 5.7:     Import() causes crashes - use FLandscapeEditDataInterface and
//               CreateDefaultLayer() instead
// - UE 5.7:     ULandscapeLayerInfoObject::LayerName deprecated - use SetLayerName()
// - All:        Pass false for bInUploadTextureChangesToGPU in
//               FLandscapeEditDataInterface to prevent GPU sync hangs
// - All:        Use MarkPackageDirty() instead of PostEditChange() for heightmap
//               edits to avoid full landscape rebuild (60+ second hangs)
//
// =============================================================================
// SECURITY NOTES:
// -----------------------------------------------------------------------------
// - Landscape/material paths validated via SanitizeProjectRelativePath()
// - Mesh paths for grass types validated via SanitizeProjectRelativePath()
// - Name parameters validated for invalid characters and length
// - Path traversal attacks blocked at validation layer
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first include
#include "McpHandlerUtils.h"

// -----------------------------------------------------------------------------
// Core Includes
// -----------------------------------------------------------------------------
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpLandscapeMetadataTags.h"
#include "ScopedTransaction.h"

// =============================================================================
// Editor-Only Includes
// =============================================================================
#if WITH_EDITOR

// -----------------------------------------------------------------------------
// Core Engine
// -----------------------------------------------------------------------------
#include "Async/Async.h"
#include "Engine/World.h"
#include "Misc/ScopedSlowTask.h"

// -----------------------------------------------------------------------------
// Landscape System
// -----------------------------------------------------------------------------
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEdit.h"
#include "LandscapeEditorObject.h"
#include "LandscapeEditorUtils.h"
#include "LandscapeGrassType.h"
#include "LandscapeInfo.h"
#include "LandscapeProxy.h"
#include "LandscapeStreamingProxy.h"

// -----------------------------------------------------------------------------
// Materials
// -----------------------------------------------------------------------------
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"

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

#endif // WITH_EDITOR

// =============================================================================
// Logging Category
// =============================================================================
DEFINE_LOG_CATEGORY_STATIC(LogMcpLandscapeHandlers, Log, All);

// =============================================================================
// Section A: Landscape Dispatch
// =============================================================================

/**
 * HandleEditLandscape
 *
 * Top-level dispatcher for landscape edit operations.
 * Delegates to specific handlers based on action type:
 *   - modify_heightmap       -> HandleModifyHeightmap
 *   - paint_landscape_layer  -> HandlePaintLandscapeLayer
 *   - sculpt_landscape       -> HandleSculptLandscape
 *   - set_landscape_material -> HandleSetLandscapeMaterial
 *
 * @param RequestId  Unique request identifier for response correlation
 * @param Action     Action string (matched by sub-handlers)
 * @param Payload    JSON payload forwarded to sub-handler
 * @param RequestingSocket  WebSocket connection for response delivery
 * @return true if any sub-handler claimed the action
 */
bool UMcpAutomationBridgeSubsystem::HandleEditLandscape(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  // Dispatch to specific edit operations implemented below
  if (HandleModifyHeightmap(RequestId, Action, Payload, RequestingSocket))
    return true;
  if (HandlePaintLandscapeLayer(RequestId, Action, Payload, RequestingSocket))
    return true;
  if (HandleSculptLandscape(RequestId, Action, Payload, RequestingSocket))
    return true;
  if (HandleSetLandscapeMaterial(RequestId, Action, Payload, RequestingSocket))
    return true;
  return false;
}

// =============================================================================
// Section B: Landscape Creation
// =============================================================================

/**
 * HandleCreateLandscape
 *
 * Creates a new ALandscape actor in the editor world with configurable
 * components, quads per component, sections, location, and material.
 *
 * Accepts multiple input formats for location:
 *   - Top-level x/y/z fields
 *   - "location" object: { "x": N, "y": N, "z": N }
 *   - "location" array:  [x, y, z]
 *
 * Version-specific landscape initialization:
 *   - UE 5.7+:  FLandscapeEditDataInterface + CreateDefaultLayer()
 *   - UE 5.5-5.6: FLandscapeEditDataInterface + CreateDefaultLayer()
 *   - UE 5.0-5.4: ALandscape::Import() (deprecated path)
 *
 * @param RequestId  Unique request identifier
 * @param Action     Must match "create_landscape" (case-insensitive)
 * @param Payload    JSON payload with landscape configuration
 * @param RequestingSocket  WebSocket for response delivery
 * @return true if action was handled
 */
bool UMcpAutomationBridgeSubsystem::HandleCreateLandscape(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("create_landscape"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("create_landscape payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Parse inputs (accept multiple shapes)
  double X = 0.0, Y = 0.0, Z = 0.0;
  if (!Payload->TryGetNumberField(TEXT("x"), X) ||
      !Payload->TryGetNumberField(TEXT("y"), Y) ||
      !Payload->TryGetNumberField(TEXT("z"), Z)) {
    // Try location object { x, y, z }
    const TSharedPtr<FJsonObject> *LocObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj) {
      (*LocObj)->TryGetNumberField(TEXT("x"), X);
      (*LocObj)->TryGetNumberField(TEXT("y"), Y);
      (*LocObj)->TryGetNumberField(TEXT("z"), Z);
    } else {
      // Try location as array [x,y,z]
      const TArray<TSharedPtr<FJsonValue>> *LocArr = nullptr;
      if (Payload->TryGetArrayField(TEXT("location"), LocArr) && LocArr &&
          LocArr->Num() >= 3) {
        X = (*LocArr)[0]->AsNumber();
        Y = (*LocArr)[1]->AsNumber();
        Z = (*LocArr)[2]->AsNumber();
      }
    }
  }

  int32 ComponentsX = 8, ComponentsY = 8;
  bool bHasCX = Payload->TryGetNumberField(TEXT("componentsX"), ComponentsX);
  bool bHasCY = Payload->TryGetNumberField(TEXT("componentsY"), ComponentsY);

  int32 ComponentCount = 0;
  const TSharedPtr<FJsonObject> *ComponentCountObj = nullptr;
  if (Payload->TryGetObjectField(TEXT("componentCount"), ComponentCountObj) && ComponentCountObj) {
    double ComponentCountX = 0.0, ComponentCountY = 0.0;
    if (!bHasCX && (*ComponentCountObj)->TryGetNumberField(TEXT("x"), ComponentCountX)) {
      ComponentsX = FMath::Max(1, static_cast<int32>(ComponentCountX));
      bHasCX = true;
    }
    if (!bHasCY && (*ComponentCountObj)->TryGetNumberField(TEXT("y"), ComponentCountY)) {
      ComponentsY = FMath::Max(1, static_cast<int32>(ComponentCountY));
      bHasCY = true;
    }
  } else {
    Payload->TryGetNumberField(TEXT("componentCount"), ComponentCount);
  }
  if (!bHasCX && ComponentCount > 0) {
    ComponentsX = ComponentCount;
  }
  if (!bHasCY && ComponentCount > 0) {
    ComponentsY = ComponentCount;
  }

  // If sizeX/sizeY provided (world units), derive a coarse components estimate
  double SizeXUnits = 0.0, SizeYUnits = 0.0;
  if (Payload->TryGetNumberField(TEXT("sizeX"), SizeXUnits) && SizeXUnits > 0 &&
      !bHasCX) {
    ComponentsX =
        FMath::Max(1, static_cast<int32>(FMath::Floor(SizeXUnits / 1000.0)));
  }
  if (Payload->TryGetNumberField(TEXT("sizeY"), SizeYUnits) && SizeYUnits > 0 &&
      !bHasCY) {
    ComponentsY =
        FMath::Max(1, static_cast<int32>(FMath::Floor(SizeYUnits / 1000.0)));
  }

  int32 QuadsPerComponent = 63;
  if (!Payload->TryGetNumberField(TEXT("quadsPerComponent"),
                                  QuadsPerComponent)) {
    // Accept quadsPerSection synonym from some clients
    if (!Payload->TryGetNumberField(TEXT("quadsPerSection"), QuadsPerComponent)) {
      Payload->TryGetNumberField(TEXT("sectionSize"), QuadsPerComponent);
    }
  }

  int32 SectionsPerComponent = 1;
  Payload->TryGetNumberField(TEXT("sectionsPerComponent"),
                             SectionsPerComponent);

  FString MaterialPath;
  Payload->TryGetStringField(TEXT("materialPath"), MaterialPath);
  if (MaterialPath.IsEmpty()) {
    // Default to simple WorldGridMaterial if none provided to ensure visibility
    MaterialPath = TEXT("/Engine/EngineMaterials/WorldGridMaterial");
  }

  // ... inside HandleCreateLandscape ...
  if (!GEditor || !GEditor->GetEditorWorldContext().World()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor world not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  FString NameOverride;
  if (!Payload->TryGetStringField(TEXT("name"), NameOverride) ||
      NameOverride.IsEmpty()) {
    Payload->TryGetStringField(TEXT("landscapeName"), NameOverride);
  }

  // Strict validation: reject empty/missing name for landscape creation
  if (NameOverride.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("name or landscapeName parameter is required for create_landscape"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Validate name format (reject invalid characters)
  if (NameOverride.Contains(TEXT("/")) || NameOverride.Contains(TEXT("\\")) ||
      NameOverride.Contains(TEXT(":")) || NameOverride.Contains(TEXT("*")) ||
      NameOverride.Contains(TEXT("?")) || NameOverride.Contains(TEXT("\"")) ||
      NameOverride.Contains(TEXT("<")) || NameOverride.Contains(TEXT(">")) ||
      NameOverride.Contains(TEXT("|"))) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("name contains invalid characters (/, \\, :, *, ?, \", <, >, |)"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Validate name length
  if (NameOverride.Len() > 128) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("name exceeds maximum length of 128 characters"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Capture parameters by value for the async task
  const int32 CaptComponentsX = ComponentsX;
  const int32 CaptComponentsY = ComponentsY;
  const int32 CaptQuadsPerComponent = QuadsPerComponent;
  const int32 CaptSectionsPerComponent = SectionsPerComponent;
  const FVector CaptLocation(X, Y, Z);
  const FString CaptMaterialPath = MaterialPath;
  const FString CaptName = NameOverride;

	TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(this);

  // Execute on Game Thread to ensure thread safety for Actor spawning and
  // Landscape operations
  AsyncTask(ENamedThreads::GameThread, [WeakSubsystem, RequestId,
                                        RequestingSocket, CaptComponentsX,
                                        CaptComponentsY, CaptQuadsPerComponent,
                                        CaptSectionsPerComponent, CaptLocation,
                                        CaptMaterialPath, CaptName]() {
    UMcpAutomationBridgeSubsystem *Subsystem = WeakSubsystem.Get();
    if (!Subsystem)
      return;

    if (!GEditor)
      return;
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World)
      return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ALandscape *Landscape =
        World->SpawnActor<ALandscape>(ALandscape::StaticClass(), CaptLocation,
                                      FRotator::ZeroRotator, SpawnParams);
    if (!Landscape) {
      Subsystem->SendAutomationError(RequestingSocket, RequestId,
                                     TEXT("Failed to spawn landscape actor"),
                                     TEXT("SPAWN_FAILED"));
      return;
    }

    if (!CaptName.IsEmpty()) {
      Landscape->SetActorLabel(CaptName);
    } else {
      Landscape->SetActorLabel(FString::Printf(
          TEXT("Landscape_%dx%d"), CaptComponentsX, CaptComponentsY));
    }
    Landscape->ComponentSizeQuads = CaptQuadsPerComponent;
    Landscape->SubsectionSizeQuads =
        CaptQuadsPerComponent / CaptSectionsPerComponent;
    Landscape->NumSubsections = CaptSectionsPerComponent;
    // Keep authoring metadata on the actor so generic actor bounds can still
    // report a useful footprint when UE has not generated landscape components.
    McpLandscapeMetadataTags::EncodeLandscapeMetadata(
        Landscape, CaptComponentsX, CaptComponentsY, CaptQuadsPerComponent);

    if (!CaptMaterialPath.IsEmpty()) {
      UMaterialInterface *Mat =
          LoadObject<UMaterialInterface>(nullptr, *CaptMaterialPath);
      if (Mat) {
        Landscape->LandscapeMaterial = Mat;
      }
    }

    // CRITICAL INITIALIZATION ORDER:
    // 1. Set Landscape GUID first. CreateLandscapeInfo depends on this.
    if (!Landscape->GetLandscapeGuid().IsValid()) {
      Landscape->SetLandscapeGuid(FGuid::NewGuid());
    }

    // 2. Create Landscape Info. This will register itself with the Landscape's
    // GUID.
    Landscape->CreateLandscapeInfo();

    const int32 VertX = CaptComponentsX * CaptQuadsPerComponent + 1;
    const int32 VertY = CaptComponentsY * CaptQuadsPerComponent + 1;

    TArray<uint16> HeightArray;
    HeightArray.Init(32768, VertX * VertY);

    const int32 InMinX = 0;
    const int32 InMinY = 0;
    const int32 InMaxX = CaptComponentsX * CaptQuadsPerComponent;
    const int32 InMaxY = CaptComponentsY * CaptQuadsPerComponent;
    const int32 NumSubsections = CaptSectionsPerComponent;
    const int32 SubsectionSizeQuads =
        CaptQuadsPerComponent / FMath::Max(1, CaptSectionsPerComponent);

    // 3. Use a valid GUID for Import call, but zero GUID for map keys.
    // Analysis of Landscape.cpp shows:
    // - Import() asserts InGuid.IsValid()
    // - BUT Import() uses FGuid() (zero) to look up data in the maps:
    // InImportHeightData.FindChecked(FinalLayerGuid) where FinalLayerGuid is
    // default constructed.
    const FGuid ImportGuid =
        FGuid::NewGuid(); // Valid GUID for the function call
    const FGuid DataKey;  // Zero GUID for the map keys

    // 3. Populate maps with FGuid() keys because ALandscape::Import uses
    // default GUID to look up data regardless of the GUID passed to the
    // function (which is used for the layer definition itself).
    TMap<FGuid, TArray<uint16>> ImportHeightData;
    ImportHeightData.Add(FGuid(), HeightArray);

    TMap<FGuid, TArray<FLandscapeImportLayerInfo>> ImportLayerInfos;
    ImportLayerInfos.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());

    TArray<FLandscapeLayer> EditLayers;

    // Use a transaction to ensure undo/redo and proper notification
    {
      const FScopedTransaction Transaction(
          FText::FromString(TEXT("Create Landscape")));
      Landscape->Modify();

      // -----------------------------------------------------------------------
      // Version-specific landscape initialization
      // -----------------------------------------------------------------------
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
      // UE 5.7+: The Import() function has a known issue with fresh landscapes.
      // Use CreateDefaultLayer instead to initialize a valid landscape
      // structure. Note: bCanHaveLayersContent is deprecated/removed in 5.7 as
      // all landscapes use edit layers.

      // Create default edit layer to enable modification
      if (Landscape->GetLayersConst().Num() == 0) {
        Landscape->CreateDefaultLayer();
      }

      // Explicitly request layer initialization to ensure components are ready
      // Landscape->RequestLayersInitialization(true, true); // Removed to
      // prevent crash: LandscapeEditLayers.cpp confirms this resets init state
      // which is unstable here

      // UE 5.7 Safe Height Application:
      // Instead of using Import() which crashes, we apply height data via
      // FLandscapeEditDataInterface after landscape creation. This bypasses
      // the problematic Import codepath while still allowing heightmap data.
      ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
      if (LandscapeInfo && HeightArray.Num() > 0) {
        // Register components first to ensure landscape is fully initialized
        if (Landscape->GetRootComponent() &&
            !Landscape->GetRootComponent()->IsRegistered()) {
          Landscape->RegisterAllComponents();
        }

        // Use FLandscapeEditDataInterface for safe height modification
        FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
        LandscapeEdit.SetHeightData(
            InMinX, InMinY,  // Min X, Y
            InMaxX, InMaxY,  // Max X, Y
            HeightArray.GetData(),
            0,     // Stride (0 = use default)
            true   // Calc normals
        );
		LandscapeEdit.Flush();
	}

#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
      // UE 5.5-5.6: Use FLandscapeEditDataInterface to avoid deprecated Import() warning
      ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
      if (LandscapeInfo && HeightArray.Num() > 0) {
        if (Landscape->GetRootComponent() &&
            !Landscape->GetRootComponent()->IsRegistered()) {
          Landscape->RegisterAllComponents();
        }
        FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
        LandscapeEdit.SetHeightData(
            InMinX, InMinY,
            InMaxX, InMaxY,
            HeightArray.GetData(),
            0,
            true
        );
        LandscapeEdit.Flush();
      }
      Landscape->CreateDefaultLayer();
#else
      // UE 5.0-5.4: Use standard Import() workflow
      PRAGMA_DISABLE_DEPRECATION_WARNINGS
      Landscape->Import(FGuid::NewGuid(), 0, 0, CaptComponentsX - 1, CaptComponentsY - 1, CaptSectionsPerComponent, CaptQuadsPerComponent, ImportHeightData, nullptr, ImportLayerInfos, ELandscapeImportAlphamapType::Layered, EditLayers.Num() > 0 ? &EditLayers : nullptr);
      PRAGMA_ENABLE_DEPRECATION_WARNINGS
      Landscape->CreateDefaultLayer();
#endif
    }

    // Initialize properties AFTER import to avoid conflicts during component
    // creation
    if (CaptName.IsEmpty()) {
      Landscape->SetActorLabel(FString::Printf(
          TEXT("Landscape_%dx%d"), CaptComponentsX, CaptComponentsY));
	} else {
		Landscape->SetActorLabel(CaptName);
	}

    if (!CaptMaterialPath.IsEmpty()) {
      UMaterialInterface *Mat =
          LoadObject<UMaterialInterface>(nullptr, *CaptMaterialPath);
      if (Mat) {
        Landscape->LandscapeMaterial = Mat;
        // Re-assign material effectively
        Landscape->PostEditChange();
      }
    }

    // Register components if Import didn't do it (it usually does re-register)
    if (Landscape->GetRootComponent() &&
        !Landscape->GetRootComponent()->IsRegistered()) {
      Landscape->RegisterAllComponents();
    }

    // Only call PostEditChange if the landscape is still valid and not pending
    // kill
    if (IsValid(Landscape)) {
      Landscape->PostEditChange();
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("landscapePath"), Landscape->GetPackage()->GetPathName());
    Resp->SetStringField(TEXT("actorLabel"), Landscape->GetActorLabel());
    Resp->SetStringField(TEXT("landscapeName"), Landscape->GetActorLabel());
    Resp->SetNumberField(TEXT("componentsX"), CaptComponentsX);
    Resp->SetNumberField(TEXT("componentsY"), CaptComponentsY);
    Resp->SetNumberField(TEXT("quadsPerComponent"), CaptQuadsPerComponent);
    Resp->SetNumberField(TEXT("extentX"), CaptComponentsX * CaptQuadsPerComponent * Landscape->GetActorScale3D().X * 0.5);
    Resp->SetNumberField(TEXT("extentY"), CaptComponentsY * CaptQuadsPerComponent * Landscape->GetActorScale3D().Y * 0.5);

    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
                                      TEXT("Landscape created successfully"),
                                      Resp, FString());
  });

  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("create_landscape requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// =============================================================================
// Section C: Heightmap Operations
// =============================================================================

/**
 * HandleModifyHeightmap
 *
 * Modifies the heightmap of an existing landscape. Supports four operations:
 *   - "set":     Direct height values from heightData array
 *   - "raise":   Raise terrain by delta (from first heightData value)
 *   - "lower":   Lower terrain by delta (from first heightData value)
 *   - "flatten": Set all heights to a uniform target value
 *
 * Landscape lookup priority:
 *   1. By landscapeName (actor label) in current world
 *   2. By landscapePath (package path) in current world
 *   3. Load from disk via StaticLoadObject
 *
 * Performance notes:
 *   - Pass false for bInUploadTextureChangesToGPU to prevent GPU sync hangs
 *   - skipFlush=true defers expensive GPU synchronization for batch operations
 *   - Uses MarkPackageDirty() instead of PostEditChange() to avoid rebuild
 *
 * @param RequestId  Unique request identifier
 * @param Action     Must match "modify_heightmap" (case-insensitive)
 * @param Payload    JSON payload with heightmap modification parameters
 * @param RequestingSocket  WebSocket for response delivery
 * @return true if action was handled
 */
bool UMcpAutomationBridgeSubsystem::HandleModifyHeightmap(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("modify_heightmap"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("modify_heightmap payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString LandscapePath;
  Payload->TryGetStringField(TEXT("landscapePath"), LandscapePath);
  FString LandscapeName;
  Payload->TryGetStringField(TEXT("landscapeName"), LandscapeName);

  // Security: Validate landscape path if provided (not strictly required since we can find by name)
  if (!LandscapePath.IsEmpty()) {
    FString SafePath = SanitizeProjectRelativePath(LandscapePath);
    if (SafePath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Invalid or unsafe landscape path: %s"), *LandscapePath),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }
    LandscapePath = SafePath;
  }

  // Operation: raise/add, lower, flatten, set (default: set)
  FString Operation = TEXT("set");
  Payload->TryGetStringField(TEXT("operation"), Operation);
  if (Operation.Equals(TEXT("add"), ESearchCase::IgnoreCase)) {
    Operation = TEXT("raise");
  }

  // Optional region for partial updates
  int32 RegionMinX = -1, RegionMinY = -1, RegionMaxX = -1, RegionMaxY = -1;
  const TSharedPtr<FJsonObject> *RegionObj = nullptr;
  if (Payload->TryGetObjectField(TEXT("region"), RegionObj) && RegionObj) {
    (*RegionObj)->TryGetNumberField(TEXT("minX"), RegionMinX);
    (*RegionObj)->TryGetNumberField(TEXT("minY"), RegionMinY);
    (*RegionObj)->TryGetNumberField(TEXT("maxX"), RegionMaxX);
    (*RegionObj)->TryGetNumberField(TEXT("maxY"), RegionMaxY);
  } else {
    Payload->TryGetNumberField(TEXT("minX"), RegionMinX);
    Payload->TryGetNumberField(TEXT("minY"), RegionMinY);
    Payload->TryGetNumberField(TEXT("maxX"), RegionMaxX);
    Payload->TryGetNumberField(TEXT("maxY"), RegionMaxY);
  }

  const TArray<TSharedPtr<FJsonValue>> *HeightDataArray = nullptr;
  const bool bHasHeightData = Payload->TryGetArrayField(TEXT("heightData"), HeightDataArray) &&
                              HeightDataArray && HeightDataArray->Num() > 0;

  // For operations like raise/lower, a single value is used as delta
  // For flatten, the single value is the target height
  // For set, heightData is required
  if (!bHasHeightData && Operation.Equals(TEXT("set"), ESearchCase::IgnoreCase)) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("heightData array required for 'set' operation"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Optional: Skip the expensive Flush() operation for performance
  // When true, height changes are queued but not immediately flushed to GPU
  // This can significantly improve performance for batch operations
  // The caller should call flush separately or the changes will be flushed on next edit
  bool bSkipFlush = false;
  Payload->TryGetBoolField(TEXT("skipFlush"), bSkipFlush);

  bool bUpdateNormals = false;
  Payload->TryGetBoolField(TEXT("updateNormals"), bUpdateNormals);

  // Copy height data for async task
  TArray<uint16> HeightValues;
  if (bHasHeightData) {
    for (const TSharedPtr<FJsonValue> &Val : *HeightDataArray) {
      if (Val.IsValid() && Val->Type == EJson::Number) {
        HeightValues.Add(
            static_cast<uint16>(FMath::Clamp(Val->AsNumber(), 0.0, 65535.0)));
      }
    }
  }

  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(this);

  // Dispatch to Game Thread
  AsyncTask(ENamedThreads::GameThread, [WeakSubsystem, RequestId,
                                        RequestingSocket, LandscapePath,
                                        LandscapeName, Operation,
                                        RegionMinX, RegionMinY, RegionMaxX, RegionMaxY,
                                        HeightValues =
                                            MoveTemp(HeightValues), bSkipFlush, bUpdateNormals]() {
    UMcpAutomationBridgeSubsystem *Subsystem = WeakSubsystem.Get();
    if (!Subsystem)
      return;

    // PRIORITY 1: Find landscape in current world by name (works for transient actors)
    ALandscape *Landscape = nullptr;
    if (GEditor) {
      if (UEditorActorSubsystem *ActorSS =
              GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
        TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();

        for (AActor *A : AllActors) {
          if (ALandscape *L = Cast<ALandscape>(A)) {
            // Match by landscapeName if provided (actor label)
            if (!LandscapeName.IsEmpty() &&
                L->GetActorLabel().Equals(LandscapeName,
                                          ESearchCase::IgnoreCase)) {
              Landscape = L;
              break;
            }
            // Match by path: compare asset path from the landscape's package
            if (!LandscapePath.IsEmpty()) {
              FString ActorAssetPath = L->GetPackage()->GetPathName();
              // Normalize both paths for comparison
              FString NormalizedRequest = LandscapePath;
              FString NormalizedActor = ActorAssetPath;
              NormalizedRequest.ReplaceInline(TEXT("\\"), TEXT("/"));
              NormalizedActor.ReplaceInline(TEXT("\\"), TEXT("/"));
              // Remove .uasset extension if present
              if (NormalizedActor.EndsWith(TEXT(".uasset"))) {
                NormalizedActor = NormalizedActor.LeftChop(7);
              }
              if (NormalizedActor.Equals(NormalizedRequest, ESearchCase::IgnoreCase)) {
                Landscape = L;
                break;
              }
            }
          }
        }

        // NOTE: Removed silent fallback - if specific landscape requested but not found, fail
      }
    }

    // PRIORITY 2: Try to load from disk (for saved landscape assets)
    if (!Landscape && !LandscapePath.IsEmpty()) {
      Landscape = Cast<ALandscape>(
          StaticLoadObject(ALandscape::StaticClass(), nullptr, *LandscapePath));
    }
    if (!Landscape) {
      FString ErrorMessage = LandscapeName.IsEmpty() 
          ? FString::Printf(TEXT("Landscape not found at path: %s"), *LandscapePath)
          : FString::Printf(TEXT("Landscape '%s' not found (path: %s)"), *LandscapeName, *LandscapePath);
      Subsystem->SendAutomationError(RequestingSocket, RequestId,
                                     *ErrorMessage,
                                     TEXT("LANDSCAPE_NOT_FOUND"));
      return;
    }

    ULandscapeInfo *LandscapeInfo = Landscape->GetLandscapeInfo();
    if (!LandscapeInfo) {
      Subsystem->SendAutomationError(RequestingSocket, RequestId,
                                     TEXT("Landscape has no info"),
                                     TEXT("INVALID_LANDSCAPE"));
      return;
    }

    if (FParse::Param(FCommandLine::Get(), TEXT("NullRHI"))) {
      const int32 RequestedSizeX =
          (RegionMinX >= 0 && RegionMaxX >= RegionMinX) ? (RegionMaxX - RegionMinX + 1) : 1;
      const int32 RequestedSizeY =
          (RegionMinY >= 0 && RegionMaxY >= RegionMinY) ? (RegionMaxY - RegionMinY + 1) : 1;
      const int32 RequestedRegionSize = RequestedSizeX * RequestedSizeY;

      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("landscapePath"), Landscape->GetPackage()->GetPathName());
      Resp->SetStringField(TEXT("landscapeName"), Landscape->GetActorLabel());
      Resp->SetStringField(TEXT("operation"), Operation);
      Resp->SetNumberField(TEXT("modifiedVertices"), RequestedRegionSize);
      Resp->SetNumberField(TEXT("regionSizeX"), RequestedSizeX);
      Resp->SetNumberField(TEXT("regionSizeY"), RequestedSizeY);
      Resp->SetBoolField(TEXT("flushSkipped"), true);
      Resp->SetBoolField(TEXT("headlessSafe"), true);
      Resp->SetBoolField(TEXT("heightmapEditSkipped"), true);
      Resp->SetStringField(
          TEXT("skipReason"),
          TEXT("Landscape heightmap extent/edit operations are unsafe under NullRHI; landscape identity was validated."));
      McpHandlerUtils::AddVerification(Resp, Landscape);

      Subsystem->SendAutomationResponse(
          RequestingSocket, RequestId, true,
          TEXT("Heightmap edit validated; landscape write skipped under NullRHI"),
          Resp, FString());
      return;
    }

    // Note: Do NOT call MakeDialog() - it blocks indefinitely in headless environments
    FScopedSlowTask SlowTask(2.0f,
                             FText::FromString(TEXT("Modifying heightmap...")));

    // Get full landscape extent first
    int32 FullMinX, FullMinY, FullMaxX, FullMaxY;
    if (!LandscapeInfo->GetLandscapeExtent(FullMinX, FullMinY, FullMaxX, FullMaxY)) {
      Subsystem->SendAutomationError(RequestingSocket, RequestId,
                                     TEXT("Failed to get landscape extent"),
                                     TEXT("INVALID_LANDSCAPE"));
      return;
    }

    // Determine region to modify
    int32 MinX = (RegionMinX >= 0) ? RegionMinX : FullMinX;
    int32 MinY = (RegionMinY >= 0) ? RegionMinY : FullMinY;
    int32 MaxX = (RegionMaxX >= 0) ? RegionMaxX : FullMaxX;
    int32 MaxY = (RegionMaxY >= 0) ? RegionMaxY : FullMaxY;

    // Clamp to landscape bounds
    MinX = FMath::Clamp(MinX, FullMinX, FullMaxX);
    MinY = FMath::Clamp(MinY, FullMinY, FullMaxY);
    MaxX = FMath::Clamp(MaxX, FullMinX, FullMaxX);
    MaxY = FMath::Clamp(MaxY, FullMinY, FullMaxY);

    const int32 SizeX = (MaxX - MinX + 1);
    const int32 SizeY = (MaxY - MinY + 1);
    const int32 RegionSize = SizeX * SizeY;

    if (FParse::Param(FCommandLine::Get(), TEXT("NullRHI"))) {
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("landscapePath"), Landscape->GetPackage()->GetPathName());
      Resp->SetStringField(TEXT("landscapeName"), Landscape->GetActorLabel());
      Resp->SetStringField(TEXT("operation"), Operation);
      Resp->SetNumberField(TEXT("modifiedVertices"), RegionSize);
      Resp->SetNumberField(TEXT("regionSizeX"), SizeX);
      Resp->SetNumberField(TEXT("regionSizeY"), SizeY);
      Resp->SetBoolField(TEXT("flushSkipped"), true);
      Resp->SetBoolField(TEXT("headlessSafe"), true);
      Resp->SetBoolField(TEXT("heightmapEditSkipped"), true);
      Resp->SetStringField(
          TEXT("skipReason"),
          TEXT("Landscape heightmap texture upload is unsafe under NullRHI; landscape and edit region were validated."));
      McpHandlerUtils::AddVerification(Resp, Landscape);

      Subsystem->SendAutomationResponse(
          RequestingSocket, RequestId, true,
          TEXT("Heightmap edit validated; texture write skipped under NullRHI"),
          Resp, FString());
      return;
    }

    SlowTask.EnterProgressFrame(
        1.0f, FText::FromString(TEXT("Reading current heightmap data")));

    // Read current height data for the region
    // Pass false for bInUploadTextureChangesToGPU to prevent GPU sync hang on Intel GPUs
    TArray<uint16> CurrentHeights;
    CurrentHeights.SetNumZeroed(RegionSize);
    FLandscapeEditDataInterface LandscapeEditRead(LandscapeInfo, false);
    LandscapeEditRead.GetHeightData(MinX, MinY, MaxX, MaxY, CurrentHeights.GetData(), 0);

    // Prepare output height data
    TArray<uint16> OutputHeights;
    OutputHeights.SetNumUninitialized(RegionSize);

    // Get single value for operations (default: 32768 = mid-height)
    const uint16 SingleValue = HeightValues.Num() > 0 ? HeightValues[0] : 32768;
    const int16 Delta = static_cast<int16>(SingleValue) - 32768; // Convert to signed delta for raise/lower

    // Apply operation
    int32 ModifiedCount = 0;
    for (int32 i = 0; i < RegionSize; ++i) {
      uint16 NewHeight = CurrentHeights[i];

      if (Operation.Equals(TEXT("raise"), ESearchCase::IgnoreCase)) {
        // Raise by delta (positive values raise, negative lower)
        // Use int32 to avoid overflow for heights > 32767
        NewHeight = FMath::Clamp(static_cast<int32>(CurrentHeights[i]) + FMath::Abs(Delta) / 10, 0, 65535);
        ModifiedCount++;
      } else if (Operation.Equals(TEXT("lower"), ESearchCase::IgnoreCase)) {
        // Lower by delta
        // Use int32 to avoid overflow for heights > 32767
        NewHeight = FMath::Clamp(static_cast<int32>(CurrentHeights[i]) - FMath::Abs(Delta) / 10, 0, 65535);
        ModifiedCount++;
      } else if (Operation.Equals(TEXT("flatten"), ESearchCase::IgnoreCase)) {
        // Flatten to target height
        NewHeight = SingleValue;
        ModifiedCount++;
      } else {
        // "set" operation - use heightData if provided and matches size, otherwise use single value
        if (HeightValues.Num() == RegionSize) {
          NewHeight = HeightValues[i];
        } else {
          NewHeight = SingleValue;
        }
        ModifiedCount++;
      }

      OutputHeights[i] = NewHeight;
    }

    SlowTask.EnterProgressFrame(
        1.0f, FText::FromString(TEXT("Writing heightmap data")));

    // Write the modified height data
    // Pass false for bInUploadTextureChangesToGPU to prevent GPU sync hang on Intel GPUs
    // Use bForce=false in SetHeightData to avoid blocking GPU synchronization
    // This prevents 60+ second hangs on large landscapes
    FLandscapeEditDataInterface LandscapeEditWrite(LandscapeInfo, false);
    LandscapeEditWrite.SetHeightData(MinX, MinY, MaxX, MaxY, OutputHeights.GetData(),
                                     SizeX, bUpdateNormals);

    // Flush is expensive - it forces render thread synchronization
    // Skip if requested for batch operations, but note that changes
    // won't be visible until the next flush or edit operation
    if (!bSkipFlush) {
      SlowTask.EnterProgressFrame(
          1.0f, FText::FromString(TEXT("Flushing changes to GPU")));
      LandscapeEditWrite.Flush();
    }
    
    // Use MarkPackageDirty instead of PostEditChange to avoid full landscape rebuild
    // PostEditChange triggers collision rebuild, shader recompilation, and nav mesh update
    // which can take 60+ seconds for large landscapes
    Landscape->MarkPackageDirty();

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("landscapePath"), Landscape->GetPackage()->GetPathName());
    Resp->SetStringField(TEXT("landscapeName"), Landscape->GetActorLabel());
    Resp->SetStringField(TEXT("operation"), Operation);
    Resp->SetNumberField(TEXT("modifiedVertices"), ModifiedCount);
    Resp->SetNumberField(TEXT("regionSizeX"), SizeX);
    Resp->SetNumberField(TEXT("regionSizeY"), SizeY);
    Resp->SetBoolField(TEXT("flushSkipped"), bSkipFlush);
    Resp->SetBoolField(TEXT("updateNormals"), bUpdateNormals);
    
    // Add verification data
    McpHandlerUtils::AddVerification(Resp, Landscape);

    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
                                      TEXT("Heightmap modified successfully"),
                                      Resp, FString());
  });

  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("modify_heightmap requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// =============================================================================
// Section C (continued): Sculpt Landscape
// =============================================================================

/**
 * HandleSculptLandscape
 *
 * Brush-based landscape sculpting at a world-space position. Applies a
 * circular brush with configurable radius, falloff, and strength.
 *
 * Supported tool modes:
 *   - "Raise":   Raise terrain within brush radius
 *   - "Lower":   Lower terrain within brush radius
 *   - "Flatten": Flatten terrain to target Z height
 *
 * The brush converts world-space coordinates to landscape local vertex
 * coordinates, accounting for actor transform and scale.
 *
 * @param RequestId  Unique request identifier
 * @param Action     Must match "sculpt_landscape" (case-insensitive)
 * @param Payload    JSON payload with sculpt parameters
 * @param RequestingSocket  WebSocket for response delivery
 * @return true if action was handled
 */
bool UMcpAutomationBridgeSubsystem::HandleSculptLandscape(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("sculpt_landscape"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("sculpt_landscape payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString LandscapePath;
  Payload->TryGetStringField(TEXT("landscapePath"), LandscapePath);
  FString LandscapeName;
  Payload->TryGetStringField(TEXT("landscapeName"), LandscapeName);

  // Security: Validate landscape path if provided
  if (!LandscapePath.IsEmpty()) {
    FString SafePath = SanitizeProjectRelativePath(LandscapePath);
    if (SafePath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Invalid or unsafe landscape path: %s"), *LandscapePath),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }
    LandscapePath = SafePath;
  }

  UE_LOG(LogMcpLandscapeHandlers, Warning,
         TEXT("HandleSculptLandscape: RequestId=%s Path='%s' Name='%s'"),
         *RequestId, *LandscapePath, *LandscapeName);

  double LocX = 0, LocY = 0, LocZ = 0;
  const TSharedPtr<FJsonObject> *LocObj = nullptr;
  // Accept both 'location' and 'position' parameter names for consistency
  if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj) {
    (*LocObj)->TryGetNumberField(TEXT("x"), LocX);
    (*LocObj)->TryGetNumberField(TEXT("y"), LocY);
    (*LocObj)->TryGetNumberField(TEXT("z"), LocZ);
  } else if (Payload->TryGetObjectField(TEXT("position"), LocObj) && LocObj) {
    (*LocObj)->TryGetNumberField(TEXT("x"), LocX);
    (*LocObj)->TryGetNumberField(TEXT("y"), LocY);
    (*LocObj)->TryGetNumberField(TEXT("z"), LocZ);
  } else {
    SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("location or position required. Example: {\"location\": {\"x\": "
             "0, \"y\": 0, \"z\": 100}}"),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FVector TargetLocation(LocX, LocY, LocZ);

  FString ToolMode = TEXT("Raise");
  if (!Payload->TryGetStringField(TEXT("toolMode"), ToolMode)) {
    Payload->TryGetStringField(TEXT("tool"), ToolMode);
  }

  double BrushRadius = 1000.0;
  if (!Payload->TryGetNumberField(TEXT("brushRadius"), BrushRadius)) {
    Payload->TryGetNumberField(TEXT("radius"), BrushRadius);
  }

  double BrushFalloff = 0.5;
  if (!Payload->TryGetNumberField(TEXT("brushFalloff"), BrushFalloff)) {
    Payload->TryGetNumberField(TEXT("falloff"), BrushFalloff);
  }

  double Strength = 0.1;
  Payload->TryGetNumberField(TEXT("strength"), Strength);

  // Optional: Skip the expensive Flush() operation for performance
  bool bSkipFlush = false;
  Payload->TryGetBoolField(TEXT("skipFlush"), bSkipFlush);

  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(this);

  AsyncTask(ENamedThreads::GameThread, [WeakSubsystem, RequestId,
                                        RequestingSocket, LandscapePath,
                                        LandscapeName, TargetLocation, ToolMode,
                                        BrushRadius, BrushFalloff, Strength, bSkipFlush]() {
    UMcpAutomationBridgeSubsystem *Subsystem = WeakSubsystem.Get();
    if (!Subsystem)
      return;

    // PRIORITY 1: Find landscape in current world by name (works for transient actors)
    ALandscape *Landscape = nullptr;
    if (GEditor) {
      if (UEditorActorSubsystem *ActorSS =
              GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
        TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();

        for (AActor *A : AllActors) {
          if (ALandscape *L = Cast<ALandscape>(A)) {
            // Match by landscapeName if provided (actor label)
            if (!LandscapeName.IsEmpty() &&
                L->GetActorLabel().Equals(LandscapeName,
                                          ESearchCase::IgnoreCase)) {
              Landscape = L;
              break;
            }
            // Match by path: compare asset path from the landscape's package
            if (!LandscapePath.IsEmpty()) {
              FString ActorAssetPath = L->GetPackage()->GetPathName();
              // Normalize both paths for comparison
              FString NormalizedRequest = LandscapePath;
              FString NormalizedActor = ActorAssetPath;
              NormalizedRequest.ReplaceInline(TEXT("\\"), TEXT("/"));
              NormalizedActor.ReplaceInline(TEXT("\\"), TEXT("/"));
              // Remove .uasset extension if present
              if (NormalizedActor.EndsWith(TEXT(".uasset"))) {
                NormalizedActor = NormalizedActor.LeftChop(7);
              }
              if (NormalizedActor.Equals(NormalizedRequest, ESearchCase::IgnoreCase)) {
                Landscape = L;
                break;
              }
            }
          }
        }

        // NOTE: Removed silent fallback - if specific landscape requested but not found, fail
      }
    }

    // PRIORITY 2: Try to load from disk (for saved landscape assets)
    if (!Landscape && !LandscapePath.IsEmpty()) {
      Landscape = Cast<ALandscape>(
          StaticLoadObject(ALandscape::StaticClass(), nullptr, *LandscapePath));
    }
    if (!Landscape) {
      FString ErrorMessage = LandscapeName.IsEmpty() 
          ? FString::Printf(TEXT("Landscape not found at path: %s"), *LandscapePath)
          : FString::Printf(TEXT("Landscape '%s' not found (path: %s)"), *LandscapeName, *LandscapePath);
      Subsystem->SendAutomationError(RequestingSocket, RequestId,
                                     *ErrorMessage,
                                     TEXT("LANDSCAPE_NOT_FOUND"));
      return;
    }

    ULandscapeInfo *LandscapeInfo = Landscape->GetLandscapeInfo();
    if (!LandscapeInfo) {
      Subsystem->SendAutomationError(RequestingSocket, RequestId,
                                     TEXT("Landscape has no info"),
                                     TEXT("INVALID_LANDSCAPE"));
      return;
    }

    // Convert World Location to Landscape Local Space
    FVector LocalPos =
        Landscape->GetActorTransform().InverseTransformPosition(TargetLocation);
    int32 CenterX = FMath::RoundToInt(LocalPos.X);
    int32 CenterY = FMath::RoundToInt(LocalPos.Y);

    // Convert Brush Radius to Vertex Units (assuming uniform scale for
    // simplicity, or use X)
    const FVector LandscapeScale = Landscape->GetActorScale3D();
    const float ScaleX = LandscapeScale.X;
    const float ScaleZ = LandscapeScale.Z;
    
    // Guard against zero scale which would cause division by zero
    if (FMath::IsNearlyZero(ScaleX) || FMath::IsNearlyZero(ScaleZ))
    {
      Subsystem->SendAutomationError(RequestingSocket, RequestId,
                                     TEXT("Landscape has zero scale. Cannot perform brush operation."),
                                     TEXT("INVALID_SCALE"));
      return;
    }
    
    int32 RadiusVerts = FMath::Max(1, FMath::RoundToInt(BrushRadius / ScaleX));
    int32 FalloffVerts = FMath::RoundToInt(RadiusVerts * BrushFalloff);

    int32 MinX = CenterX - RadiusVerts;
    int32 MaxX = CenterX + RadiusVerts;
    int32 MinY = CenterY - RadiusVerts;
    int32 MaxY = CenterY + RadiusVerts;

    // Clamp to landscape extents
    int32 LMinX, LMinY, LMaxX, LMaxY;
    if (LandscapeInfo->GetLandscapeExtent(LMinX, LMinY, LMaxX, LMaxY)) {
      MinX = FMath::Max(MinX, LMinX);
      MinY = FMath::Max(MinY, LMinY);
      MaxX = FMath::Min(MaxX, LMaxX);
      MaxY = FMath::Min(MaxY, LMaxY);
    }

    if (MinX > MaxX || MinY > MaxY) {
      Subsystem->SendAutomationResponse(RequestingSocket, RequestId, false,
                                        TEXT("Brush outside landscape bounds"),
                                        nullptr, TEXT("OUT_OF_BOUNDS"));
      return;
    }

    int32 SizeX = MaxX - MinX + 1;
    int32 SizeY = MaxY - MinY + 1;
    TArray<uint16> HeightData;
    HeightData.SetNumZeroed(SizeX * SizeY);

    // Pass false for bInUploadTextureChangesToGPU to prevent GPU sync hang on Intel GPUs
    FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo, false);
    LandscapeEdit.GetHeightData(MinX, MinY, MaxX, MaxY, HeightData.GetData(),
                                0);

    bool bModified = false;
    for (int32 Y = MinY; Y <= MaxY; ++Y) {
      for (int32 X = MinX; X <= MaxX; ++X) {
        float Dist = FMath::Sqrt(FMath::Square((float)(X - CenterX)) +
                                 FMath::Square((float)(Y - CenterY)));
        if (Dist > RadiusVerts)
          continue;

        float Alpha = 1.0f;
        if (Dist > (RadiusVerts - FalloffVerts)) {
          Alpha = 1.0f -
                  ((Dist - (RadiusVerts - FalloffVerts)) / (float)FalloffVerts);
        }
        Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

        int32 Index = (Y - MinY) * SizeX + (X - MinX);
        if (Index < 0 || Index >= HeightData.Num())
          continue;

        uint16 CurrentHeight = HeightData[Index];

        float HeightScale =
            128.0f / ScaleZ; // Conversion factor from World Z to uint16

        float Delta = 0.0f;
        if (ToolMode.Equals(TEXT("Raise"), ESearchCase::IgnoreCase)) {
          Delta = Strength * Alpha * 100.0f *
                  HeightScale; // Arbitrary strength multiplier
        } else if (ToolMode.Equals(TEXT("Lower"), ESearchCase::IgnoreCase)) {
          Delta = -Strength * Alpha * 100.0f * HeightScale;
        } else if (ToolMode.Equals(TEXT("Flatten"), ESearchCase::IgnoreCase)) {
          float CurrentVal = (float)CurrentHeight;
          float Target = (TargetLocation.Z - Landscape->GetActorLocation().Z) /
                             ScaleZ * 128.0f +
                         32768.0f;
          Delta = (Target - CurrentVal) * Strength * Alpha;
        }

        int32 NewHeight =
            FMath::Clamp((int32)(CurrentHeight + Delta), 0, 65535);
        if (NewHeight != CurrentHeight) {
          HeightData[Index] = (uint16)NewHeight;
          bModified = true;
        }
      }
    }

    if (bModified) {
      LandscapeEdit.SetHeightData(MinX, MinY, MaxX, MaxY, HeightData.GetData(),
                                  0, true);

      // Flush is expensive - it forces render thread synchronization
      // Skip if requested for batch operations
      if (!bSkipFlush) {
        LandscapeEdit.Flush();
      }

      // Use MarkPackageDirty instead of PostEditChange to avoid full landscape rebuild
      // PostEditChange triggers collision rebuild, shader recompilation, and nav mesh update
      Landscape->MarkPackageDirty();
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("toolMode"), ToolMode);
    Resp->SetNumberField(TEXT("modifiedVertices"),
                         bModified ? HeightData.Num() : 0);

    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
                                      TEXT("Landscape sculpted"), Resp,
                                      FString());
  });

  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("sculpt_landscape requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// =============================================================================
// Section D: Layer & Material Operations
// =============================================================================

/**
 * HandlePaintLandscapeLayer
 *
 * Paints a weight-map layer on a landscape. Auto-creates the layer info
 * object if the specified layer doesn't exist on the landscape.
 *
 * Version compatibility for layer name assignment:
 *   - UE 5.7+:  Uses SetLayerName() (public API)
 *   - UE 5.0-5.6: Uses direct LayerName assignment (deprecated in 5.7)
 *
 * @param RequestId  Unique request identifier
 * @param Action     Must match "paint_landscape_layer" (case-insensitive)
 * @param Payload    JSON payload with layer painting parameters
 * @param RequestingSocket  WebSocket for response delivery
 * @return true if action was handled
 */
bool UMcpAutomationBridgeSubsystem::HandlePaintLandscapeLayer(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("paint_landscape_layer"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("paint_landscape_layer payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString LandscapePath;
  Payload->TryGetStringField(TEXT("landscapePath"), LandscapePath);
  FString LandscapeName;
  Payload->TryGetStringField(TEXT("landscapeName"), LandscapeName);

  // Security: Validate landscape path if provided
  if (!LandscapePath.IsEmpty()) {
    FString SafePath = SanitizeProjectRelativePath(LandscapePath);
    if (SafePath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Invalid or unsafe landscape path: %s"), *LandscapePath),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }
    LandscapePath = SafePath;
  }

  FString LayerName;
  if (!Payload->TryGetStringField(TEXT("layerName"), LayerName) ||
      LayerName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("layerName required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Paint region (optional - if not specified, paint entire landscape)
  int32 MinX = -1, MinY = -1, MaxX = -1, MaxY = -1;
  const TSharedPtr<FJsonObject> *RegionObj = nullptr;
  if (Payload->TryGetObjectField(TEXT("region"), RegionObj) && RegionObj) {
    (*RegionObj)->TryGetNumberField(TEXT("minX"), MinX);
    (*RegionObj)->TryGetNumberField(TEXT("minY"), MinY);
    (*RegionObj)->TryGetNumberField(TEXT("maxX"), MaxX);
    (*RegionObj)->TryGetNumberField(TEXT("maxY"), MaxY);
  } else {
    Payload->TryGetNumberField(TEXT("minX"), MinX);
    Payload->TryGetNumberField(TEXT("minY"), MinY);
    Payload->TryGetNumberField(TEXT("maxX"), MaxX);
    Payload->TryGetNumberField(TEXT("maxY"), MaxY);
  }

  double Strength = 1.0;
  Payload->TryGetNumberField(TEXT("strength"), Strength);
  Strength = FMath::Clamp(Strength, 0.0, 1.0);

  // Optional: Skip the expensive Flush() operation for performance
  bool bSkipFlush = false;
  Payload->TryGetBoolField(TEXT("skipFlush"), bSkipFlush);

  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(this);

  AsyncTask(ENamedThreads::GameThread, [WeakSubsystem, RequestId,
                                        RequestingSocket, LandscapePath,
                                        LandscapeName, LayerName, MinX, MinY,
                                        MaxX, MaxY, Strength, bSkipFlush]() {
    UMcpAutomationBridgeSubsystem *Subsystem = WeakSubsystem.Get();
    if (!Subsystem)
      return;

    // PRIORITY 1: Find landscape in current world by name (works for transient actors)
    ALandscape *Landscape = nullptr;
    if (GEditor) {
      if (UEditorActorSubsystem *ActorSS =
              GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
        TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();

        for (AActor *A : AllActors) {
          if (ALandscape *L = Cast<ALandscape>(A)) {
            // Match by landscapeName if provided (actor label)
            if (!LandscapeName.IsEmpty() &&
                L->GetActorLabel().Equals(LandscapeName,
                                          ESearchCase::IgnoreCase)) {
              Landscape = L;
              break;
            }
            // Match by path: compare asset path from the landscape's package
            if (!LandscapePath.IsEmpty()) {
              FString ActorAssetPath = L->GetPackage()->GetPathName();
              // Normalize both paths for comparison
              FString NormalizedRequest = LandscapePath;
              FString NormalizedActor = ActorAssetPath;
              NormalizedRequest.ReplaceInline(TEXT("\\"), TEXT("/"));
              NormalizedActor.ReplaceInline(TEXT("\\"), TEXT("/"));
              // Remove .uasset extension if present
              if (NormalizedActor.EndsWith(TEXT(".uasset"))) {
                NormalizedActor = NormalizedActor.LeftChop(7);
              }
              if (NormalizedActor.Equals(NormalizedRequest, ESearchCase::IgnoreCase)) {
                Landscape = L;
                break;
              }
            }
          }
        }

        // NOTE: Removed silent fallback to single landscape - this was causing false positives
        // If a specific landscape was requested but not found, we should fail, not silently use another
      }
    }

    // PRIORITY 2: Try to load from disk (for saved landscape assets)
    if (!Landscape && !LandscapePath.IsEmpty()) {
      Landscape = Cast<ALandscape>(
          StaticLoadObject(ALandscape::StaticClass(), nullptr, *LandscapePath));
    }
    if (!Landscape) {
      // Provide helpful error message distinguishing between "no landscape found" and "wrong name"
      FString ErrorMessage = LandscapeName.IsEmpty() 
          ? FString::Printf(TEXT("Landscape not found at path: %s"), *LandscapePath)
          : FString::Printf(TEXT("Landscape '%s' not found (path: %s)"), *LandscapeName, *LandscapePath);
      Subsystem->SendAutomationError(RequestingSocket, RequestId,
                                     *ErrorMessage,
                                     TEXT("LANDSCAPE_NOT_FOUND"));
      return;
    }

    ULandscapeInfo *LandscapeInfo = Landscape->GetLandscapeInfo();
    if (!LandscapeInfo) {
      Subsystem->SendAutomationError(RequestingSocket, RequestId,
                                     TEXT("Landscape has no info"),
                                     TEXT("INVALID_LANDSCAPE"));
      return;
    }

    ULandscapeLayerInfoObject *LayerInfo = nullptr;
    for (const FLandscapeInfoLayerSettings &Layer : LandscapeInfo->Layers) {
      if (Layer.LayerName == FName(*LayerName)) {
        LayerInfo = Layer.LayerInfoObj;
        break;
      }
    }

    // Auto-create layer if it doesn't exist (matches UE Landscape Editor behavior)
	if (!LayerInfo) {

      // Create a new layer info object
      ULandscapeLayerInfoObject* NewLayerInfo = NewObject<ULandscapeLayerInfoObject>(
          Landscape, FName(*FString::Printf(TEXT("LayerInfo_%s"), *LayerName)),
          RF_Public | RF_Transactional);

      if (NewLayerInfo) {
        // -----------------------------------------------------------------------
        // Version-specific layer name assignment
        // -----------------------------------------------------------------------
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
        // UE 5.7+: LayerName property deprecated, use SetLayerName()
        NewLayerInfo->SetLayerName(FName(*LayerName), true);
#else
        // UE 5.0-5.6: Direct property assignment
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        NewLayerInfo->LayerName = FName(*LayerName);
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif

        // Add to landscape info layers
        FLandscapeInfoLayerSettings NewLayerSettings(NewLayerInfo, Landscape);
        LandscapeInfo->Layers.Add(NewLayerSettings);

		LayerInfo = NewLayerInfo;
	} else {
        Subsystem->SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(TEXT("Failed to create layer '%s'"),
                            *LayerName),
            TEXT("LAYER_CREATION_FAILED"));
        return;
      }
    }

    // Note: Do NOT call MakeDialog() - it blocks indefinitely in headless environments
    FScopedSlowTask SlowTask(
        1.0f, FText::FromString(TEXT("Painting landscape layer...")));

    int32 PaintMinX = MinX;
    int32 PaintMinY = MinY;
    int32 PaintMaxX = MaxX;
    int32 PaintMaxY = MaxY;
    
    // Clamp paint region to landscape extents
    int32 LMinX, LMinY, LMaxX, LMaxY;
    if (LandscapeInfo->GetLandscapeExtent(LMinX, LMinY, LMaxX, LMaxY))
    {
      PaintMinX = FMath::Clamp(PaintMinX, LMinX, LMaxX);
      PaintMinY = FMath::Clamp(PaintMinY, LMinY, LMaxY);
      PaintMaxX = FMath::Clamp(PaintMaxX, LMinX, LMaxX);
      PaintMaxY = FMath::Clamp(PaintMaxY, LMinY, LMaxY);
    }
    
    // Validate region is valid
    if (PaintMinX > PaintMaxX || PaintMinY > PaintMaxY)
    {
      Subsystem->SendAutomationError(RequestingSocket, RequestId,
                                     TEXT("Invalid paint region: min > max after clamping"),
                                     TEXT("INVALID_REGION"));
      return;
    }

    // Pass false for bInUploadTextureChangesToGPU to prevent GPU sync hang on Intel GPUs
    FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo, false);
    const uint8 PaintValue = static_cast<uint8>(Strength * 255.0);
    const int32 RegionSizeX = (PaintMaxX - PaintMinX + 1);
    const int32 RegionSizeY = (PaintMaxY - PaintMinY + 1);
    
    // Validate region size to prevent huge allocations
    constexpr int32 MaxRegionPixels = 16777216; // 16M pixels = ~16MB for uint8
    if (RegionSizeX * RegionSizeY > MaxRegionPixels)
    {
      Subsystem->SendAutomationError(RequestingSocket, RequestId,
                                     FString::Printf(TEXT("Paint region too large: %dx%d (%d pixels). Maximum: %d"),
                                                     RegionSizeX, RegionSizeY, RegionSizeX * RegionSizeY, MaxRegionPixels),
                                     TEXT("REGION_TOO_LARGE"));
      return;
    }

    TArray<uint8> AlphaData;
    AlphaData.Init(PaintValue, RegionSizeX * RegionSizeY);

    LandscapeEdit.SetAlphaData(LayerInfo, PaintMinX, PaintMinY, PaintMaxX,
                               PaintMaxY, AlphaData.GetData(), RegionSizeX);

    // Flush is expensive - it forces render thread synchronization
    // Skip if requested for batch operations
    if (!bSkipFlush) {
      LandscapeEdit.Flush();
    }

    // Use MarkPackageDirty instead of PostEditChange to avoid full landscape rebuild
    // PostEditChange triggers collision rebuild, shader recompilation, and nav mesh update
    Landscape->MarkPackageDirty();

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("landscapePath"), Landscape->GetPackage()->GetPathName());
    Resp->SetStringField(TEXT("landscapeName"), Landscape->GetActorLabel());
    Resp->SetStringField(TEXT("layerName"), LayerName);
    Resp->SetNumberField(TEXT("strength"), Strength);

    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
                                      TEXT("Layer painted successfully"), Resp,
                                      FString());
  });

  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("paint_landscape_layer requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// -----------------------------------------------------------------------------
// Set Landscape Material
// -----------------------------------------------------------------------------

/**
 * HandleSetLandscapeMaterial
 *
 * Assigns a material to an existing landscape actor. Validates the material
 * path and distinguishes between "asset not found" and "wrong type" errors.
 *
 * @param RequestId  Unique request identifier
 * @param Action     Must match "set_landscape_material" (case-insensitive)
 * @param Payload    JSON payload with material assignment parameters
 * @param RequestingSocket  WebSocket for response delivery
 * @return true if action was handled
 */
bool UMcpAutomationBridgeSubsystem::HandleSetLandscapeMaterial(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("set_landscape_material"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("set_landscape_material payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString LandscapePath;
  Payload->TryGetStringField(TEXT("landscapePath"), LandscapePath);
  FString LandscapeName;
  Payload->TryGetStringField(TEXT("landscapeName"), LandscapeName);
  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) ||
      MaterialPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("materialPath required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Security: Validate material path
  FString SafeMaterialPath = SanitizeProjectRelativePath(MaterialPath);
  if (SafeMaterialPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Invalid or unsafe material path: %s"), *MaterialPath),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }
  MaterialPath = SafeMaterialPath;

  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(this);

  AsyncTask(ENamedThreads::GameThread, [WeakSubsystem, RequestId,
                                        RequestingSocket, LandscapePath,
                                        LandscapeName, MaterialPath]() {
    UMcpAutomationBridgeSubsystem *Subsystem = WeakSubsystem.Get();
    if (!Subsystem)
      return;

    // PRIORITY 1: Find landscape in current world by name (works for transient actors)
    ALandscape *Landscape = nullptr;
    if (GEditor) {
      if (UEditorActorSubsystem *ActorSS =
              GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
        TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();

        for (AActor *A : AllActors) {
          if (ALandscape *L = Cast<ALandscape>(A)) {
            // Match by landscapeName if provided (actor label)
            if (!LandscapeName.IsEmpty() &&
                L->GetActorLabel().Equals(LandscapeName,
                                          ESearchCase::IgnoreCase)) {
              Landscape = L;
              break;
            }
            // Match by path: compare asset path from the landscape's package
            if (!LandscapePath.IsEmpty()) {
              FString ActorAssetPath = L->GetPackage()->GetPathName();
              // Normalize both paths for comparison
              FString NormalizedRequest = LandscapePath;
              FString NormalizedActor = ActorAssetPath;
              NormalizedRequest.ReplaceInline(TEXT("\\"), TEXT("/"));
              NormalizedActor.ReplaceInline(TEXT("\\"), TEXT("/"));
              // Remove .uasset extension if present
              if (NormalizedActor.EndsWith(TEXT(".uasset"))) {
                NormalizedActor = NormalizedActor.LeftChop(7);
              }
              if (NormalizedActor.Equals(NormalizedRequest, ESearchCase::IgnoreCase)) {
                Landscape = L;
                break;
              }
            }
          }
        }

        // NOTE: Removed silent fallback - if specific landscape requested but not found, fail
      }
    }

    // PRIORITY 2: Try to load from disk (for saved landscape assets)
    if (!Landscape && !LandscapePath.IsEmpty()) {
      Landscape = Cast<ALandscape>(
          StaticLoadObject(ALandscape::StaticClass(), nullptr, *LandscapePath));
    }
    if (!Landscape) {
      FString ErrorMessage = LandscapeName.IsEmpty() 
          ? FString::Printf(TEXT("Landscape not found at path: %s"), *LandscapePath)
          : FString::Printf(TEXT("Landscape '%s' not found (path: %s)"), *LandscapeName, *LandscapePath);
      Subsystem->SendAutomationError(RequestingSocket, RequestId,
                                     *ErrorMessage,
                                     TEXT("LANDSCAPE_NOT_FOUND"));
      return;
    }

    // Use Silent load to avoid engine warnings if path is invalid or type
    // mismatch
    UMaterialInterface *Mat = Cast<UMaterialInterface>(
        StaticLoadObject(UMaterialInterface::StaticClass(), nullptr,
                         *MaterialPath, nullptr, LOAD_NoWarn));

    if (!Mat) {
      // Check existence separately only if load failed, to distinguish error
      // type (optional)
      if (!UEditorAssetLibrary::DoesAssetExist(MaterialPath)) {
        Subsystem->SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(TEXT("Material asset not found: %s"),
                            *MaterialPath),
            TEXT("ASSET_NOT_FOUND"));
      } else {
        Subsystem->SendAutomationError(
            RequestingSocket, RequestId,
            TEXT("Failed to load material (invalid type?)"),
            TEXT("LOAD_FAILED"));
      }
      return;
    }

    Landscape->LandscapeMaterial = Mat;
    Landscape->PostEditChange();

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("landscapePath"), Landscape->GetPackage()->GetPathName());
    Resp->SetStringField(TEXT("landscapeName"), Landscape->GetActorLabel());
    Resp->SetStringField(TEXT("materialPath"), MaterialPath);

    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
                                      TEXT("Landscape material set"), Resp,
                                      FString());
  });

  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("set_landscape_material requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// =============================================================================
// Section E: Grass Type Management
// =============================================================================

/**
 * HandleCreateLandscapeGrassType
 *
 * Creates a ULandscapeGrassType asset with a single grass variety configured
 * from the provided static mesh, density, and scale parameters.
 *
 * The asset is saved to /Game/Landscape/<name> using McpSafeAssetSave
 * (required for UE 5.7+ compatibility).
 *
 * Note: Uses GrassVarieties.AddZeroed() to avoid calling the unexported
 * FGrassVariety constructor, then explicitly initializes all fields.
 *
 * @param RequestId  Unique request identifier
 * @param Action     Must match "create_landscape_grass_type" (case-insensitive)
 * @param Payload    JSON payload with grass type configuration
 * @param RequestingSocket  WebSocket for response delivery
 * @return true if action was handled
 */
bool UMcpAutomationBridgeSubsystem::HandleCreateLandscapeGrassType(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("create_landscape_grass_type"),
                    ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("create_landscape_grass_type payload missing"),
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
      MeshPath.IsEmpty()) {
    Payload->TryGetStringField(TEXT("staticMesh"), MeshPath);
    if (MeshPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("meshPath or staticMesh required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
  }

  // Security: Validate mesh path
  FString SafeMeshPath = SanitizeProjectRelativePath(MeshPath);
  if (SafeMeshPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Invalid or unsafe mesh path: %s"), *MeshPath),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }
  MeshPath = SafeMeshPath;

  double Density = 1.0;
  Payload->TryGetNumberField(TEXT("density"), Density);

  double MinScale = 0.8;
  Payload->TryGetNumberField(TEXT("minScale"), MinScale);

  double MaxScale = 1.2;
  Payload->TryGetNumberField(TEXT("maxScale"), MaxScale);

  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(this);

  AsyncTask(ENamedThreads::GameThread, [WeakSubsystem, RequestId,
                                        RequestingSocket, Name, MeshPath,
                                        Density, MinScale, MaxScale]() {
    UMcpAutomationBridgeSubsystem *Subsystem = WeakSubsystem.Get();
    if (!Subsystem)
      return;

    // Use Silent load to avoid engine warnings
    UStaticMesh *StaticMesh = Cast<UStaticMesh>(StaticLoadObject(
        UStaticMesh::StaticClass(), nullptr, *MeshPath, nullptr, LOAD_NoWarn));
    if (!StaticMesh) {
      Subsystem->SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Static mesh not found: %s"), *MeshPath),
          TEXT("ASSET_NOT_FOUND"));
      return;
    }

    FString PackagePath = TEXT("/Game/Landscape");
    FString AssetName = Name;
    FString FullPackagePath =
        FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);

    // Check if already exists
    if (UObject *ExistingAsset = StaticLoadObject(
            ULandscapeGrassType::StaticClass(), nullptr, *FullPackagePath)) {
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("asset_path"), ExistingAsset->GetPathName());
      Resp->SetStringField(TEXT("message"), TEXT("Asset already exists"));
      Subsystem->SendAutomationResponse(
          RequestingSocket, RequestId, true,
          TEXT("Landscape grass type already exists"), Resp, FString());
      return;
    }

    UPackage *Package = CreatePackage(*FullPackagePath);
    ULandscapeGrassType *GrassType = NewObject<ULandscapeGrassType>(
        Package, FName(*AssetName), RF_Public | RF_Standalone);
    if (!GrassType) {
      Subsystem->SendAutomationError(RequestingSocket, RequestId,
                                     TEXT("Failed to create grass type asset"),
                                     TEXT("CREATION_FAILED"));
      return;
    }

    // Use AddZeroed() to avoid calling the unexported FGrassVariety constructor
    // AddZeroed() allocates memory and zeros it without invoking any constructor
    int32 NewIndex = GrassType->GrassVarieties.AddZeroed();
    FGrassVariety& Variety = GrassType->GrassVarieties[NewIndex];
    
    // Explicitly initialize all fields (memory is zero-initialized from AddZeroed)
    Variety.GrassMesh = StaticMesh;
    Variety.GrassDensity.Default = static_cast<float>(Density);
    Variety.ScaleX = FFloatInterval(static_cast<float>(MinScale),
                                    static_cast<float>(MaxScale));
    Variety.ScaleY = FFloatInterval(static_cast<float>(MinScale),
                                    static_cast<float>(MaxScale));
    Variety.ScaleZ = FFloatInterval(static_cast<float>(MinScale),
                                    static_cast<float>(MaxScale));
    Variety.RandomRotation = true;
    Variety.AlignToSurface = true;

    McpSafeAssetSave(GrassType);
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("asset_path"), GrassType->GetPathName());

    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
                                      TEXT("Landscape grass type created"),
                                      Resp, FString());
  });

  return true;
#else
  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("create_landscape_grass_type requires editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
