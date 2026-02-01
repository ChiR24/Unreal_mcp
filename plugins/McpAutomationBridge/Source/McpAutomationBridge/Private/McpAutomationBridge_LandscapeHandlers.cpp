#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Runtime/Launch/Resources/Version.h"
#include "ScopedTransaction.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Math/UnrealMathUtility.h"


#if WITH_EDITOR
#include "Async/Async.h"
#include "EditorAssetLibrary.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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
#if __has_include("LandscapeLayerInfoObject.h")
#include "LandscapeLayerInfoObject.h"
#endif
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/ScopedSlowTask.h"
// Note: SavePackage.h removed - use McpSafeAssetSave() from McpAutomationBridgeHelpers.h instead

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
  if (!GEditor || !GetActiveWorld()) {
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
    UWorld *World = GetActiveWorld();
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

#else
            // UE 5.6 and older: Use standard Import() workflow
            Landscape->Import(FGuid::NewGuid(), 0, 0, CaptComponentsX - 1, CaptComponentsY - 1, CaptSectionsPerComponent, CaptQuadsPerComponent, ImportHeightData, nullptr, ImportLayerInfos, ELandscapeImportAlphamapType::Layered, TArrayView<const FLandscapeLayer>(EditLayers));
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

  const TArray<TSharedPtr<FJsonValue>> *HeightDataArray = nullptr;
  if (!Payload->TryGetArrayField(TEXT("heightData"), HeightDataArray) ||
      !HeightDataArray || HeightDataArray->Num() == 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("heightData array required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Copy height data for async task
  TArray<uint16> HeightValues;
  for (const TSharedPtr<FJsonValue> &Val : *HeightDataArray) {
    if (Val.IsValid() && Val->Type == EJson::Number) {
      HeightValues.Add(
          static_cast<uint16>(FMath::Clamp(Val->AsNumber(), 0.0, 65535.0)));
    }
  }

  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(this);

  // Dispatch to Game Thread
  AsyncTask(ENamedThreads::GameThread, [WeakSubsystem, RequestId,
                                        RequestingSocket, LandscapePath,
                                        LandscapeName,
                                        HeightValues =
                                            MoveTemp(HeightValues)]() {
    UMcpAutomationBridgeSubsystem *Subsystem = WeakSubsystem.Get();
    if (!Subsystem)
      return;

    ALandscape *Landscape = nullptr;
    if (!LandscapePath.IsEmpty()) {
      Landscape = Cast<ALandscape>(
          StaticLoadObject(ALandscape::StaticClass(), nullptr, *LandscapePath));
    }

    // Find landscape with fallback to single instance
    if (!Landscape && GEditor) {
      if (UEditorActorSubsystem *ActorSS =
              GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
        TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
        ALandscape *Fallback = nullptr;
        int32 Count = 0;

        for (AActor *A : AllActors) {
          if (ALandscape *L = Cast<ALandscape>(A)) {
            Count++;
            Fallback = L;
            if (!LandscapeName.IsEmpty() &&
                L->GetActorLabel().Equals(LandscapeName,
                                          ESearchCase::IgnoreCase)) {
              Landscape = L;
              break;
            }
          }
        }

        if (!Landscape && Count == 1) {
          Landscape = Fallback;
        }
      }
    }
    if (!Landscape) {
      Subsystem->SendAutomationError(RequestingSocket, RequestId,
                                     TEXT("Failed to find landscape"),
                                     TEXT("LOAD_FAILED"));
      return;
    }

    ULandscapeInfo *LandscapeInfo = Landscape->GetLandscapeInfo();
    if (!LandscapeInfo) {
      Subsystem->SendAutomationError(RequestingSocket, RequestId,
                                     TEXT("Landscape has no info"),
                                     TEXT("INVALID_LANDSCAPE"));
      return;
    }

    // Ensure landscape components are registered and initialized
    // This fixes the INVALID_LANDSCAPE error for newly created landscapes
    if (Landscape->GetRootComponent() &&
        !Landscape->GetRootComponent()->IsRegistered()) {
      Landscape->RegisterAllComponents();
    }
    
    // Force landscape info update to ensure valid extents
    LandscapeInfo->UpdateLayerInfoMap();

    FScopedSlowTask SlowTask(2.0f,
                             FText::FromString(TEXT("Modifying heightmap...")));
    SlowTask.MakeDialog();

    int32 MinX, MinY, MaxX, MaxY;
    if (!LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY)) {
      // If extent is still not available, the landscape might not be fully initialized
      // Try to recreate collision components which forces extent calculation
      Landscape->RecreateCollisionComponents();
      
      // Try again after recreating collision
      if (!LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY)) {
        Subsystem->SendAutomationError(RequestingSocket, RequestId,
                                       TEXT("Failed to get landscape extent. Landscape may not be fully initialized."),
                                       TEXT("INVALID_LANDSCAPE"));
        return;
      }
    }

    SlowTask.EnterProgressFrame(
        1.0f, FText::FromString(TEXT("Writing heightmap data")));

    const int32 SizeX = (MaxX - MinX + 1);
    const int32 SizeY = (MaxY - MinY + 1);

    if (HeightValues.Num() != SizeX * SizeY) {
      Subsystem->SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Height data size mismatch. Expected %d x %d = "
                               "%d values, got %d"),
                          SizeX, SizeY, SizeX * SizeY, HeightValues.Num()),
          TEXT("INVALID_ARGUMENT"));
      return;
    }

    FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
    LandscapeEdit.SetHeightData(MinX, MinY, MaxX, MaxY, HeightValues.GetData(),
                                SizeX, true);

    SlowTask.EnterProgressFrame(
        1.0f, FText::FromString(TEXT("Rebuilding collision")));
    LandscapeEdit.Flush();
    Landscape->PostEditChange();

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("landscapePath"), LandscapePath);
    Resp->SetNumberField(TEXT("modifiedVertices"), HeightValues.Num());

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

  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(this);

  AsyncTask(ENamedThreads::GameThread, [WeakSubsystem, RequestId,
                                        RequestingSocket, LandscapePath,
                                        LandscapeName, LayerName, MinX, MinY,
                                        MaxX, MaxY, Strength]() {
    UMcpAutomationBridgeSubsystem *Subsystem = WeakSubsystem.Get();
    if (!Subsystem)
      return;

    ALandscape *Landscape = nullptr;
    if (!LandscapePath.IsEmpty()) {
      Landscape = Cast<ALandscape>(
          StaticLoadObject(ALandscape::StaticClass(), nullptr, *LandscapePath));
    }
    if (!Landscape && !LandscapeName.IsEmpty()) {
      if (UEditorActorSubsystem *ActorSS =
              GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
        TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
        for (AActor *A : AllActors) {
          if (A && A->IsA<ALandscape>() &&
              A->GetActorLabel().Equals(LandscapeName,
                                        ESearchCase::IgnoreCase)) {
            Landscape = Cast<ALandscape>(A);
            break;
          }
        }
      }
    }
    if (!Landscape) {
      Subsystem->SendAutomationError(RequestingSocket, RequestId,
                                     TEXT("Failed to find landscape"),
                                     TEXT("LOAD_FAILED"));
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

    if (!LayerInfo) {
      // Auto-create landscape layer info if it doesn't exist
      FString LayerPath = TEXT("/Game/Landscape/Layers");
      FString FullLayerPath = LayerPath / LayerName;
      
      // Create package for the new layer info
      UPackage* LayerPackage = CreatePackage(*FullLayerPath);
      if (LayerPackage) {
        LayerInfo = NewObject<ULandscapeLayerInfoObject>(LayerPackage, FName(*LayerName), RF_Public | RF_Standalone);
        if (LayerInfo) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
          LayerInfo->SetLayerName(FName(*LayerName), true);
#else
          LayerInfo->LayerName = FName(*LayerName);
#endif
          
          // Register with LandscapeInfo
          LandscapeInfo->Layers.Add(FLandscapeInfoLayerSettings(LayerInfo, Landscape));
          LandscapeInfo->UpdateLayerInfoMap();
          
          // Save the new asset
          McpSafeAssetSave(LayerInfo);
          
          UE_LOG(LogMcpAutomationBridgeSubsystem, Display, 
                 TEXT("HandlePaintLandscapeLayer: Auto-created layer info for '%s' at '%s'"), 
                 *LayerName, *FullLayerPath);
        }
      }
    }

    if (!LayerInfo) {
      Subsystem->SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Layer '%s' not found and could not be auto-created."),
                          *LayerName),
          TEXT("LAYER_NOT_FOUND"));
      return;
    }

    FScopedSlowTask SlowTask(
        1.0f, FText::FromString(TEXT("Painting landscape layer...")));
    SlowTask.MakeDialog();

    int32 PaintMinX = MinX;
    int32 PaintMinY = MinY;
    int32 PaintMaxX = MaxX;
    int32 PaintMaxY = MaxY;
    if (PaintMinX < 0 || PaintMaxX < 0) {
      LandscapeInfo->GetLandscapeExtent(PaintMinX, PaintMinY, PaintMaxX,
                                        PaintMaxY);
    }

    FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
    const uint8 PaintValue = static_cast<uint8>(Strength * 255.0);
    const int32 RegionSizeX = (PaintMaxX - PaintMinX + 1);
    const int32 RegionSizeY = (PaintMaxY - PaintMinY + 1);

    TArray<uint8> AlphaData;
    AlphaData.Init(PaintValue, RegionSizeX * RegionSizeY);

    LandscapeEdit.SetAlphaData(LayerInfo, PaintMinX, PaintMinY, PaintMaxX,
                               PaintMaxY, AlphaData.GetData(), RegionSizeX);
    LandscapeEdit.Flush();
    Landscape->PostEditChange();

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("landscapePath"), LandscapePath);
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

  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(this);

  AsyncTask(ENamedThreads::GameThread, [WeakSubsystem, RequestId,
                                        RequestingSocket, LandscapePath,
                                        LandscapeName, TargetLocation, ToolMode,
                                        BrushRadius, BrushFalloff, Strength]() {
    UMcpAutomationBridgeSubsystem *Subsystem = WeakSubsystem.Get();
    if (!Subsystem)
      return;

    ALandscape *Landscape = nullptr;
    if (!LandscapePath.IsEmpty()) {
      Landscape = Cast<ALandscape>(
          StaticLoadObject(ALandscape::StaticClass(), nullptr, *LandscapePath));
    }

    if (!Landscape && GEditor) {
      if (UEditorActorSubsystem *ActorSS =
              GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
        TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
        ALandscape *Fallback = nullptr;
        int32 LandscapeCount = 0;

        for (AActor *A : AllActors) {
          if (ALandscape *L = Cast<ALandscape>(A)) {
            LandscapeCount++;
            Fallback = L;

            if (!LandscapeName.IsEmpty() &&
                L->GetActorLabel().Equals(LandscapeName,
                                          ESearchCase::IgnoreCase)) {
              Landscape = L;
              break;
            }
          }
        }

        if (!Landscape && LandscapeCount == 1) {
          Landscape = Fallback;
          UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                 TEXT("HandleSculptLandscape: Exact match for '%s' not found, "
                      "using single available Landscape: '%s'"),
                 *LandscapeName, *Landscape->GetActorLabel());
        }
      }
    }
    if (!Landscape) {
      Subsystem->SendAutomationError(RequestingSocket, RequestId,
                                     TEXT("Failed to find landscape"),
                                     TEXT("LOAD_FAILED"));
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

    FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
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
      LandscapeEdit.Flush();
      Landscape->PostEditChange();
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

  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(this);

  AsyncTask(ENamedThreads::GameThread, [WeakSubsystem, RequestId,
                                        RequestingSocket, LandscapePath,
                                        LandscapeName, MaterialPath]() {
    UMcpAutomationBridgeSubsystem *Subsystem = WeakSubsystem.Get();
    if (!Subsystem)
      return;

    ALandscape *Landscape = nullptr;
    if (!LandscapePath.IsEmpty()) {
      Landscape = Cast<ALandscape>(
          StaticLoadObject(ALandscape::StaticClass(), nullptr, *LandscapePath));
    }
    if (!Landscape && !LandscapeName.IsEmpty()) {
      if (UEditorActorSubsystem *ActorSS =
              GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
        TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
        for (AActor *A : AllActors) {
          if (A && A->IsA<ALandscape>() &&
              A->GetActorLabel().Equals(LandscapeName,
                                        ESearchCase::IgnoreCase)) {
            Landscape = Cast<ALandscape>(A);
            break;
          }
        }
      }
    }

    // Fallback: If no path/name provided (or name not found but let's be
    // generous if no path was given), find first available landscape
    if (!Landscape && LandscapePath.IsEmpty() && LandscapeName.IsEmpty()) {
      if (UEditorActorSubsystem *ActorSS =
              GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
        TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
        for (AActor *A : AllActors) {
          if (ALandscape *L = Cast<ALandscape>(A)) {
            Landscape = L;
            break;
          }
        }
      }
    }
    if (!Landscape) {
      Subsystem->SendAutomationError(
          RequestingSocket, RequestId,
          TEXT("Failed to find landscape and no name provided"),
          TEXT("LOAD_FAILED"));
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
    Resp->SetStringField(TEXT("landscapePath"), Landscape->GetPathName());
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

    FGrassVariety Variety;
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

    GrassType->GrassVarieties.Add(Variety);

    // Verify asset was saved successfully
    // McpSafeAssetSave returns true if the asset was marked dirty successfully
    bool bSaveSuccess = McpSafeAssetSave(GrassType);
    
    // Additional verification: check if the asset exists and is valid
    bool bAssetExists = false;
    if (bSaveSuccess && GrassType) {
      // Try to find the asset in the package
      UObject* VerifyAsset = StaticLoadObject(
          ULandscapeGrassType::StaticClass(), nullptr, *FullPackagePath, nullptr, LOAD_NoWarn);
      bAssetExists = (VerifyAsset != nullptr);
    }
    
    if (!bSaveSuccess || !bAssetExists) {
      FString ErrorMsg = !bSaveSuccess 
          ? TEXT("Failed to save grass type asset (package may be Untitled)")
          : TEXT("Asset creation succeeded but verification failed");
      FString ErrorCode = !bSaveSuccess ? TEXT("SAVE_FAILED") : TEXT("VERIFICATION_FAILED");
      Subsystem->SendAutomationError(RequestingSocket, RequestId, ErrorMsg, ErrorCode);
      return;
    }
    
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("asset_path"), GrassType->GetPathName());
    Resp->SetBoolField(TEXT("assetSaved"), true);
    Resp->SetBoolField(TEXT("assetVerified"), bAssetExists);

    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
                                      TEXT("Landscape grass type created and saved successfully"),
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

// ============================================================================
// Phase 28: Extended Landscape Actions
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleImportHeightmap(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("import_heightmap"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("import_heightmap payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString FilePath;
  FString LandscapeName;
  if (!Payload->TryGetStringField(TEXT("filePath"), FilePath)) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("filePath required for import_heightmap"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }
  Payload->TryGetStringField(TEXT("landscapeName"), LandscapeName);

  // Read heightmap dimensions
  int32 Width = 0, Height = 0;
  Payload->TryGetNumberField(TEXT("width"), Width);
  Payload->TryGetNumberField(TEXT("height"), Height);

  // Import scale
  double ScaleX = 100.0, ScaleY = 100.0, ScaleZ = 100.0;
  Payload->TryGetNumberField(TEXT("scaleX"), ScaleX);
  Payload->TryGetNumberField(TEXT("scaleY"), ScaleY);
  Payload->TryGetNumberField(TEXT("scaleZ"), ScaleZ);

  // Check file exists
  if (!FPaths::FileExists(FilePath)) {
    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Heightmap file not found: %s"), *FilePath),
                        TEXT("FILE_NOT_FOUND"));
    return true;
  }

  // Read raw heightmap data
  TArray<uint8> RawData;
  if (!FFileHelper::LoadFileToArray(RawData, *FilePath)) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to read heightmap file"),
                        TEXT("READ_FAILED"));
    return true;
  }

  // Validate dimensions
  if (Width <= 0 || Height <= 0) {
    // Try to infer from file size (assuming 16-bit heightmap)
    int32 PixelCount = RawData.Num() / 2;
    int32 InferredSize = FMath::RoundToInt(FMath::Sqrt(static_cast<float>(PixelCount)));
    if (InferredSize * InferredSize == PixelCount) {
      Width = InferredSize;
      Height = InferredSize;
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Could not infer heightmap dimensions. Provide width and height."),
                          TEXT("INVALID_DIMENSIONS"));
      return true;
    }
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("filePath"), FilePath);
  Resp->SetNumberField(TEXT("width"), Width);
  Resp->SetNumberField(TEXT("height"), Height);
  Resp->SetNumberField(TEXT("dataSize"), RawData.Num());
  Resp->SetStringField(TEXT("message"), TEXT("Heightmap data loaded. Create landscape with create_landscape action using this data."));

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Heightmap imported successfully"), Resp, FString());
  return true;
#else
  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("import_heightmap requires editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleExportHeightmap(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("export_heightmap"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("export_heightmap payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString OutputPath;
  FString LandscapeName;
  if (!Payload->TryGetStringField(TEXT("outputPath"), OutputPath)) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("outputPath required for export_heightmap"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }
  Payload->TryGetStringField(TEXT("landscapeName"), LandscapeName);

  if (!GEditor) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UWorld *World = GetActiveWorld();
  if (!World) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("No world available"),
                        TEXT("WORLD_NOT_AVAILABLE"));
    return true;
  }

  // Find landscape
  ALandscapeProxy *TargetLandscape = nullptr;
  for (TActorIterator<ALandscapeProxy> It(World); It; ++It) {
    ALandscapeProxy *Landscape = *It;
    if (Landscape && (LandscapeName.IsEmpty() || 
        Landscape->GetActorLabel().Equals(LandscapeName, ESearchCase::IgnoreCase))) {
      TargetLandscape = Landscape;
      break;
    }
  }

  if (!TargetLandscape) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("No landscape found in level"),
                        TEXT("LANDSCAPE_NOT_FOUND"));
    return true;
  }

  // Get landscape info
  ULandscapeInfo *LandscapeInfo = TargetLandscape->GetLandscapeInfo();
  if (!LandscapeInfo) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to get landscape info"),
                        TEXT("LANDSCAPE_INFO_MISSING"));
    return true;
  }

  // Get bounds
  int32 MinX = MAX_int32, MinY = MAX_int32, MaxX = MIN_int32, MaxY = MIN_int32;
  LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY);

  int32 Width = MaxX - MinX + 1;
  int32 Height = MaxY - MinY + 1;

  // Export heightmap data
  TArray<uint16> HeightData;
  HeightData.SetNum(Width * Height);

  // Read height data from landscape
  FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
  LandscapeEdit.GetHeightData(MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0);

  // Convert to raw bytes and save
  TArray<uint8> RawData;
  RawData.SetNum(HeightData.Num() * 2);
  FMemory::Memcpy(RawData.GetData(), HeightData.GetData(), RawData.Num());

  if (!FFileHelper::SaveArrayToFile(RawData, *OutputPath)) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to write heightmap file"),
                        TEXT("WRITE_FAILED"));
    return true;
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("outputPath"), OutputPath);
  Resp->SetNumberField(TEXT("width"), Width);
  Resp->SetNumberField(TEXT("height"), Height);
  Resp->SetStringField(TEXT("landscapeName"), TargetLandscape->GetActorLabel());

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Heightmap exported successfully"), Resp, FString());
  return true;
