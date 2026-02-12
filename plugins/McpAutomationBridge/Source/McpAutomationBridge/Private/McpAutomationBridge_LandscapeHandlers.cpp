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

    FScopedSlowTask SlowTask(2.0f,
                             FText::FromString(TEXT("Modifying heightmap...")));
    SlowTask.MakeDialog();

    int32 MinX, MinY, MaxX, MaxY;
    if (!LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY)) {
      Subsystem->SendAutomationError(RequestingSocket, RequestId,
                                     TEXT("Failed to get landscape extent"),
                                     TEXT("INVALID_LANDSCAPE"));
      return;
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
    if (!Landscape && !LandscapeName.IsEmpty() && GEditor) {
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
      Subsystem->SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Layer '%s' not found. Create layer first using "
                               "landscape editor."),
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
