#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Domains/Foliage/McpFoliagePrivate.h"

bool UEngineWireSubsystem::HandleAddFoliageType(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  FString Name;
  if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("name required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString MeshPath;
  if (!Payload->TryGetStringField(TEXT("meshPath"), MeshPath) || MeshPath.IsEmpty()) {
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

  // An object or package path, else one relative to /Game ("Meshes/Rock" or "Meshes/Rock.Rock").
  UStaticMesh *StaticMesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
  if (!StaticMesh && !MeshPath.StartsWith(TEXT("/"))) {
    StaticMesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/%s"), *MeshPath));
    if (!StaticMesh) {
      StaticMesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/%s.%s"), *MeshPath, *FPaths::GetBaseFilename(MeshPath)));
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

  // `path` is published on build_environment.create_foliage_type, but this
  // handler hardcoded /Game/Foliage and never read it — so the asset was
  // reported as created (success) at a location the caller never asked for.
  // Sanitized because it becomes a package path.
  FString PackagePath;
  if (!Payload->TryGetStringField(TEXT("path"), PackagePath) ||
      PackagePath.IsEmpty()) {
    Payload->TryGetStringField(TEXT("savePath"), PackagePath);
  }
  const FString RequestedPath = PackagePath;
  PackagePath = PackagePath.IsEmpty()
                    ? FString(TEXT("/Game/Foliage"))
                    : SanitizeProjectRelativePath(PackagePath);
  if (PackagePath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        McpPathRefusalMessage(TEXT("path"), RequestedPath),
                        TEXT("INVALID_PATH"));
    return true;
  }
  PackagePath.RemoveFromEnd(TEXT("/"));
  FString AssetName = Name;
  FString FullPackagePath =
      FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);

  // A foliage type already there is updated; another kind of asset under the name is refused (NewObject over it
  // brought the editor down).
  UFoliageType_InstancedStaticMesh *FoliageType = nullptr;
  if (McpAssetExists(FullPackagePath)) {
    FoliageType = Cast<UFoliageType_InstancedStaticMesh>(McpLoadAsset(FullPackagePath));
    if (!FoliageType) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("%s already holds an asset that is not a static mesh foliage type; pick another name. Nothing was changed."), *FullPackagePath),
                          TEXT("ASSET_ALREADY_EXISTS"));
      return true;
    }
    FoliageType->Modify();
  }
  const bool bCreated = FoliageType == nullptr;
  if (!FoliageType) {
    UPackage *Package = CreatePackage(*FullPackagePath);
    FoliageType = Package ? NewObject<UFoliageType_InstancedStaticMesh>(
        Package, FName(*AssetName), RF_Public | RF_Standalone) : nullptr;
  }
  if (!FoliageType) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create foliage type"),
                        TEXT("CREATION_FAILED"));
    return true;
  }

  FoliageType->SetStaticMesh(StaticMesh);
  if (bCreated || Payload->HasField(TEXT("density"))) {
    FoliageType->Density = static_cast<float>(Density);
  }
  if (bCreated || Payload->HasField(TEXT("minScale")) || Payload->HasField(TEXT("maxScale"))) {
    FoliageType->Scaling = EFoliageScaling::Uniform;
    // Uniform scaling reads ScaleX only.
    FoliageType->ScaleX.Min = static_cast<float>(MinScale);
    FoliageType->ScaleX.Max = static_cast<float>(MaxScale);
  }
  if (bCreated || Payload->HasField(TEXT("alignToNormal"))) {
    FoliageType->AlignToNormal = AlignToNormal;
  }
  if (bCreated || Payload->HasField(TEXT("randomYaw"))) {
    FoliageType->RandomYaw = RandomYaw;
  }
  if (CullDistance > 0) {
    FoliageType->CullDistance.Min = 0;
    FoliageType->CullDistance.Max = CullDistance;
  }
  FoliageType->ReapplyDensity = true;

  const bool bSaved = McpSafeAssetSave(FoliageType);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("created"), bCreated);
  Resp->SetBoolField(TEXT("saved"), bSaved);
  Resp->SetBoolField(TEXT("exists_after"), true);
  Resp->SetStringField(TEXT("asset_path"), FoliageType->GetPathName());
  Resp->SetStringField(TEXT("used_mesh"), MeshPath);

  McpHandlerUtils::AddVerification(Resp, FoliageType);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         bCreated ? TEXT("Foliage type created") : TEXT("Foliage type updated; settings not sent were kept"), Resp,
                         FString());

  return true;
}