#else
  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("export_heightmap requires editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleConfigureLandscapeLOD(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("configure_landscape_lod"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("configure_landscape_lod payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString LandscapeName;
  Payload->TryGetStringField(TEXT("landscapeName"), LandscapeName);

  // LOD settings
  double LODBias = 0.0;
  double LODDistributionSetting = 0.0;
  int32 StaticLightingLOD = -1;
  bool bUseDynamicMaterialInstance = false;

  Payload->TryGetNumberField(TEXT("lodBias"), LODBias);
  Payload->TryGetNumberField(TEXT("lodDistributionSetting"), LODDistributionSetting);
  Payload->TryGetNumberField(TEXT("staticLightingLOD"), StaticLightingLOD);
  Payload->TryGetBoolField(TEXT("useDynamicMaterialInstance"), bUseDynamicMaterialInstance);

  if (!GEditor) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UWorld *World = GetActiveWorld();
  if (!World) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("No world available"),
                        TEXT("WORLD_NOT_AVAILABLE"));
    return true;
  }

  // Find landscape
  ALandscapeProxy *TargetLandscape = nullptr;
  for (TActorIterator<ALandscapeProxy> It(World); It; ++It) {
    ALandscapeProxy *Landscape = *It;
    if (Landscape && (LandscapeName.IsEmpty() || 
        Landscape->GetActorLabel().Equals(LandscapeName, ESearchCase::IgnoreCase))) {
      TargetLandscape = Landscape;
      break;
    }
  }

  if (!TargetLandscape) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("No landscape found in level"),
                        TEXT("LANDSCAPE_NOT_FOUND"));
    return true;
  }

  // Apply LOD settings
  TargetLandscape->Modify();
  
  // LOD bias is on each component
  for (ULandscapeComponent *Component : TargetLandscape->LandscapeComponents) {
    if (Component) {
      Component->SetLODBias(static_cast<int8>(FMath::Clamp(LODBias, -2.0, 2.0)));
    }
  }

  // Static lighting LOD
  if (StaticLightingLOD >= 0) {
    TargetLandscape->StaticLightingLOD = StaticLightingLOD;
  }

  // Use dynamic material instance
  TargetLandscape->bUseDynamicMaterialInstance = bUseDynamicMaterialInstance;

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("landscapeName"), TargetLandscape->GetActorLabel());
  Resp->SetNumberField(TEXT("lodBias"), LODBias);
  Resp->SetNumberField(TEXT("staticLightingLOD"), TargetLandscape->StaticLightingLOD);
  Resp->SetBoolField(TEXT("useDynamicMaterialInstance"), TargetLandscape->bUseDynamicMaterialInstance);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Landscape LOD configured"), Resp, FString());
  return true;
