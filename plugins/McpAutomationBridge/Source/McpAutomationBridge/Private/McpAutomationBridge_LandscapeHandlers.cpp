#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Runtime/Launch/Resources/Version.h"
#include "ScopedTransaction.h"


#if WITH_EDITOR
#include "Async/Async.h"
#include "EditorAssetLibrary.h"
#include "Engine/World.h"
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
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/ScopedSlowTask.h"
#include "UObject/SavePackage.h"

#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#endif

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
  Payload->TryGetNumberField(TEXT("componentCount"), ComponentCount);
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
    Payload->TryGetNumberField(TEXT("quadsPerSection"), QuadsPerComponent);
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

  // Debug log to confirm name capture
  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("HandleCreateLandscape: Captured name '%s' (from override '%s')"),
         *CaptName, *NameOverride);

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

        UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
               TEXT("HandleCreateLandscape: Applied height data via "
                    "FLandscapeEditDataInterface (%d vertices)"),
               HeightArray.Num());
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
      UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
             TEXT("HandleCreateLandscape: Set ActorLabel to '%s'"), *CaptName);
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

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("landscapePath"), Landscape->GetPathName());
    Resp->SetStringField(TEXT("actorLabel"), Landscape->GetActorLabel());
    Resp->SetNumberField(TEXT("componentsX"), CaptComponentsX);
    Resp->SetNumberField(TEXT("componentsY"), CaptComponentsY);
    Resp->SetNumberField(TEXT("quadsPerComponent"), CaptQuadsPerComponent);

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

  // Operation: raise, lower, flatten, set (default: set)
  FString Operation = TEXT("set");
  Payload->TryGetStringField(TEXT("operation"), Operation);

  // Optional region for partial updates
  int32 RegionMinX = -1, RegionMinY = -1, RegionMaxX = -1, RegionMaxY = -1;
  const TSharedPtr<FJsonObject> *RegionObj = nullptr;
  if (Payload->TryGetObjectField(TEXT("region"), RegionObj) && RegionObj) {
    (*RegionObj)->TryGetNumberField(TEXT("minX"), RegionMinX);
    (*RegionObj)->TryGetNumberField(TEXT("minY"), RegionMinY);
    (*RegionObj)->TryGetNumberField(TEXT("maxX"), RegionMaxX);
    (*RegionObj)->TryGetNumberField(TEXT("maxY"), RegionMaxY);
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
                                            MoveTemp(HeightValues), bSkipFlush]() {
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
                                     SizeX, false);

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

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("landscapePath"), Landscape->GetPackage()->GetPathName());
    Resp->SetStringField(TEXT("landscapeName"), Landscape->GetActorLabel());
    Resp->SetStringField(TEXT("operation"), Operation);
    Resp->SetNumberField(TEXT("modifiedVertices"), ModifiedCount);
    Resp->SetNumberField(TEXT("regionSizeX"), SizeX);
    Resp->SetNumberField(TEXT("regionSizeY"), SizeY);
    Resp->SetBoolField(TEXT("flushSkipped"), bSkipFlush);
    
    // Add verification data
    AddActorVerification(Resp, Landscape);

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
      UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
             TEXT("HandlePaintLandscapeLayer: Layer '%s' not found, auto-creating..."),
             *LayerName);

      // Create a new layer info object
      ULandscapeLayerInfoObject* NewLayerInfo = NewObject<ULandscapeLayerInfoObject>(
          Landscape, FName(*FString::Printf(TEXT("LayerInfo_%s"), *LayerName)),
          RF_Public | RF_Transactional);

      if (NewLayerInfo) {
        // Set the layer name using the public API (handles deprecation in 5.7+)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
        NewLayerInfo->SetLayerName(FName(*LayerName), true);
#else
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        NewLayerInfo->LayerName = FName(*LayerName);
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif

        // Add to landscape info layers
        FLandscapeInfoLayerSettings NewLayerSettings(NewLayerInfo, Landscape);
        LandscapeInfo->Layers.Add(NewLayerSettings);

        LayerInfo = NewLayerInfo;

        UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
               TEXT("HandlePaintLandscapeLayer: Auto-created layer '%s'"),
               *LayerName);
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
    if (PaintMinX < 0 || PaintMaxX < 0) {
      LandscapeInfo->GetLandscapeExtent(PaintMinX, PaintMinY, PaintMaxX,
                                        PaintMaxY);
    }

    // Pass false for bInUploadTextureChangesToGPU to prevent GPU sync hang on Intel GPUs
    FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo, false);
    const uint8 PaintValue = static_cast<uint8>(Strength * 255.0);
    const int32 RegionSizeX = (PaintMaxX - PaintMinX + 1);
    const int32 RegionSizeY = (PaintMaxY - PaintMinY + 1);

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

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
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

  UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
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
  Payload->TryGetStringField(TEXT("toolMode"), ToolMode);

  double BrushRadius = 1000.0;
  Payload->TryGetNumberField(TEXT("brushRadius"), BrushRadius);

  double BrushFalloff = 0.5;
  Payload->TryGetNumberField(TEXT("brushFalloff"), BrushFalloff);

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
    float ScaleX = Landscape->GetActorScale3D().X;
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

        float ScaleZ = Landscape->GetActorScale3D().Z;
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

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
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

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
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
    SendAutomationError(RequestingSocket, RequestId, TEXT("meshPath required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
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
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
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
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
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
