#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_FoliageHandlersPrivate.h"

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

  int32 CullDistance = 0;
  Payload->TryGetNumberField(TEXT("cullDistance"), CullDistance);

  UStaticMesh *StaticMesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
  if (!StaticMesh) {
    if (FPackageName::IsValidLongPackageName(MeshPath)) {
      StaticMesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
    }

    if (!StaticMesh) {
      if (!MeshPath.StartsWith(TEXT("/"))) {
        FString GamePath = FString::Printf(TEXT("/Game/%s"), *MeshPath);
        StaticMesh = LoadObject<UStaticMesh>(nullptr, *GamePath);
        if (!StaticMesh) {
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
  if (CullDistance > 0) {
    FoliageType->CullDistance.Min = 0;
    FoliageType->CullDistance.Max = CullDistance;
  }
  FoliageType->ReapplyDensity = true;

  McpSafeAssetSave(FoliageType);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("created"), true);
  Resp->SetBoolField(TEXT("exists_after"), true);
  Resp->SetStringField(TEXT("asset_path"), FoliageType->GetPathName());
  Resp->SetStringField(TEXT("used_mesh"), MeshPath);
  Resp->SetStringField(TEXT("method"), TEXT("native_asset_creation"));

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