#else
  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("configure_landscape_lod requires editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGetLandscapeInfo(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("get_landscape_info"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  FString LandscapeName;
  if (Payload.IsValid()) {
    Payload->TryGetStringField(TEXT("landscapeName"), LandscapeName);
  }

  if (!GEditor) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UWorld *World = GetActiveWorld();
  if (!World) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("No world available"),
                        TEXT("WORLD_NOT_AVAILABLE"));
    return true;
  }

  // Find landscape
  ALandscapeProxy *TargetLandscape = nullptr;
  for (TActorIterator<ALandscapeProxy> It(World); It; ++It) {
    ALandscapeProxy *Landscape = *It;
    if (Landscape && (LandscapeName.IsEmpty() || 
        Landscape->GetActorLabel().Equals(LandscapeName, ESearchCase::IgnoreCase))) {
      TargetLandscape = Landscape;
      break;
    }
  }

  if (!TargetLandscape) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("No landscape found in level"),
                        TEXT("LANDSCAPE_NOT_FOUND"));
    return true;
  }

  // Get landscape info
  ULandscapeInfo *LandscapeInfo = TargetLandscape->GetLandscapeInfo();
  
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("landscapeName"), TargetLandscape->GetActorLabel());
  Resp->SetStringField(TEXT("landscapePath"), TargetLandscape->GetPathName());
  Resp->SetNumberField(TEXT("componentCount"), TargetLandscape->LandscapeComponents.Num());
  
  // Get extent
  if (LandscapeInfo) {
    int32 MinX = MAX_int32, MinY = MAX_int32, MaxX = MIN_int32, MaxY = MIN_int32;
    LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY);
    
    TSharedPtr<FJsonObject> ExtentObj = MakeShared<FJsonObject>();
    ExtentObj->SetNumberField(TEXT("minX"), MinX);
    ExtentObj->SetNumberField(TEXT("minY"), MinY);
    ExtentObj->SetNumberField(TEXT("maxX"), MaxX);
    ExtentObj->SetNumberField(TEXT("maxY"), MaxY);
    ExtentObj->SetNumberField(TEXT("width"), MaxX - MinX + 1);
    ExtentObj->SetNumberField(TEXT("height"), MaxY - MinY + 1);
    Resp->SetObjectField(TEXT("extent"), ExtentObj);
  }

  // Transform info
  FVector Location = TargetLandscape->GetActorLocation();
  FVector Scale = TargetLandscape->GetActorScale3D();
  
  TSharedPtr<FJsonObject> LocationObj = MakeShared<FJsonObject>();
  LocationObj->SetNumberField(TEXT("x"), Location.X);
  LocationObj->SetNumberField(TEXT("y"), Location.Y);
  LocationObj->SetNumberField(TEXT("z"), Location.Z);
  Resp->SetObjectField(TEXT("location"), LocationObj);

  TSharedPtr<FJsonObject> ScaleObj = MakeShared<FJsonObject>();
  ScaleObj->SetNumberField(TEXT("x"), Scale.X);
  ScaleObj->SetNumberField(TEXT("y"), Scale.Y);
  ScaleObj->SetNumberField(TEXT("z"), Scale.Z);
  Resp->SetObjectField(TEXT("scale"), ScaleObj);

  // Material info
  if (TargetLandscape->LandscapeMaterial) {
    Resp->SetStringField(TEXT("materialPath"), TargetLandscape->LandscapeMaterial->GetPathName());
  }

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Landscape info retrieved"), Resp, FString());
  return true;
