#include "Core/Compatibility/McpVersionCompatibility.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "LandscapeGrassType.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Safety/McpSafeOperations.h"

bool UEngineWireSubsystem::HandleCreateLandscapeGrassType(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  FString Name;
  if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("name required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString MeshPath = McpGetFirstStringField(Payload, {TEXT("meshPath"), TEXT("staticMesh")});
  if (MeshPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("meshPath or staticMesh required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }
  const FString RequestedPath = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game/Landscape"));
  const FString SafeMeshPath = SanitizeProjectRelativePath(MeshPath);
  const FString PackagePath = SanitizeProjectRelativePath(RequestedPath);
  if (SafeMeshPath.IsEmpty() || PackagePath.IsEmpty()) {
    SendAutomationError(
        RequestingSocket, RequestId,
        SafeMeshPath.IsEmpty() ? McpPathRefusalMessage(TEXT("meshPath"), MeshPath)
                               : McpPathRefusalMessage(TEXT("path"), RequestedPath),
        TEXT("SECURITY_VIOLATION"));
    return true;
  }
  MeshPath = SafeMeshPath;

  UStaticMesh *StaticMesh = Cast<UStaticMesh>(StaticLoadObject(
      UStaticMesh::StaticClass(), nullptr, *MeshPath, nullptr,
      LOAD_NoWarn));
  if (!StaticMesh) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Static mesh not found: %s"),
                        *MeshPath),
        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  const FString FullPackagePath = PackagePath / Name;
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  if (ULandscapeGrassType *Existing = Cast<ULandscapeGrassType>(McpLoadAsset(FullPackagePath))) {
    Resp->SetStringField(TEXT("asset_path"), Existing->GetPathName());
    Resp->SetBoolField(TEXT("alreadyExists"), true);
    McpHandlerUtils::MarkNoAssetsChanged(Resp);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Landscape grass type already exists; nothing was changed"), Resp, FString());
    return true;
  }

  // Another kind of asset at the path was rebuilt in place, or crashed the editor.
  FString PackageError;
  FString PackageCode;
  UPackage *Package = McpCreatePackageForNewAsset(FullPackagePath, PackageError, PackageCode);
  if (!Package) {
    SendAutomationError(RequestingSocket, RequestId, PackageError, PackageCode);
    return true;
  }
  ULandscapeGrassType *GrassType =
      NewObject<ULandscapeGrassType>(
          Package, FName(*Name),
          RF_Public | RF_Standalone);
  if (!GrassType) {
    SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("Failed to create grass type asset"),
        TEXT("CREATION_FAILED"));
    return true;
  }

  int32 NewIndex = GrassType->GrassVarieties.AddZeroed();
  FGrassVariety &Variety = GrassType->GrassVarieties[NewIndex];
  Variety.GrassMesh = StaticMesh;
  Variety.GrassDensity.Default = 1.0f;
  Variety.ScaleX = Variety.ScaleY = Variety.ScaleZ = FFloatInterval(0.8f, 1.2f);
  Variety.RandomRotation = true;
  Variety.AlignToSurface = true;

  FAssetRegistryModule::AssetCreated(GrassType);
  GrassType->MarkPackageDirty();
  Resp->SetStringField(TEXT("asset_path"), GrassType->GetPathName());
  Resp->SetBoolField(TEXT("saved"), McpSafeAssetSave(GrassType));
  McpHandlerUtils::AddVerification(Resp, GrassType);
  SendAutomationResponse(
      RequestingSocket, RequestId, true,
      TEXT("Landscape grass type created"), Resp, FString());
  return true;
}
