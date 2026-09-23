#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Foliage/McpAutomationBridge_FoliageHandlersPrivate.h"

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
    Payload->TryGetStringField(TEXT("foliageType"), FoliageTypePath);
  }
  if (FoliageTypePath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("foliageTypePath (or foliageType) required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString SafePath = SanitizeProjectRelativePath(FoliageTypePath);
  if (SafePath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Invalid or unsafe foliage type path: %s"), *FoliageTypePath),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }
  FoliageTypePath = SafePath;

  if (!FoliageTypePath.IsEmpty() &&
      FPaths::GetPath(FoliageTypePath).IsEmpty()) {
    FoliageTypePath =
        FString::Printf(TEXT("/Game/Foliage/%s"), *FoliageTypePath);
  }

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

  // A box area is the brush for strips and fields: a caller used to have to
  // generate every point itself (hundreds, for one strip of grass).
  FVector AreaMin = FVector::ZeroVector, AreaMax = FVector::ZeroVector;
  const TSharedPtr<FJsonObject> *AreaObj = nullptr;
  const bool bHasArea = Payload->TryGetObjectField(TEXT("area"), AreaObj) && AreaObj &&
      (*AreaObj)->HasField(TEXT("min")) && (*AreaObj)->HasField(TEXT("max"));
  if (bHasArea) {
    ReadVectorField(*AreaObj, TEXT("min"), AreaMin, FVector::ZeroVector);
    ReadVectorField(*AreaObj, TEXT("max"), AreaMax, FVector::ZeroVector);
  }
  if (Locations.Num() == 0 && !bHasArea) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("area {min, max}, locations array or position required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  double PaintDensity = 1.0;
  Payload->TryGetNumberField(TEXT("density"), PaintDensity);
  PaintDensity = FMath::Clamp(PaintDensity, 0.0, 1.0);
  double CountValue = 0.0;
  Payload->TryGetNumberField(TEXT("count"), CountValue);
  // count overrides density; otherwise one instance per ~(300uu)^2 at full
  // density. Capped so a huge brush cannot spawn an unbounded number.
  auto TargetFor = [&](double SurfaceArea) {
    return FMath::Clamp(CountValue > 0.0 ? FMath::RoundToInt(CountValue)
                                         : FMath::RoundToInt(SurfaceArea / (300.0 * 300.0) * PaintDensity), 1, 2000);
  };
  FRandomStream Stream(GetTypeHash(RequestId));
  double BrushRadius = 0.0;
  Payload->TryGetNumberField(TEXT("radius"), BrushRadius);
  if (bHasArea) {
    const FBox2D Box(FVector2D(FMath::Min(AreaMin.X, AreaMax.X), FMath::Min(AreaMin.Y, AreaMax.Y)),
                     FVector2D(FMath::Max(AreaMin.X, AreaMax.X), FMath::Max(AreaMin.Y, AreaMax.Y)));
    const double TopZ = FMath::Max(AreaMin.Z, AreaMax.Z);
    Locations.Reset();
    for (int32 Index = TargetFor(Box.GetArea()); Index > 0; --Index) {
      Locations.Add(FVector(Stream.FRandRange(Box.Min.X, Box.Max.X), Stream.FRandRange(Box.Min.Y, Box.Max.Y), TopZ));
    }
  } else if (BrushRadius > 0.0) {
    // `radius` and `density` were once never read: every supplied point placed
    // exactly one instance whatever the brush. Expand each point into a disc.
    const int32 Target = TargetFor(PI * BrushRadius * BrushRadius);
    TArray<FVector> BrushLocations;
    BrushLocations.Reserve(Locations.Num() * Target);
    for (const FVector &Center : Locations) {
      for (int32 Index = 0; Index < Target; ++Index) {
        // sqrt on the radial term keeps the points uniform over the disc.
        const double Angle = Stream.FRandRange(0.0, 2.0 * PI);
        const double Dist = BrushRadius * FMath::Sqrt(Stream.FRand());
        BrushLocations.Add(Center + FVector(Dist * FMath::Cos(Angle),
                                            Dist * FMath::Sin(Angle), 0.0));
      }
    }
    Locations = MoveTemp(BrushLocations);
  }

  if (!GEditor || !GEditor->GetEditorWorldContext().World()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor world not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UWorld *World = GEditor->GetEditorWorldContext().World();
  UFoliageType *FoliageType = nullptr;
  if (UEditorAssetLibrary::DoesAssetExist(FoliageTypePath)) {
    FoliageType = LoadObject<UFoliageType>(nullptr, *FoliageTypePath);
  }

  if (!FoliageType) {
    UStaticMesh *StaticMesh = LoadObject<UStaticMesh>(nullptr, *FoliageTypePath);
    if (StaticMesh) {
      FString BaseName = FPaths::GetBaseFilename(FoliageTypePath);
      FString AutoFTPath = FString::Printf(TEXT("/Game/Foliage/Auto_%s"), *BaseName);

      if (UEditorAssetLibrary::DoesAssetExist(AutoFTPath)) {
        FoliageType = LoadObject<UFoliageType>(nullptr, *AutoFTPath);
        if (FoliageType) {
          FoliageTypePath = AutoFTPath;
        }
      } else {
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
      McpFoliageHandlers::GetOrCreateFoliageActorForWorldSafe(World, true);
  if (!IFA) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to get foliage actor"),
                        TEXT("FOLIAGE_ACTOR_FAILED"));
    return true;
  }

  // Each point drops onto the first static surface below it, as the editor's
  // own brush does; placing at the caller's z left instances floating or buried
  // wherever the guess was off. No surface below (a pit, off the level) skips it.
  const bool bSnap = McpHandlerUtils::GetOptionalBool(Payload, TEXT("snapToSurface"), true);
  const bool bRandomYaw = McpHandlerUtils::GetOptionalBool(Payload, TEXT("randomYaw"), false);
  const bool bAlign = McpHandlerUtils::GetOptionalBool(Payload, TEXT("alignToNormal"), false);
  double MinScale = 1.0, MaxScale = 1.0;
  Payload->TryGetNumberField(TEXT("minScale"), MinScale);
  Payload->TryGetNumberField(TEXT("maxScale"), MaxScale);
  const double BottomZ = FMath::Min(AreaMin.Z, AreaMax.Z);
  const FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(McpPaintFoliage), true);
  int32 SkippedNoSurface = 0;
  TArray<FVector> PlacedLocations;
  for (const FVector &Point : Locations) {
    FFoliageInstance Instance;
    Instance.Location = Point;
    FVector Normal = FVector::UpVector;
    if (bSnap) {
      FHitResult Hit;
      const FVector End(Point.X, Point.Y, (bHasArea ? BottomZ : Point.Z) - 1000.0);
      if (!World->LineTraceSingleByObjectType(Hit, Point + FVector(0.0, 0.0, 500.0), End,
                                              FCollisionObjectQueryParams(ECC_WorldStatic), TraceParams)) {
        ++SkippedNoSurface;
        continue;
      }
      Instance.Location = Hit.ImpactPoint;
      Normal = Hit.ImpactNormal;
    }
    Instance.Rotation = bAlign ? FRotationMatrix::MakeFromZ(Normal).Rotator() : FRotator::ZeroRotator;
    if (bRandomYaw) {
      Instance.Rotation.Yaw = Stream.FRandRange(0.0, 360.0);
    }
    Instance.DrawScale3D = FVector3f(Stream.FRandRange(FMath::Min(MinScale, MaxScale), FMath::Max(MinScale, MaxScale)));
    Instance.ZOffset = 0.0f;

    if (FFoliageInfo *Info = IFA->FindInfo(FoliageType)) {
      Info->AddInstance(FoliageType, Instance, nullptr);
    } else {
      IFA->AddFoliageType(FoliageType);
      if (FFoliageInfo *NewInfo = IFA->FindInfo(FoliageType)) {
        NewInfo->AddInstance(FoliageType, Instance, nullptr);
      }
    }
    PlacedLocations.Add(Instance.Location);
  }
  if (PlacedLocations.Num() == 0) {
    SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("No static surface below any of the %d points (searched 500 above each point's z to 1000 below the lowest). Raise area.max.z / the location z above the ground, or pass snapToSurface:false to place at the given z."),
                        Locations.Num()),
        TEXT("NO_SURFACE"));
    return true;
  }

  IFA->Modify();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("foliageTypePath"), FoliageTypePath);
  Resp->SetNumberField(TEXT("instancesPlaced"), PlacedLocations.Num());
  Resp->SetNumberField(TEXT("brushRadius"), BrushRadius);
  Resp->SetNumberField(TEXT("requestedPoints"), Locations.Num());
  Resp->SetBoolField(TEXT("snappedToSurface"), bSnap);
  Resp->SetNumberField(TEXT("skippedNoSurface"), SkippedNoSurface);
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
