// ============================================================================
// 8. NANITE HANDLERS
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleEnableNaniteMesh(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("enable_nanite_mesh payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("assetPath required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  bool bEnable = true;
  Payload->TryGetBoolField(TEXT("enableNanite"), bEnable);

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Asset not found"),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  UStaticMesh *Mesh = Cast<UStaticMesh>(Asset);
  if (!Mesh) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Asset is not a StaticMesh"),
                        TEXT("INVALID_ASSET_TYPE"));
    return true;
  }

  if (Mesh->NaniteSettings.bEnabled != bEnable) {
    Mesh->Modify();
    Mesh->NaniteSettings.bEnabled = bEnable;
    Mesh->PostEditChange();
    
    // Save the asset
    if (!McpSafeAssetSave(Mesh)) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to save asset"), TEXT("SAVE_FAILED"));
        return true;
    }
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("enabled"), Mesh->NaniteSettings.bEnabled);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         bEnable ? TEXT("Nanite enabled") : TEXT("Nanite disabled"),
                         Resp);
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Nanite actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSetNaniteSettings(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("set_nanite_settings payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("assetPath required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  UStaticMesh *Mesh = Cast<UStaticMesh>(Asset);
  if (!Mesh) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Asset is not a StaticMesh"),
                        TEXT("INVALID_ASSET_TYPE"));
    return true;
  }

  Mesh->Modify();
  bool bChanged = false;

  int32 PositionPrecision = 0;
  if (Payload->TryGetNumberField(TEXT("positionPrecision"), PositionPrecision)) {
      if (Mesh->NaniteSettings.PositionPrecision != PositionPrecision) {
          Mesh->NaniteSettings.PositionPrecision = PositionPrecision;
          bChanged = true;
      }
  }

  double PercentTriangles;
  if (Payload->TryGetNumberField(TEXT("percentTriangles"), PercentTriangles)) {
      // Clamp 0-1
      float Val = FMath::Clamp((float)PercentTriangles, 0.0f, 1.0f);
      if (Mesh->NaniteSettings.KeepPercentTriangles != Val) {
          Mesh->NaniteSettings.KeepPercentTriangles = Val;
          bChanged = true;
      }
  }

  double FallbackError;
  if (Payload->TryGetNumberField(TEXT("fallbackRelativeError"), FallbackError)) {
      if (Mesh->NaniteSettings.FallbackRelativeError != (float)FallbackError) {
          Mesh->NaniteSettings.FallbackRelativeError = (float)FallbackError;
          bChanged = true;
      }
  }

  if (bChanged) {
      Mesh->PostEditChange();
      if (!McpSafeAssetSave(Mesh)) {
          SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to save asset"), TEXT("SAVE_FAILED"));
          return true;
      }
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("positionPrecision"), Mesh->NaniteSettings.PositionPrecision);
  Resp->SetNumberField(TEXT("percentTriangles"), Mesh->NaniteSettings.KeepPercentTriangles);
  
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Nanite settings updated"), Resp);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleBatchNaniteConvert(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("batch_nanite_convert payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString Directory;
  if (!Payload->TryGetStringField(TEXT("directory"), Directory) || Directory.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("directory required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  bool bRecursive = true;
  Payload->TryGetBoolField(TEXT("recursive"), bRecursive);

  bool bEnable = true;
  Payload->TryGetBoolField(TEXT("enableNanite"), bEnable);

  // Normalize path
  if (Directory.StartsWith(TEXT("/Content"))) {
      Directory = FString::Printf(TEXT("/Game%s"), *Directory.RightChop(8));
  }

  // Find assets
  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  FARFilter Filter;
  Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("StaticMesh")));
  Filter.PackagePaths.Add(FName(*Directory));
  Filter.bRecursivePaths = bRecursive;

  TArray<FAssetData> AssetList;
  AssetRegistry.GetAssets(Filter, AssetList);

  int32 UpdatedCount = 0;
  for (const FAssetData &AssetData : AssetList) {
      UStaticMesh *Mesh = Cast<UStaticMesh>(AssetData.GetAsset());
      if (Mesh && Mesh->NaniteSettings.bEnabled != bEnable) {
          Mesh->Modify();
          Mesh->NaniteSettings.bEnabled = bEnable;
          Mesh->PostEditChange();
          McpSafeAssetSave(Mesh);
          UpdatedCount++;
      }
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("updatedCount"), UpdatedCount);
  Resp->SetNumberField(TEXT("totalFound"), AssetList.Num());

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         FString::Printf(TEXT("Updated %d meshes"), UpdatedCount),
                         Resp);
  return true;
#else
  return false;
#endif
}
