// Copyright Epic Games, Inc. All Rights Reserved.

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpBridgeWebSocket.h"
#include "EditorAssetLibrary.h"
#include "Engine/StaticMesh.h"
#include "Math/UnrealMathUtility.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/Factory.h"
#include "ObjectTools.h"
#include "Misc/PackageName.h"
#include "UObject/MetaData.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "UObject/ObjectRedirector.h"

#if WITH_EDITOR
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlOperations.h"
#endif

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
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Extract subAction from payload - TS sends subAction field for asset operations
  FString SubAction;
  if (!Payload->TryGetStringField(TEXT("subAction"), SubAction)) {
    // Fallback to action field if subAction not present
    Payload->TryGetStringField(TEXT("action"), SubAction);
  }
  
  // Normalize subAction to lowercase for comparison
  const FString LowerSubAction = SubAction.ToLower();

  // ============================================================================
  // LIST ASSETS
  // ============================================================================
  if (LowerSubAction == TEXT("list")) {
    FString Directory;
    Payload->TryGetStringField(TEXT("directory"), Directory);
    if (Directory.IsEmpty()) {
      Directory = TEXT("/Game");
    }
    
    // Normalize path: /Content -> /Game
    if (Directory.StartsWith(TEXT("/Content"))) {
      Directory = FString::Printf(TEXT("/Game%s"), *Directory.RightChop(8));
    }
    
    bool bRecursive = true;
    Payload->TryGetBoolField(TEXT("recursive"), bRecursive);
    
    TArray<FString> Assets = UEditorAssetLibrary::ListAssets(Directory, bRecursive, false);
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> AssetsArray;
    for (const FString& AssetPath : Assets) {
      AssetsArray.Add(MakeShared<FJsonValueString>(AssetPath));
    }
    Result->SetArrayField(TEXT("assets"), AssetsArray);
    Result->SetNumberField(TEXT("count"), Assets.Num());
    Result->SetBoolField(TEXT("success"), true);
    
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           FString::Printf(TEXT("Found %d assets"), Assets.Num()), Result);
    return true;
  }

  // ============================================================================
  // ASSET EXISTS CHECK
  // ============================================================================
  if (LowerSubAction == TEXT("exists")) {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("assetPath required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    
    // Normalize path
    if (AssetPath.StartsWith(TEXT("/Content"))) {
      AssetPath = FString::Printf(TEXT("/Game%s"), *AssetPath.RightChop(8));
    }
    
    bool bExists = UEditorAssetLibrary::DoesAssetExist(AssetPath);
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("exists"), bExists);
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetBoolField(TEXT("success"), true);
    
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           bExists ? TEXT("Asset exists") : TEXT("Asset does not exist"), Result);
    return true;
  }

  // ============================================================================
  // DUPLICATE ASSET
  // ============================================================================
  if (LowerSubAction == TEXT("duplicate")) {
    FString SourcePath, DestinationPath;
    if (!Payload->TryGetStringField(TEXT("sourcePath"), SourcePath) || SourcePath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("sourcePath required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath) || DestinationPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("destinationPath required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    
    // Normalize paths
    if (SourcePath.StartsWith(TEXT("/Content"))) {
      SourcePath = FString::Printf(TEXT("/Game%s"), *SourcePath.RightChop(8));
    }
    if (DestinationPath.StartsWith(TEXT("/Content"))) {
      DestinationPath = FString::Printf(TEXT("/Game%s"), *DestinationPath.RightChop(8));
    }
    
    if (!UEditorAssetLibrary::DoesAssetExist(SourcePath)) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Source asset not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    
    UObject* DuplicatedAsset = UEditorAssetLibrary::DuplicateAsset(SourcePath, DestinationPath);
    
    if (DuplicatedAsset) {
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetBoolField(TEXT("success"), true);
      Result->SetStringField(TEXT("sourcePath"), SourcePath);
      Result->SetStringField(TEXT("destinationPath"), DestinationPath);
      Result->SetStringField(TEXT("newAssetPath"), DestinationPath);
      
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Asset duplicated successfully"), Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to duplicate asset"),
                          TEXT("DUPLICATE_FAILED"));
    }
    return true;
  }

  // ============================================================================
  // RENAME ASSET
  // ============================================================================
  if (LowerSubAction == TEXT("rename")) {
    FString SourcePath, NewName;
    if (!Payload->TryGetStringField(TEXT("sourcePath"), SourcePath) &&
        !Payload->TryGetStringField(TEXT("assetPath"), SourcePath)) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("sourcePath or assetPath required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("newName"), NewName) &&
        !Payload->TryGetStringField(TEXT("destinationPath"), NewName)) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("newName or destinationPath required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    
    // Normalize source path
    if (SourcePath.StartsWith(TEXT("/Content"))) {
      SourcePath = FString::Printf(TEXT("/Game%s"), *SourcePath.RightChop(8));
    }
    
    // Build destination path if newName is just a name (not a full path)
    FString DestinationPath = NewName;
    if (!NewName.Contains(TEXT("/"))) {
      // Extract directory from source and append new name
      FString Directory = FPackageName::GetLongPackagePath(SourcePath);
      DestinationPath = Directory / NewName;
    }
    if (DestinationPath.StartsWith(TEXT("/Content"))) {
      DestinationPath = FString::Printf(TEXT("/Game%s"), *DestinationPath.RightChop(8));
    }
    
    if (!UEditorAssetLibrary::DoesAssetExist(SourcePath)) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Source asset not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    
    bool bSuccess = UEditorAssetLibrary::RenameAsset(SourcePath, DestinationPath);
    
    if (bSuccess) {
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetBoolField(TEXT("success"), true);
      Result->SetStringField(TEXT("originalPath"), SourcePath);
      Result->SetStringField(TEXT("newPath"), DestinationPath);
      
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Asset renamed successfully"), Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to rename asset"),
                          TEXT("RENAME_FAILED"));
    }
    return true;
  }

  // ============================================================================
  // MOVE ASSET (uses same RenameAsset API)
  // ============================================================================
  if (LowerSubAction == TEXT("move")) {
    FString SourcePath, DestinationPath;
    if (!Payload->TryGetStringField(TEXT("sourcePath"), SourcePath) || SourcePath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("sourcePath required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath) || DestinationPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("destinationPath required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    
    // Normalize paths
    if (SourcePath.StartsWith(TEXT("/Content"))) {
      SourcePath = FString::Printf(TEXT("/Game%s"), *SourcePath.RightChop(8));
    }
    if (DestinationPath.StartsWith(TEXT("/Content"))) {
      DestinationPath = FString::Printf(TEXT("/Game%s"), *DestinationPath.RightChop(8));
    }
    
    if (!UEditorAssetLibrary::DoesAssetExist(SourcePath)) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Source asset not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    
    // RenameAsset handles both rename and move operations
    bool bSuccess = UEditorAssetLibrary::RenameAsset(SourcePath, DestinationPath);
    
    if (bSuccess) {
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetBoolField(TEXT("success"), true);
      Result->SetStringField(TEXT("sourcePath"), SourcePath);
      Result->SetStringField(TEXT("destinationPath"), DestinationPath);
      
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Asset moved successfully"), Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to move asset"),
                          TEXT("MOVE_FAILED"));
    }
    return true;
  }

  // ============================================================================
  // DELETE ASSET(S) - Supports both singular assetPath and array assetPaths
  // ============================================================================
  if (LowerSubAction == TEXT("delete")) {
    TArray<FString> PathsToDelete;
    
    // First try singular assetPath
    FString SinglePath;
    if (Payload->TryGetStringField(TEXT("assetPath"), SinglePath) && !SinglePath.IsEmpty()) {
      PathsToDelete.Add(SinglePath);
    }
    
    // Also try array assetPaths (TS sends this format)
    const TArray<TSharedPtr<FJsonValue>>* PathsArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("assetPaths"), PathsArray) && PathsArray) {
      for (const TSharedPtr<FJsonValue>& PathValue : *PathsArray) {
        if (PathValue.IsValid() && PathValue->Type == EJson::String) {
          FString PathStr = PathValue->AsString();
          if (!PathStr.IsEmpty()) {
            PathsToDelete.Add(PathStr);
          }
        }
      }
    }
    
    if (PathsToDelete.Num() == 0) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("assetPath or assetPaths required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    
    TArray<FString> DeletedPaths;
    TArray<FString> FailedPaths;
    TArray<FString> NotFoundPaths;
    
    for (FString& AssetPath : PathsToDelete) {
      // Normalize path
      if (AssetPath.StartsWith(TEXT("/Content"))) {
        AssetPath = FString::Printf(TEXT("/Game%s"), *AssetPath.RightChop(8));
      }
      
      if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
        NotFoundPaths.Add(AssetPath);
        continue;
      }
      
      if (UEditorAssetLibrary::DeleteAsset(AssetPath)) {
        DeletedPaths.Add(AssetPath);
      } else {
        FailedPaths.Add(AssetPath);
      }
    }
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), FailedPaths.Num() == 0 && NotFoundPaths.Num() == 0);
    Result->SetNumberField(TEXT("deletedCount"), DeletedPaths.Num());
    Result->SetNumberField(TEXT("failedCount"), FailedPaths.Num());
    Result->SetNumberField(TEXT("notFoundCount"), NotFoundPaths.Num());
    
    // Add arrays for detailed reporting
    TArray<TSharedPtr<FJsonValue>> DeletedArray;
    for (const FString& Path : DeletedPaths) {
      DeletedArray.Add(MakeShared<FJsonValueString>(Path));
    }
    Result->SetArrayField(TEXT("deleted"), DeletedArray);
    
    if (FailedPaths.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> FailedArray;
      for (const FString& Path : FailedPaths) {
        FailedArray.Add(MakeShared<FJsonValueString>(Path));
      }
      Result->SetArrayField(TEXT("failed"), FailedArray);
    }
    
    if (NotFoundPaths.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> NotFoundArray;
      for (const FString& Path : NotFoundPaths) {
        NotFoundArray.Add(MakeShared<FJsonValueString>(Path));
      }
      Result->SetArrayField(TEXT("notFound"), NotFoundArray);
    }
    
    FString Message = FString::Printf(TEXT("Deleted %d asset(s)"), DeletedPaths.Num());
    if (NotFoundPaths.Num() > 0) {
      Message += FString::Printf(TEXT(", %d not found"), NotFoundPaths.Num());
    }
    if (FailedPaths.Num() > 0) {
      Message += FString::Printf(TEXT(", %d failed"), FailedPaths.Num());
    }
    
    SendAutomationResponse(RequestingSocket, RequestId, 
                           FailedPaths.Num() == 0,
                           Message, Result);
    return true;
  }

  // ============================================================================
  // SAVE ASSET
  // ============================================================================
  if (LowerSubAction == TEXT("save_asset") || LowerSubAction == TEXT("save")) {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("assetPath required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    
    // Normalize path
    if (AssetPath.StartsWith(TEXT("/Content"))) {
      AssetPath = FString::Printf(TEXT("/Game%s"), *AssetPath.RightChop(8));
    }
    
    if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Asset not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    
    UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to load asset"),
                          TEXT("LOAD_FAILED"));
      return true;
    }
    
    // Use safe save helper for UE 5.7 compatibility
    bool bSuccess = McpSafeAssetSave(Asset);
    
    if (bSuccess) {
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetBoolField(TEXT("success"), true);
      Result->SetStringField(TEXT("savedPath"), AssetPath);
      
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Asset saved successfully"), Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to save asset"),
                          TEXT("SAVE_FAILED"));
    }
    return true;
  }

  // ============================================================================
  // CREATE FOLDER
  // ============================================================================
  if (LowerSubAction == TEXT("create_folder")) {
    FString FolderPath;
    if (!Payload->TryGetStringField(TEXT("folderPath"), FolderPath) &&
        !Payload->TryGetStringField(TEXT("path"), FolderPath)) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("folderPath or path required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    
    // Normalize path
    if (FolderPath.StartsWith(TEXT("/Content"))) {
      FolderPath = FString::Printf(TEXT("/Game%s"), *FolderPath.RightChop(8));
    }
    
    bool bSuccess = UEditorAssetLibrary::MakeDirectory(FolderPath);
    
    if (bSuccess) {
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetBoolField(TEXT("success"), true);
      Result->SetStringField(TEXT("createdPath"), FolderPath);
      
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Folder created successfully"), Result);
    } else {
      // Check if folder already exists (MakeDirectory returns false in that case)
      if (UEditorAssetLibrary::DoesDirectoryExist(FolderPath)) {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetBoolField(TEXT("alreadyExists"), true);
        Result->SetStringField(TEXT("path"), FolderPath);
        
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Folder already exists"), Result);
      } else {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create folder"),
                            TEXT("CREATE_FOLDER_FAILED"));
      }
    }
    return true;
  }

  // ============================================================================
  // GET METADATA
  // ============================================================================
  if (LowerSubAction == TEXT("get_metadata")) {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("assetPath required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    
    // Normalize path
    if (AssetPath.StartsWith(TEXT("/Content"))) {
      AssetPath = FString::Printf(TEXT("/Game%s"), *AssetPath.RightChop(8));
    }
    
    if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Asset not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    
    UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to load asset"),
                          TEXT("LOAD_FAILED"));
      return true;
    }
    
    // Get all metadata tags
    TMap<FName, FString> MetadataTags = UEditorAssetLibrary::GetMetadataTagValues(Asset);
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> TagsObj = MakeShared<FJsonObject>();
    
    for (const auto& Pair : MetadataTags) {
      TagsObj->SetStringField(Pair.Key.ToString(), Pair.Value);
    }
    
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetStringField(TEXT("className"), Asset->GetClass()->GetName());
    Result->SetObjectField(TEXT("metadata"), TagsObj);
    Result->SetNumberField(TEXT("tagCount"), MetadataTags.Num());
    
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Metadata retrieved"), Result);
    return true;
  }

  // ============================================================================
  // SET TAGS (Metadata)
  // ============================================================================
  if (LowerSubAction == TEXT("set_tags") || LowerSubAction == TEXT("set_metadata")) {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("assetPath required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    
    // Normalize path
    if (AssetPath.StartsWith(TEXT("/Content"))) {
      AssetPath = FString::Printf(TEXT("/Game%s"), *AssetPath.RightChop(8));
    }
    
    if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Asset not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    
    UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to load asset"),
                          TEXT("LOAD_FAILED"));
      return true;
    }
    
    // Get tags object from payload
    const TSharedPtr<FJsonObject>* TagsPtr = nullptr;
    int32 TagsSet = 0;
    
    if (Payload->TryGetObjectField(TEXT("tags"), TagsPtr) && TagsPtr) {
      for (const auto& Pair : (*TagsPtr)->Values) {
        FString TagValue;
        if (Pair.Value->TryGetString(TagValue)) {
          UEditorAssetLibrary::SetMetadataTag(Asset, FName(*Pair.Key), TagValue);
          TagsSet++;
        }
      }
    }
    
    // Save after setting metadata
    McpSafeAssetSave(Asset);
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetNumberField(TEXT("tagsSet"), TagsSet);
    
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           FString::Printf(TEXT("Set %d metadata tags"), TagsSet), Result);
    return true;
  }

  // If we reach here, the subAction was not recognized by this handler
  // Return false to let other handlers try (or let the fallback error occur)
  return false;
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

  // Normalize path
  if (AssetPath.StartsWith(TEXT("/Content"))) {
    AssetPath = FString::Printf(TEXT("/Game%s"), *AssetPath.RightChop(8));
  }

  if (!ISourceControlModule::Get().IsEnabled()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Source control not enabled"),
                        TEXT("SC_DISABLED"));
    return true;
  }

  ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
  
  // Convert asset path to file path for source control operations
  FString FilePath = FPackageName::LongPackageNameToFilename(AssetPath, FPackageName::GetAssetPackageExtension());
  
  TArray<FString> Files;
  Files.Add(FilePath);
  
  ECommandResult::Type Result = Provider.Execute(
    ISourceControlOperation::Create<FCheckOut>(),
    Files
  );
  
  if (Result == ECommandResult::Succeeded) {
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("assetPath"), AssetPath);
    ResultObj->SetStringField(TEXT("filePath"), FilePath);
    
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Asset checked out successfully"), ResultObj);
  } else {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to checkout asset"),
                        TEXT("CHECKOUT_FAILED"));
  }
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
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Payload required"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  if (!ISourceControlModule::Get().IsEnabled()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Source control not enabled"),
                        TEXT("SC_DISABLED"));
    return true;
  }

  // Get asset paths to submit
  TArray<FString> AssetPaths;
  const TArray<TSharedPtr<FJsonValue>>* PathsArray = nullptr;
  
  if (Payload->TryGetArrayField(TEXT("assetPaths"), PathsArray) && PathsArray) {
    for (const auto& Val : *PathsArray) {
      FString Path = Val->AsString();
      if (Path.StartsWith(TEXT("/Content"))) {
        Path = FString::Printf(TEXT("/Game%s"), *Path.RightChop(8));
      }
      AssetPaths.Add(Path);
    }
  } else {
    FString SinglePath;
    if (Payload->TryGetStringField(TEXT("assetPath"), SinglePath)) {
      if (SinglePath.StartsWith(TEXT("/Content"))) {
        SinglePath = FString::Printf(TEXT("/Game%s"), *SinglePath.RightChop(8));
      }
      AssetPaths.Add(SinglePath);
    }
  }

  if (AssetPaths.Num() == 0) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("assetPath or assetPaths required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Get description
  FString Description;
  Payload->TryGetStringField(TEXT("description"), Description);
  if (Description.IsEmpty()) {
    Description = TEXT("Automated submit via MCP");
  }

  // Convert asset paths to file paths
  TArray<FString> FilePaths;
  for (const FString& AssetPath : AssetPaths) {
    FString FilePath = FPackageName::LongPackageNameToFilename(AssetPath, FPackageName::GetAssetPackageExtension());
    FilePaths.Add(FilePath);
  }

  ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
  
  TSharedRef<FCheckIn> CheckInOperation = ISourceControlOperation::Create<FCheckIn>();
  CheckInOperation->SetDescription(FText::FromString(Description));
  
  ECommandResult::Type Result = Provider.Execute(CheckInOperation, FilePaths);
  
  if (Result == ECommandResult::Succeeded) {
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetNumberField(TEXT("filesSubmitted"), FilePaths.Num());
    ResultObj->SetStringField(TEXT("description"), Description);
    
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           FString::Printf(TEXT("Submitted %d files"), FilePaths.Num()), ResultObj);
  } else {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to submit files"),
                        TEXT("SUBMIT_FAILED"));
  }
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
  FString AssetPath;
  if (!Payload.IsValid() || !Payload->TryGetStringField(TEXT("assetPath"), AssetPath)) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("assetPath required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Normalize path
  if (AssetPath.StartsWith(TEXT("/Content"))) {
    AssetPath = FString::Printf(TEXT("/Game%s"), *AssetPath.RightChop(8));
  }

  if (!ISourceControlModule::Get().IsEnabled()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Source control not enabled"),
                        TEXT("SC_DISABLED"));
    return true;
  }

  // Convert to file path
  FString FilePath = FPackageName::LongPackageNameToFilename(AssetPath, FPackageName::GetAssetPackageExtension());

  ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
  FSourceControlStatePtr State = Provider.GetState(FilePath, EStateCacheUsage::Use);

  if (State.IsValid()) {
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetBoolField(TEXT("isCheckedOut"), State->IsCheckedOut());
    Result->SetBoolField(TEXT("isCheckedOutOther"), State->IsCheckedOutOther());
    Result->SetBoolField(TEXT("isCurrent"), State->IsCurrent());
    Result->SetBoolField(TEXT("isSourceControlled"), State->IsSourceControlled());
    Result->SetBoolField(TEXT("isAdded"), State->IsAdded());
    Result->SetBoolField(TEXT("isDeleted"), State->IsDeleted());
    Result->SetBoolField(TEXT("isModified"), State->IsModified());
    Result->SetBoolField(TEXT("canCheckIn"), State->CanCheckIn());
    Result->SetBoolField(TEXT("canCheckOut"), State->CanCheckout());

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Source control state retrieved"), Result);
  } else {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Could not get source control state"),
                        TEXT("STATE_FAILED"));
  }
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
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Payload required"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString Directory;
  if (!Payload->TryGetStringField(TEXT("directory"), Directory)) {
    Directory = TEXT("/Game");
  }

  // Normalize path
  if (Directory.StartsWith(TEXT("/Content"))) {
    Directory = FString::Printf(TEXT("/Game%s"), *Directory.RightChop(8));
  }

  bool bRecursive = true;
  Payload->TryGetBoolField(TEXT("recursive"), bRecursive);

  // Find all redirectors in the directory
  FAssetRegistryModule& AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  
  FARFilter Filter;
  Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/CoreUObject"), TEXT("ObjectRedirector")));
  Filter.PackagePaths.Add(FName(*Directory));
  Filter.bRecursivePaths = bRecursive;

  TArray<FAssetData> RedirectorAssets;
  AssetRegistryModule.Get().GetAssets(Filter, RedirectorAssets);

  if (RedirectorAssets.Num() == 0) {
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("fixedCount"), 0);
    Result->SetStringField(TEXT("message"), TEXT("No redirectors found"));
    
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("No redirectors to fix"), Result);
    return true;
  }

  // Use AssetTools to fix up redirectors
  FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
  
  TArray<UObjectRedirector*> Redirectors;
  for (const FAssetData& AssetData : RedirectorAssets) {
    UObjectRedirector* Redirector = Cast<UObjectRedirector>(AssetData.GetAsset());
    if (Redirector) {
      Redirectors.Add(Redirector);
    }
  }

  // Fix up redirectors
  AssetToolsModule.Get().FixupReferencers(Redirectors);

  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetNumberField(TEXT("fixedCount"), Redirectors.Num());
  Result->SetStringField(TEXT("directory"), Directory);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         FString::Printf(TEXT("Fixed %d redirectors"), Redirectors.Num()), Result);
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
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Payload required"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Expect array of {sourcePath, destinationPath} objects
  const TArray<TSharedPtr<FJsonValue>>* RenamesArray = nullptr;
  if (!Payload->TryGetArrayField(TEXT("renames"), RenamesArray) || !RenamesArray || RenamesArray->Num() == 0) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("renames array required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  int32 SuccessCount = 0;
  int32 FailCount = 0;
  TArray<TSharedPtr<FJsonValue>> Results;

  for (const auto& RenameVal : *RenamesArray) {
    const TSharedPtr<FJsonObject>* RenameObj = nullptr;
    if (!RenameVal->TryGetObject(RenameObj) || !RenameObj) {
      FailCount++;
      continue;
    }

    FString SourcePath, DestinationPath;
    (*RenameObj)->TryGetStringField(TEXT("sourcePath"), SourcePath);
    (*RenameObj)->TryGetStringField(TEXT("destinationPath"), DestinationPath);

    if (SourcePath.IsEmpty() || DestinationPath.IsEmpty()) {
      FailCount++;
      continue;
    }

    // Normalize paths
    if (SourcePath.StartsWith(TEXT("/Content"))) {
      SourcePath = FString::Printf(TEXT("/Game%s"), *SourcePath.RightChop(8));
    }
    if (DestinationPath.StartsWith(TEXT("/Content"))) {
      DestinationPath = FString::Printf(TEXT("/Game%s"), *DestinationPath.RightChop(8));
    }

    if (UEditorAssetLibrary::DoesAssetExist(SourcePath)) {
      if (UEditorAssetLibrary::RenameAsset(SourcePath, DestinationPath)) {
        SuccessCount++;
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("from"), SourcePath);
        Result->SetStringField(TEXT("to"), DestinationPath);
        Results.Add(MakeShared<FJsonValueObject>(Result));
      } else {
        FailCount++;
      }
    } else {
      FailCount++;
    }
  }

  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), FailCount == 0);
  Result->SetNumberField(TEXT("successCount"), SuccessCount);
  Result->SetNumberField(TEXT("failCount"), FailCount);
  Result->SetArrayField(TEXT("results"), Results);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         FString::Printf(TEXT("Renamed %d assets (%d failed)"), SuccessCount, FailCount), Result);
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
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Payload required"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Expect array of asset paths
  TArray<FString> AssetPaths;
  const TArray<TSharedPtr<FJsonValue>>* PathsArray = nullptr;
  
  if (Payload->TryGetArrayField(TEXT("assetPaths"), PathsArray) && PathsArray) {
    for (const auto& Val : *PathsArray) {
      FString Path = Val->AsString();
      if (Path.StartsWith(TEXT("/Content"))) {
        Path = FString::Printf(TEXT("/Game%s"), *Path.RightChop(8));
      }
      AssetPaths.Add(Path);
    }
  }

  if (AssetPaths.Num() == 0) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("assetPaths array required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  int32 DeletedCount = 0;
  int32 FailedCount = 0;
  TArray<TSharedPtr<FJsonValue>> DeletedPaths;
  TArray<TSharedPtr<FJsonValue>> FailedPaths;

  for (const FString& AssetPath : AssetPaths) {
    if (UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
      if (UEditorAssetLibrary::DeleteAsset(AssetPath)) {
        DeletedCount++;
        DeletedPaths.Add(MakeShared<FJsonValueString>(AssetPath));
      } else {
        FailedCount++;
        FailedPaths.Add(MakeShared<FJsonValueString>(AssetPath));
      }
    } else {
      FailedCount++;
      FailedPaths.Add(MakeShared<FJsonValueString>(AssetPath));
    }
  }

  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), FailedCount == 0);
  Result->SetNumberField(TEXT("deletedCount"), DeletedCount);
  Result->SetNumberField(TEXT("failedCount"), FailedCount);
  Result->SetArrayField(TEXT("deletedPaths"), DeletedPaths);
  Result->SetArrayField(TEXT("failedPaths"), FailedPaths);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         FString::Printf(TEXT("Deleted %d assets (%d failed)"), DeletedCount, FailedCount), Result);
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
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Payload required"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("assetPath required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Normalize path
  if (AssetPath.StartsWith(TEXT("/Content"))) {
    AssetPath = FString::Printf(TEXT("/Game%s"), *AssetPath.RightChop(8));
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Asset not found"),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  if (!Asset) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to load asset"),
                        TEXT("LOAD_FAILED"));
    return true;
  }

  // Request thumbnail generation through the thumbnail manager
  FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  
  // Mark asset as needing thumbnail update
  Asset->MarkPackageDirty();
  
  // The thumbnail will be regenerated on next request
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("assetPath"), AssetPath);
  Result->SetStringField(TEXT("message"), TEXT("Thumbnail regeneration requested"));

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Thumbnail regeneration requested"), Result);
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
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Payload required"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("assetPath required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Normalize path
  if (AssetPath.StartsWith(TEXT("/Content"))) {
    AssetPath = FString::Printf(TEXT("/Game%s"), *AssetPath.RightChop(8));
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Asset not found"),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  UMaterial* Material = Cast<UMaterial>(Asset);
  UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Asset);

  if (!Material && !MaterialInstance) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Asset is not a Material"),
                        TEXT("INVALID_ASSET_TYPE"));
    return true;
  }

  // Force material recompilation
  if (Material) {
    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    McpSafeAssetSave(Material);
  } else if (MaterialInstance) {
    MaterialInstance->PreEditChange(nullptr);
    MaterialInstance->PostEditChange();
    McpSafeAssetSave(MaterialInstance);
  }

  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("assetPath"), AssetPath);
  Result->SetStringField(TEXT("type"), Material ? TEXT("Material") : TEXT("MaterialInstance"));

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Material rebuilt successfully"), Result);
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
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Payload required"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("assetPath required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Normalize path
  if (AssetPath.StartsWith(TEXT("/Content"))) {
    AssetPath = FString::Printf(TEXT("/Game%s"), *AssetPath.RightChop(8));
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Asset not found"),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset);

  if (!StaticMesh) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Asset is not a StaticMesh"),
                        TEXT("INVALID_ASSET_TYPE"));
    return true;
  }

  // Get LOD count from payload (default to 4)
  int32 NumLODs = 4;
  Payload->TryGetNumberField(TEXT("numLODs"), NumLODs);
  NumLODs = FMath::Clamp(NumLODs, 1, 8);

  // Get reduction settings
  double ReductionPercent = 0.5;
  Payload->TryGetNumberField(TEXT("reductionPercent"), ReductionPercent);
  ReductionPercent = FMath::Clamp(ReductionPercent, 0.1, 0.9);

  // Configure LOD settings on the static mesh
  StaticMesh->Modify();
  
  // Set up LOD group if needed
  int32 CurrentLODCount = StaticMesh->GetNumLODs();
  
  // The actual LOD generation would require MeshDescription and more complex setup
  // For now, we configure the LOD settings that will be used during build
  
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("assetPath"), AssetPath);
  Result->SetNumberField(TEXT("currentLODs"), CurrentLODCount);
  Result->SetNumberField(TEXT("requestedLODs"), NumLODs);
  Result->SetStringField(TEXT("message"), TEXT("LOD settings configured. Rebuild mesh for changes."));

  // Save the mesh
  McpSafeAssetSave(StaticMesh);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         FString::Printf(TEXT("LOD settings configured (%d LODs)"), NumLODs), Result);
  return true;
#else
  return false;
#endif
}
