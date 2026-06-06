#include "McpVersionCompatibility.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "LandscapeGrassType.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "McpSafeOperations.h"

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
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("meshPath or staticMesh required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
  }

  FString SafeMeshPath = SanitizeProjectRelativePath(MeshPath);
  if (SafeMeshPath.IsEmpty()) {
    SendAutomationError(
        RequestingSocket, RequestId,
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
  AsyncTask(ENamedThreads::GameThread,
            [WeakSubsystem, RequestId, RequestingSocket, Name, MeshPath,
             Density, MinScale, MaxScale]() {
              UMcpAutomationBridgeSubsystem *Subsystem = WeakSubsystem.Get();
              if (!Subsystem)
                return;

              UStaticMesh *StaticMesh = Cast<UStaticMesh>(StaticLoadObject(
                  UStaticMesh::StaticClass(), nullptr, *MeshPath, nullptr,
                  LOAD_NoWarn));
              if (!StaticMesh) {
                Subsystem->SendAutomationError(
                    RequestingSocket, RequestId,
                    FString::Printf(TEXT("Static mesh not found: %s"),
                                    *MeshPath),
                    TEXT("ASSET_NOT_FOUND"));
                return;
              }

              FString PackagePath = TEXT("/Game/Landscape");
              FString AssetName = Name;
              FString FullPackagePath =
                  FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);
              if (UObject *ExistingAsset = StaticLoadObject(
                      ULandscapeGrassType::StaticClass(), nullptr,
                      *FullPackagePath)) {
                TSharedPtr<FJsonObject> Resp =
                    McpHandlerUtils::CreateResultObject();
                Resp->SetBoolField(TEXT("success"), true);
                Resp->SetStringField(TEXT("asset_path"),
                                     ExistingAsset->GetPathName());
                Resp->SetStringField(TEXT("message"),
                                     TEXT("Asset already exists"));
                Subsystem->SendAutomationResponse(
                    RequestingSocket, RequestId, true,
                    TEXT("Landscape grass type already exists"), Resp,
                    FString());
                return;
              }

              UPackage *Package = CreatePackage(*FullPackagePath);
              ULandscapeGrassType *GrassType =
                  NewObject<ULandscapeGrassType>(
                      Package, FName(*AssetName),
                      RF_Public | RF_Standalone);
              if (!GrassType) {
                Subsystem->SendAutomationError(
                    RequestingSocket, RequestId,
                    TEXT("Failed to create grass type asset"),
                    TEXT("CREATION_FAILED"));
                return;
              }

              int32 NewIndex = GrassType->GrassVarieties.AddZeroed();
              FGrassVariety &Variety = GrassType->GrassVarieties[NewIndex];
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
              TSharedPtr<FJsonObject> Resp =
                  McpHandlerUtils::CreateResultObject();
              Resp->SetBoolField(TEXT("success"), true);
              Resp->SetStringField(TEXT("asset_path"),
                                   GrassType->GetPathName());
              Subsystem->SendAutomationResponse(
                  RequestingSocket, RequestId, true,
                  TEXT("Landscape grass type created"), Resp, FString());
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
