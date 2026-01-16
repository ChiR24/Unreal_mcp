// Copyright Epic Games, Inc. All Rights Reserved.

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpBridgeWebSocket.h"
#include "EditorAssetLibrary.h"
#include "Engine/StaticMesh.h"
#include "Math/UnrealMathUtility.h"

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

#if MCP_UE56_PLUS
  if (Mesh->GetNaniteSettings().bEnabled != bEnable) {
    Mesh->Modify();
    FMeshNaniteSettings Settings = Mesh->GetNaniteSettings();
    Settings.bEnabled = bEnable;
    Mesh->SetNaniteSettings(Settings);
    Mesh->PostEditChange();
#else
  if (Mesh->NaniteSettings.bEnabled != bEnable) {
    Mesh->Modify();
    Mesh->NaniteSettings.bEnabled = bEnable;
    Mesh->PostEditChange();
#endif

    
    // Save the asset
    if (!McpSafeAssetSave(Mesh)) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to save asset"), TEXT("SAVE_FAILED"));
        return true;
    }
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
#if MCP_UE56_PLUS
  Resp->SetBoolField(TEXT("enabled"), Mesh->GetNaniteSettings().bEnabled);
#else
  Resp->SetBoolField(TEXT("enabled"), Mesh->NaniteSettings.bEnabled);
#endif

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

#if MCP_UE56_PLUS
  FMeshNaniteSettings Settings = Mesh->GetNaniteSettings();
#else
  FMeshNaniteSettings& Settings = Mesh->NaniteSettings;
#endif

  int32 PositionPrecision = 0;
  if (Payload->TryGetNumberField(TEXT("positionPrecision"), PositionPrecision)) {
      if (Settings.PositionPrecision != PositionPrecision) {
          Settings.PositionPrecision = PositionPrecision;
          bChanged = true;
      }
  }

  double PercentTriangles;
  if (Payload->TryGetNumberField(TEXT("percentTriangles"), PercentTriangles)) {
      // Clamp 0-1
      float Val = FMath::Clamp((float)PercentTriangles, 0.0f, 1.0f);
      if (Settings.KeepPercentTriangles != Val) {
          Settings.KeepPercentTriangles = Val;
          bChanged = true;
      }
  }

  double FallbackError;
  if (Payload->TryGetNumberField(TEXT("fallbackRelativeError"), FallbackError)) {
      if (Settings.FallbackRelativeError != (float)FallbackError) {
          Settings.FallbackRelativeError = (float)FallbackError;
          bChanged = true;
      }
  }

  if (bChanged) {
#if MCP_UE56_PLUS
      Mesh->SetNaniteSettings(Settings);
#endif
      Mesh->PostEditChange();

      if (!McpSafeAssetSave(Mesh)) {
          SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to save asset"), TEXT("SAVE_FAILED"));
          return true;
      }
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
#if MCP_UE56_PLUS
  Resp->SetNumberField(TEXT("positionPrecision"), Mesh->GetNaniteSettings().PositionPrecision);
  Resp->SetNumberField(TEXT("percentTriangles"), Mesh->GetNaniteSettings().KeepPercentTriangles);
#else
  Resp->SetNumberField(TEXT("positionPrecision"), Mesh->NaniteSettings.PositionPrecision);
  Resp->SetNumberField(TEXT("percentTriangles"), Mesh->NaniteSettings.KeepPercentTriangles);
#endif

  
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
      if (Mesh) {
#if MCP_UE56_PLUS
          if (Mesh->GetNaniteSettings().bEnabled != bEnable) {
              Mesh->Modify();
              FMeshNaniteSettings Settings = Mesh->GetNaniteSettings();
              Settings.bEnabled = bEnable;
              Mesh->SetNaniteSettings(Settings);
              Mesh->PostEditChange();
              McpSafeAssetSave(Mesh);
              UpdatedCount++;
          }
#else
          if (Mesh->NaniteSettings.bEnabled != bEnable) {
              Mesh->Modify();
              Mesh->NaniteSettings.bEnabled = bEnable;
              Mesh->PostEditChange();
              McpSafeAssetSave(Mesh);
              UpdatedCount++;
          }
#endif
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

// ============================================================================
// ASSET WORKFLOW HANDLERS (Source Control, Bulk Operations, etc.)
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleAssetAction(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  // This is a catch-all for asset-related actions that may be routed here
  // Most asset actions are now handled by specific handlers
  SendAutomationError(RequestingSocket, RequestId,
                      FString::Printf(TEXT("Asset action '%s' not implemented in native bridge"), *Action),
                      TEXT("NOT_IMPLEMENTED"));
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSourceControlCheckout(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  FString AssetPath;
  if (!Payload.IsValid() || !Payload->TryGetStringField(TEXT("assetPath"), AssetPath)) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("assetPath required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Source control checkout - requires source control to be enabled
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Source control checkout not yet implemented in native bridge"),
                      TEXT("NOT_IMPLEMENTED"));
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSourceControlSubmit(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Source control submit not yet implemented in native bridge"),
                      TEXT("NOT_IMPLEMENTED"));
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGetSourceControlState(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Get source control state not yet implemented in native bridge"),
                      TEXT("NOT_IMPLEMENTED"));
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleFixupRedirectors(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Fixup redirectors not yet implemented in native bridge"),
                      TEXT("NOT_IMPLEMENTED"));
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleBulkRenameAssets(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Bulk rename assets not yet implemented in native bridge"),
                      TEXT("NOT_IMPLEMENTED"));
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleBulkDeleteAssets(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Bulk delete assets not yet implemented in native bridge"),
                      TEXT("NOT_IMPLEMENTED"));
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGenerateThumbnail(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Generate thumbnail not yet implemented in native bridge"),
                      TEXT("NOT_IMPLEMENTED"));
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleRebuildMaterial(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Rebuild material not yet implemented in native bridge"),
                      TEXT("NOT_IMPLEMENTED"));
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGenerateLODs(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Generate LODs not yet implemented in native bridge"),
                      TEXT("NOT_IMPLEMENTED"));
  return true;
#else
  return false;
#endif
}