#else
  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("get_landscape_info requires editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGetTerrainHeightAt(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("get_terrain_height_at"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("get_terrain_height_at payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Get query location
  double X = 0.0, Y = 0.0;
  const TSharedPtr<FJsonObject>* LocObj = nullptr;
  if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj) {
    (*LocObj)->TryGetNumberField(TEXT("x"), X);
    (*LocObj)->TryGetNumberField(TEXT("y"), Y);
  } else {
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);
  }

  FString LandscapeName;
  Payload->TryGetStringField(TEXT("landscapeName"), LandscapeName);

  if (!GEditor) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UWorld *World = GetActiveWorld();
  if (!World) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("No world available"),
                        TEXT("WORLD_NOT_AVAILABLE"));
    return true;
  }

  // Find landscape
  ALandscapeProxy *TargetLandscape = nullptr;
  for (TActorIterator<ALandscapeProxy> It(World); It; ++It) {
    ALandscapeProxy *Landscape = *It;
    if (Landscape && (LandscapeName.IsEmpty() || 
        Landscape->GetActorLabel().Equals(LandscapeName, ESearchCase::IgnoreCase))) {
      TargetLandscape = Landscape;
      break;
    }
  }

  if (!TargetLandscape) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("No landscape found in level"),
                        TEXT("LANDSCAPE_NOT_FOUND"));
    return true;
  }

  ULandscapeInfo *LandscapeInfo = TargetLandscape->GetLandscapeInfo();
  if (!LandscapeInfo) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to get landscape info"),
                        TEXT("LANDSCAPE_INFO_MISSING"));
    return true;
  }

  // Convert world coordinates to landscape local coordinates
  FVector WorldLocation(X, Y, 0);
  FVector LocalPos = TargetLandscape->GetActorTransform().InverseTransformPosition(WorldLocation);
  
  int32 QueryX = FMath::RoundToInt(LocalPos.X);
  int32 QueryY = FMath::RoundToInt(LocalPos.Y);

  // Get landscape bounds
  int32 MinX, MinY, MaxX, MaxY;
  if (!LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY)) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to get landscape extent"),
                        TEXT("LANDSCAPE_INFO_MISSING"));
    return true;
  }

  // Clamp to bounds
  QueryX = FMath::Clamp(QueryX, MinX, MaxX);
  QueryY = FMath::Clamp(QueryY, MinY, MaxY);

  // Read single height value
  TArray<uint16> HeightData;
  HeightData.SetNum(1);

  FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
  LandscapeEdit.GetHeightData(QueryX, QueryY, QueryX, QueryY, HeightData.GetData(), 0);

  // Convert uint16 height to world Z
  uint16 RawHeight = HeightData[0];
  float HeightInUnits = (static_cast<float>(RawHeight) - 32768.0f) / 128.0f;
  float WorldZ = TargetLandscape->GetActorLocation().Z + 
                 HeightInUnits * TargetLandscape->GetActorScale3D().Z;

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("height"), WorldZ);
  Resp->SetNumberField(TEXT("rawHeight"), static_cast<double>(RawHeight));
  
  TSharedPtr<FJsonObject> QueryLocObj = MakeShared<FJsonObject>();
  QueryLocObj->SetNumberField(TEXT("x"), X);
  QueryLocObj->SetNumberField(TEXT("y"), Y);
  QueryLocObj->SetNumberField(TEXT("z"), WorldZ);
  Resp->SetObjectField(TEXT("location"), QueryLocObj);
  
  Resp->SetStringField(TEXT("landscapeName"), TargetLandscape->GetActorLabel());

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Terrain height retrieved"), Resp, FString());
  return true;
#else
  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("get_terrain_height_at requires editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
