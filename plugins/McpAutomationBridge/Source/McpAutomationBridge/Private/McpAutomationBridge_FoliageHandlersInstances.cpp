#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_FoliageHandlersPrivate.h"

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

      const TSharedPtr<FJsonObject> *LocObj = nullptr;
      if ((*TObj)->TryGetObjectField(TEXT("location"), LocObj) && LocObj) {
        (*LocObj)->TryGetNumberField(TEXT("x"), TransformData.Location.X);
        (*LocObj)->TryGetNumberField(TEXT("y"), TransformData.Location.Y);
        (*LocObj)->TryGetNumberField(TEXT("z"), TransformData.Location.Z);
      } else {
        const TArray<TSharedPtr<FJsonValue>> *LocArr = nullptr;
        if ((*TObj)->TryGetArrayField(TEXT("location"), LocArr) && LocArr &&
            LocArr->Num() >= 3) {
          TransformData.Location.X = (*LocArr)[0]->AsNumber();
          TransformData.Location.Y = (*LocArr)[1]->AsNumber();
          TransformData.Location.Z = (*LocArr)[2]->AsNumber();
        } else {
          continue;
        }
      }

      const TSharedPtr<FJsonObject> *RotObj = nullptr;
      if ((*TObj)->TryGetObjectField(TEXT("rotation"), RotObj) && RotObj) {
        double Pitch = 0, Yaw = 0, Roll = 0;
        (*RotObj)->TryGetNumberField(TEXT("pitch"), Pitch);
        (*RotObj)->TryGetNumberField(TEXT("yaw"), Yaw);
        (*RotObj)->TryGetNumberField(TEXT("roll"), Roll);
        TransformData.Rotation = FRotator(Pitch, Yaw, Roll);
      } else {
        const TArray<TSharedPtr<FJsonValue>> *RotArr = nullptr;
        if ((*TObj)->TryGetArrayField(TEXT("rotation"), RotArr) && RotArr &&
            RotArr->Num() >= 3) {
          TransformData.Rotation.Pitch = (*RotArr)[0]->AsNumber();
          TransformData.Rotation.Yaw = (*RotArr)[1]->AsNumber();
          TransformData.Rotation.Roll = (*RotArr)[2]->AsNumber();
        }
      }

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

  if (ParsedTransforms.Num() == 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("transforms or locations must contain at least one valid location"),
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

  UFoliageType *FoliageType = Cast<UFoliageType>(
      StaticLoadObject(UFoliageType::StaticClass(), nullptr, *FoliageTypePath,
                       nullptr, LOAD_NoWarn));

  if (!FoliageType) {
    UStaticMesh *StaticMesh = LoadObject<UStaticMesh>(nullptr, *FoliageTypePath);
    if (StaticMesh) {
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
