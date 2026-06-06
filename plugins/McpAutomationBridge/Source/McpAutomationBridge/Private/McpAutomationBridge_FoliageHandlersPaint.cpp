#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_FoliageHandlersPrivate.h"

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

  if (FoliageTypePath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("foliageTypePath (or foliageType) required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
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

  TArray<FVector> PlacedLocations;
  for (const FVector &Location : Locations) {
    FFoliageInstance Instance;
    Instance.Location = Location;
    Instance.Rotation = FRotator::ZeroRotator;
    Instance.DrawScale3D = FVector3f(1.0f);
    Instance.ZOffset = 0.0f;

    if (FFoliageInfo *Info = IFA->FindInfo(FoliageType)) {
      Info->AddInstance(FoliageType, Instance, nullptr);
    } else {
      IFA->AddFoliageType(FoliageType);
      if (FFoliageInfo *NewInfo = IFA->FindInfo(FoliageType)) {
        NewInfo->AddInstance(FoliageType, Instance, nullptr);
      }
    }
    PlacedLocations.Add(Location);
  }

  IFA->Modify();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("foliageTypePath"), FoliageTypePath);
  Resp->SetNumberField(TEXT("instancesPlaced"), PlacedLocations.Num());
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
