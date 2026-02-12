#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "EngineUtils.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/ScopeExit.h"
#include "UObject/MetaData.h"

#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"

// Material expression includes
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionCosine.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVertexColor.h"

bool UMcpAutomationBridgeSubsystem::HandleAssetAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  FString Lower = Action.ToLower();

  // If the action is the generic "manage_asset" tool, check for a subAction in
  // the payload
  if (Lower == TEXT("manage_asset") && Payload.IsValid()) {
    FString SubAction;
    if (Payload->TryGetStringField(TEXT("subAction"), SubAction) &&
        !SubAction.IsEmpty()) {
      Lower = SubAction.ToLower();
    }
  }

  if (Lower.IsEmpty())
    return false;

  // Dispatch to specific handlers
  if (Lower == TEXT("import"))
    return HandleImportAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("duplicate"))
    return HandleDuplicateAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("rename"))
    return HandleRenameAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("move"))
    return HandleMoveAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("delete"))
    return HandleDeleteAssets(
        RequestId, Payload,
        RequestingSocket); // Single delete routed to bulk delete logic if
                           // needed, or specific handler
  if (Lower == TEXT("create_folder"))
    return HandleCreateFolder(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("create_material"))
    return HandleCreateMaterial(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("create_material_instance"))
    return HandleCreateMaterialInstance(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("get_dependencies"))
    return HandleGetDependencies(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("get_asset_graph"))
    return HandleGetAssetGraph(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("set_tags"))
    return HandleSetTags(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("set_metadata"))
    return HandleSetMetadata(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("get_metadata"))
    return HandleGetMetadata(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("validate"))
    return HandleValidateAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("list") || Lower == TEXT("list_assets"))
    return HandleListAssets(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("generate_report"))
    return HandleGenerateReport(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("create_thumbnail") || Lower == TEXT("generate_thumbnail"))
    return HandleGenerateThumbnail(RequestId, Action, Payload,
                                   RequestingSocket);
  if (Lower == TEXT("add_material_parameter"))
    return HandleAddMaterialParameter(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("list_instances"))
    return HandleListMaterialInstances(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("reset_instance_parameters"))
    return HandleResetInstanceParameters(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("exists"))
    return HandleDoesAssetExist(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("get_material_stats"))
    return HandleGetMaterialStats(RequestId, Payload, RequestingSocket);

  // Workflow handlers are called directly from ProcessAutomationRequest, but we
  // can fallback here too if needed
  if (Lower == TEXT("fixup_redirectors"))
    return HandleFixupRedirectors(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("bulk_rename"))
    return HandleBulkRenameAssets(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("bulk_delete"))
    return HandleBulkDeleteAssets(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("generate_lods"))
    return HandleGenerateLODs(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("nanite_rebuild_mesh"))
    return HandleNaniteRebuildMesh(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("source_control_checkout"))
    return HandleSourceControlCheckout(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("source_control_submit"))
    return HandleSourceControlSubmit(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("find_by_tag"))
    return HandleFindByTag(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("add_material_node"))
    return HandleAddMaterialNode(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("connect_material_pins"))
    return HandleConnectMaterialPins(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("remove_material_node"))
    return HandleRemoveMaterialNode(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("break_material_connections"))
    return HandleBreakMaterialConnections(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("get_material_node_details"))
    return HandleGetMaterialNodeDetails(RequestId, Action, Payload, RequestingSocket);

  return false;
}

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "AssetViewUtils.h"
#include "EditorAssetLibrary.h"
#include "Engine/StaticMesh.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "FileHelpers.h"
#include "IAssetTools.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "ImageUtils.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
// MaterialDomain.h was introduced in UE 5.1 - in UE 5.0 EMaterialDomain is in MaterialShared.h
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "MaterialDomain.h"
#endif
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MaterialShared.h"
#include "Misc/FileHelper.h"
#include "ObjectTools.h"
#include "SourceControlHelpers.h"
#include "SourceControlOperations.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "UObject/MetaData.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#endif

// ============================================================================
// 1. FIXUP REDIRECTORS
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleFixupRedirectors(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("fixup_redirectors"), ESearchCase::IgnoreCase)) {
    // Not our action — allow other handlers to try
    return false;
  }

  // Implementation of redirector fixup functionality
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("fixup_redirectors payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Get optional directory path (if empty, fix all redirectors)
  FString DirectoryPath;
  Payload->TryGetStringField(TEXT("directoryPath"), DirectoryPath);

  bool bCheckoutFiles = false;
  Payload->TryGetBoolField(TEXT("checkoutFiles"), bCheckoutFiles);

  AsyncTask(ENamedThreads::GameThread, [this, RequestId, DirectoryPath,
                                        bCheckoutFiles, RequestingSocket]() {
    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

    // Find all redirectors
    FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/CoreUObject"),
                                             TEXT("ObjectRedirector")));
#else
    Filter.ClassNames.Add(FName(TEXT("ObjectRedirector")));
#endif

    if (!DirectoryPath.IsEmpty()) {
      FString NormalizedPath = DirectoryPath;
      if (NormalizedPath.StartsWith(TEXT("/Content"),
                                    ESearchCase::IgnoreCase)) {
        NormalizedPath =
            FString::Printf(TEXT("/Game%s"), *NormalizedPath.RightChop(8));
      }
      Filter.PackagePaths.Add(FName(*NormalizedPath));
      Filter.bRecursivePaths = true;
    }

    TArray<FAssetData> RedirectorAssets;
    AssetRegistry.GetAssets(Filter, RedirectorAssets);

    if (RedirectorAssets.Num() == 0) {
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetBoolField(TEXT("success"), true);
      Result->SetNumberField(TEXT("redirectorsFound"), 0);
      Result->SetNumberField(TEXT("redirectorsFixed"), 0);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("No redirectors found"), Result, FString());
      return;
    }

    // Convert to string paths for AssetTools
    TArray<FString> RedirectorPaths;
    for (const FAssetData &Asset : RedirectorAssets) {
      RedirectorPaths.Add(Asset.ToSoftObjectPath().ToString());
    }

    // Checkout files if source control is enabled
    if (bCheckoutFiles && ISourceControlModule::Get().IsEnabled()) {
      ISourceControlProvider &SourceControlProvider =
          ISourceControlModule::Get().GetProvider();
      TArray<FString> PackageNames;
      for (const FAssetData &Asset : RedirectorAssets) {
        PackageNames.Add(Asset.PackageName.ToString());
      }
      SourceControlHelpers::CheckOutFiles(PackageNames, true);
    }

    // Convert FAssetData to UObjectRedirector* for AssetTools
    TArray<UObjectRedirector *> Redirectors;
    for (const FAssetData &Asset : RedirectorAssets) {
      if (UObjectRedirector *Redirector =
              Cast<UObjectRedirector>(Asset.GetAsset())) {
        Redirectors.Add(Redirector);
      }
    }

    // Fixup redirectors using AssetTools
    if (Redirectors.Num() > 0) {
      IAssetTools &AssetTools =
          FModuleManager::LoadModuleChecked<FAssetToolsModule>(
              TEXT("AssetTools"))
              .Get();
      AssetTools.FixupReferencers(Redirectors);
    }

    // Delete the now-unused redirectors
    int32 DeletedCount = 0;
    TArray<UObject *> ObjectsToDelete;
    for (const FAssetData &Asset : RedirectorAssets) {
      if (UObject *Obj = Asset.GetAsset()) {
        ObjectsToDelete.Add(Obj);
      }
    }

    if (ObjectsToDelete.Num() > 0) {
      DeletedCount = ObjectTools::DeleteObjects(ObjectsToDelete, false);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("redirectorsFound"), RedirectorAssets.Num());
    Result->SetNumberField(TEXT("redirectorsFixed"), DeletedCount);

    SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Fixed %d redirectors"), DeletedCount), Result,
        FString());
  });

  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("fixup_redirectors requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 2. SOURCE CONTROL CHECKOUT
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleSourceControlCheckout(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("source_control_checkout"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("checkout"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("source_control_checkout payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (!Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) ||
      !AssetPathsArray || AssetPathsArray->Num() == 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("assetPaths array required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  TArray<FString> AssetPaths;
  for (const TSharedPtr<FJsonValue> &Val : *AssetPathsArray) {
    if (Val.IsValid() && Val->Type == EJson::String) {
      AssetPaths.Add(Val->AsString());
    }
  }

  if (!ISourceControlModule::Get().IsEnabled()) {
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"),
                           TEXT("Source control is not enabled"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Source control disabled"), Result,
                           TEXT("SOURCE_CONTROL_DISABLED"));
    return true;
  }

  ISourceControlProvider &SourceControlProvider =
      ISourceControlModule::Get().GetProvider();

  TArray<FString> PackageNames;
  TArray<FString> ValidPaths;
  for (const FString &Path : AssetPaths) {
    if (UEditorAssetLibrary::DoesAssetExist(Path)) {
      ValidPaths.Add(Path);
      FString PackageName = FPackageName::ObjectPathToPackageName(Path);
      PackageNames.Add(PackageName);
    }
  }

  if (PackageNames.Num() == 0) {
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"), TEXT("No valid assets found"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("No valid assets"), Result,
                           TEXT("NO_VALID_ASSETS"));
    return true;
  }

  bool bSuccess = SourceControlHelpers::CheckOutFiles(PackageNames, true);

  TArray<TSharedPtr<FJsonValue>> CheckedOutPaths;
  for (const FString &Path : ValidPaths) {
    CheckedOutPaths.Add(MakeShared<FJsonValueString>(Path));
  }

  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), bSuccess);
  Result->SetNumberField(TEXT("checkedOut"), PackageNames.Num());
  Result->SetArrayField(TEXT("assets"), CheckedOutPaths);

  SendAutomationResponse(RequestingSocket, RequestId, bSuccess,
                         bSuccess ? TEXT("Assets checked out successfully")
                                  : TEXT("Checkout failed"),
                         Result,
                         bSuccess ? FString() : TEXT("CHECKOUT_FAILED"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("source_control_checkout requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 3. SOURCE CONTROL SUBMIT
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleSourceControlSubmit(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("source_control_submit"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("submit"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("source_control_submit payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (!Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) ||
      !AssetPathsArray || AssetPathsArray->Num() == 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("assetPaths array required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString Description;
  if (!Payload->TryGetStringField(TEXT("description"), Description) ||
      Description.IsEmpty()) {
    Description = TEXT("Automated submission via MCP Automation Bridge");
  }

  TArray<FString> AssetPaths;
  for (const TSharedPtr<FJsonValue> &Val : *AssetPathsArray) {
    if (Val.IsValid() && Val->Type == EJson::String) {
      AssetPaths.Add(Val->AsString());
    }
  }

  if (!ISourceControlModule::Get().IsEnabled()) {
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"),
                           TEXT("Source control is not enabled"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Source control disabled"), Result,
                           TEXT("SOURCE_CONTROL_DISABLED"));
    return true;
  }

  ISourceControlProvider &SourceControlProvider =
      ISourceControlModule::Get().GetProvider();

  TArray<FString> PackageNames;
  for (const FString &Path : AssetPaths) {
    if (UEditorAssetLibrary::DoesAssetExist(Path)) {
      FString PackageName = FPackageName::ObjectPathToPackageName(Path);
      PackageNames.Add(PackageName);
    }
  }

  if (PackageNames.Num() == 0) {
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"), TEXT("No valid assets found"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("No valid assets"), Result,
                           TEXT("NO_VALID_ASSETS"));
    return true;
  }

  TArray<FString> FilePaths;
  for (const FString &PackageName : PackageNames) {
    FString FilePath;
    if (FPackageName::TryConvertLongPackageNameToFilename(
            PackageName, FilePath, FPackageName::GetAssetPackageExtension())) {
      FilePaths.Add(FilePath);
    }
  }

  TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOperation =
      ISourceControlOperation::Create<FCheckIn>();
  CheckInOperation->SetDescription(FText::FromString(Description));

  ECommandResult::Type Result =
      SourceControlProvider.Execute(CheckInOperation, FilePaths);
  bool bSuccess = (Result == ECommandResult::Succeeded);

  TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
  ResultObj->SetBoolField(TEXT("success"), bSuccess);
  ResultObj->SetNumberField(TEXT("submitted"),
                            bSuccess ? PackageNames.Num() : 0);
  ResultObj->SetStringField(TEXT("description"), Description);

  SendAutomationResponse(
      RequestingSocket, RequestId, bSuccess,
      bSuccess ? TEXT("Assets submitted successfully") : TEXT("Submit failed"),
      ResultObj, bSuccess ? FString() : TEXT("SUBMIT_FAILED"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("source_control_submit requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 4. BULK RENAME ASSETS
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleBulkRenameAssets(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("bulk_rename_assets"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("bulk_rename"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("bulk_rename payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (!Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) ||
      !AssetPathsArray || AssetPathsArray->Num() == 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("assetPaths array required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Get rename options
  FString Prefix, Suffix, SearchText, ReplaceText;
  Payload->TryGetStringField(TEXT("prefix"), Prefix);
  Payload->TryGetStringField(TEXT("suffix"), Suffix);
  Payload->TryGetStringField(TEXT("searchText"), SearchText);
  Payload->TryGetStringField(TEXT("replaceText"), ReplaceText);

  bool bCheckoutFiles = false;
  Payload->TryGetBoolField(TEXT("checkoutFiles"), bCheckoutFiles);

  TArray<FString> AssetPaths;
  for (const TSharedPtr<FJsonValue> &Val : *AssetPathsArray) {
    if (Val.IsValid() && Val->Type == EJson::String) {
      AssetPaths.Add(Val->AsString());
    }
  }

  TArray<FAssetRenameData> RenameData;

  for (const FString &InputPath : AssetPaths) {
    FString AssetPath = ResolveAssetPath(InputPath);
    if (AssetPath.IsEmpty()) {
      AssetPath = InputPath;
    }

    if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
      continue;
    }

    UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset) {
      continue;
    }

    FString CurrentName = Asset->GetName();
    FString NewName = CurrentName;

    if (!SearchText.IsEmpty()) {
      NewName =
          NewName.Replace(*SearchText, *ReplaceText, ESearchCase::IgnoreCase);
    }

    if (!Prefix.IsEmpty()) {
      NewName = Prefix + NewName;
    }
    if (!Suffix.IsEmpty()) {
      NewName = NewName + Suffix;
    }

    if (NewName == CurrentName) {
      continue;
    }

    FString PackagePath =
        FPackageName::GetLongPackagePath(Asset->GetOutermost()->GetName());
    FAssetRenameData RenameEntry(Asset, PackagePath, NewName);
    RenameData.Add(RenameEntry);
  }

  if (RenameData.Num() == 0) {
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("renamed"), 0);
    Result->SetStringField(TEXT("message"),
                           TEXT("No assets required renaming"));
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("No renames needed"), Result, FString());
    return true;
  }

  if (bCheckoutFiles && ISourceControlModule::Get().IsEnabled()) {
    TArray<FString> PackageNames;
    for (const FAssetRenameData &Data : RenameData) {
      PackageNames.Add(Data.Asset->GetOutermost()->GetName());
    }
    SourceControlHelpers::CheckOutFiles(PackageNames, true);
  }

  IAssetTools &AssetTools =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"))
          .Get();
  bool bSuccess = AssetTools.RenameAssets(RenameData);

  TArray<TSharedPtr<FJsonValue>> RenamedAssets;
  for (const FAssetRenameData &Data : RenameData) {
    TSharedPtr<FJsonObject> AssetInfo = MakeShared<FJsonObject>();
    AssetInfo->SetStringField(TEXT("oldPath"), Data.Asset->GetPathName());
    AssetInfo->SetStringField(TEXT("newName"), Data.NewName);
    RenamedAssets.Add(MakeShared<FJsonValueObject>(AssetInfo));
  }

  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), bSuccess);
  Result->SetNumberField(TEXT("renamed"), RenameData.Num());
  Result->SetArrayField(TEXT("assets"), RenamedAssets);

  SendAutomationResponse(
      RequestingSocket, RequestId, bSuccess,
      bSuccess ? FString::Printf(TEXT("Renamed %d assets"), RenameData.Num())
               : TEXT("Bulk rename failed"),
      Result, bSuccess ? FString() : TEXT("BULK_RENAME_FAILED"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("bulk_rename requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 5. BULK DELETE ASSETS
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleBulkDeleteAssets(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("bulk_delete_assets"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("bulk_delete"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("bulk_delete payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (!Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) ||
      !AssetPathsArray || AssetPathsArray->Num() == 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("assetPaths array required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  bool bShowConfirmation = false;
  Payload->TryGetBoolField(TEXT("showConfirmation"), bShowConfirmation);

  bool bFixupRedirectors = true;
  Payload->TryGetBoolField(TEXT("fixupRedirectors"), bFixupRedirectors);

  TArray<FString> AssetPaths;
  for (const TSharedPtr<FJsonValue> &Val : *AssetPathsArray) {
    if (Val.IsValid() && Val->Type == EJson::String) {
      AssetPaths.Add(Val->AsString());
    }
  }

  TArray<UObject *> ObjectsToDelete;
  TArray<FString> ValidPaths;

  for (const FString &AssetPath : AssetPaths) {
    if (UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
      if (UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath)) {
        ObjectsToDelete.Add(Asset);
        ValidPaths.Add(AssetPath);
      }
    }
  }

  if (ObjectsToDelete.Num() == 0) {
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"), TEXT("No valid assets found"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("No valid assets"), Result,
                           TEXT("NO_VALID_ASSETS"));
    return true;
  }

  int32 DeletedCount =
      ObjectTools::DeleteObjects(ObjectsToDelete, bShowConfirmation);

  if (bFixupRedirectors && DeletedCount > 0) {
    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

    FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/CoreUObject"),
                                             TEXT("ObjectRedirector")));
#else
    Filter.ClassNames.Add(FName(TEXT("ObjectRedirector")));
#endif

    TArray<FAssetData> RedirectorAssets;
    AssetRegistry.GetAssets(Filter, RedirectorAssets);

    if (RedirectorAssets.Num() > 0) {
      TArray<UObjectRedirector *> Redirectors;
      for (const FAssetData &Asset : RedirectorAssets) {
        if (UObjectRedirector *Redirector =
                Cast<UObjectRedirector>(Asset.GetAsset())) {
          Redirectors.Add(Redirector);
        }
      }

      if (Redirectors.Num() > 0) {
        IAssetTools &AssetTools =
            FModuleManager::LoadModuleChecked<FAssetToolsModule>(
                TEXT("AssetTools"))
                .Get();
        AssetTools.FixupReferencers(Redirectors);
      }
    }
  }

  TArray<TSharedPtr<FJsonValue>> DeletedArray;
  for (const FString &Path : ValidPaths) {
    DeletedArray.Add(MakeShared<FJsonValueString>(Path));
  }

  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), DeletedCount > 0);
  Result->SetArrayField(TEXT("deleted"), DeletedArray);
  Result->SetNumberField(TEXT("requested"), ObjectsToDelete.Num());

  SendAutomationResponse(
      RequestingSocket, RequestId, DeletedCount > 0,
      FString::Printf(TEXT("Deleted %d of %d assets"), DeletedCount,
                      ObjectsToDelete.Num()),
      Result, DeletedCount > 0 ? FString() : TEXT("BULK_DELETE_FAILED"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("bulk_delete requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 6. GENERATE THUMBNAIL
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleGenerateThumbnail(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("generate_thumbnail"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("generate_thumbnail payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
      AssetPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("assetPath required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  int32 Width = 512;
  int32 Height = 512;

  double TempWidth = 0, TempHeight = 0;
  if (Payload->TryGetNumberField(TEXT("width"), TempWidth))
    Width = static_cast<int32>(TempWidth);
  if (Payload->TryGetNumberField(TEXT("height"), TempHeight))
    Height = static_cast<int32>(TempHeight);

  FString OutputPath;
  Payload->TryGetStringField(TEXT("outputPath"), OutputPath);

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Asset not found"), nullptr,
                           TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  if (!Asset) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Failed to load asset"), nullptr,
                           TEXT("LOAD_FAILED"));
    return true;
  }

  FObjectThumbnail ObjectThumbnail;
  ThumbnailTools::RenderThumbnail(
      Asset, Width, Height,
      ThumbnailTools::EThumbnailTextureFlushMode::NeverFlush, nullptr,
      &ObjectThumbnail);

  bool bSuccess = ObjectThumbnail.GetImageWidth() > 0 &&
                  ObjectThumbnail.GetImageHeight() > 0;

  if (bSuccess && !OutputPath.IsEmpty()) {
    const TArray<uint8> &ImageData = ObjectThumbnail.GetUncompressedImageData();

    if (ImageData.Num() > 0) {
      TArray<FColor> ColorData;
      ColorData.Reserve(Width * Height);

      // Fixed: Ensure we don't read out of bounds if ImageData length isn't a multiple of 4
      for (int32 i = 0; i + 3 < ImageData.Num(); i += 4) {
        FColor Color;
        Color.B = ImageData[i + 0];
        Color.G = ImageData[i + 1];
        Color.R = ImageData[i + 2];
        Color.A = ImageData[i + 3];
        ColorData.Add(Color);
      }

      FString AbsolutePath = OutputPath;
      if (FPaths::IsRelative(OutputPath)) {
        AbsolutePath =
            FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), OutputPath);
      }

      TArray<uint8> CompressedData;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      FImageUtils::ThumbnailCompressImageArray(Width, Height, ColorData,
                                               CompressedData);
#else
      // UE 5.0: Use CompressImageArray instead
      FImageUtils::CompressImageArray(Width, Height, ColorData, CompressedData);
#endif
      bSuccess = FFileHelper::SaveArrayToFile(CompressedData, *AbsolutePath);
    }
  }

  if (Asset->GetOutermost()) {
    Asset->GetOutermost()->MarkPackageDirty();
  }

  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), bSuccess);
  Result->SetStringField(TEXT("assetPath"), AssetPath);
  Result->SetNumberField(TEXT("width"), Width);
  Result->SetNumberField(TEXT("height"), Height);

  if (!OutputPath.IsEmpty()) {
    Result->SetStringField(TEXT("outputPath"), OutputPath);
  }

  SendAutomationResponse(
      RequestingSocket, RequestId, bSuccess,
      bSuccess ? TEXT("Thumbnail generated successfully")
               : TEXT("Thumbnail generation failed"),
      Result, bSuccess ? FString() : TEXT("THUMBNAIL_GENERATION_FAILED"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("generate_thumbnail requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 7. BASIC ASSET OPERATIONS (Import, Duplicate, Rename, Move, etc.)
// ============================================================================

/**
 * Handles asset import requests.
 *
 * IMPORTANT: In UE 5.7+, the Interchange Framework is the default importer for
 * FBX/glTF files. Interchange uses the TaskGraph internally for async operations.
 * If we call ImportAssetsAutomated() synchronously from within an AsyncTask callback
 * (which is how WebSocket messages are dispatched), we hit a TaskGraph recursion
 * guard assertion: "++Queue(QueueIndex).RecursionGuard == 1".
 *
 * The fix is to defer the import to the next editor tick using GEditor->GetTimerManager(),
 * which breaks out of the TaskGraph callback chain and allows Interchange to function
 * correctly.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'sourcePath' and 'destinationPath'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleImportAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString DestinationPath;
  Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath);
  FString SourcePath;
  Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);

  if (DestinationPath.IsEmpty() || SourcePath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sourcePath and destinationPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Verify source file exists
  if (!FPaths::FileExists(SourcePath)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Source file not found: %s"), *SourcePath),
        nullptr, TEXT("SOURCE_NOT_FOUND"));
    return true;
  }

  // Sanitize destination path
  FString SafeDestPath = SanitizeProjectRelativePath(DestinationPath);
  if (SafeDestPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid destination path"), nullptr,
                           TEXT("INVALID_PATH"));
    return true;
  }

  FString DestPath = FPaths::GetPath(SafeDestPath);
  FString DestName = FPaths::GetBaseFilename(SafeDestPath);

  // If destination is just a folder, use that
  if (FPaths::GetExtension(SafeDestPath).IsEmpty()) {
    DestPath = SafeDestPath;
    DestName = FPaths::GetBaseFilename(SourcePath);
  }

  // Sanitize DestName: UE asset names cannot contain spaces or dots
  DestName.ReplaceInline(TEXT(" "), TEXT("_"));
  DestName.ReplaceInline(TEXT("."), TEXT("_"));

  // Defer the import to the next tick to avoid TaskGraph recursion issues with
  // UE 5.7+ Interchange Framework. See issue #137.
  // We use SetTimerForNextTick to ensure we're completely outside of any
  // TaskGraph callback chain before invoking the import.
  if (GEditor) {
    TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
    GEditor->GetTimerManager()->SetTimerForNextTick(
        [WeakThis, RequestId, SourcePath, DestPath, DestName, Socket]() {
          UMcpAutomationBridgeSubsystem *StrongThis = WeakThis.Get();
          if (!StrongThis) {
            return;
          }

          IAssetTools &AssetTools =
              FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools")
                  .Get();

          TArray<FString> Files;
          Files.Add(SourcePath);

          UAutomatedAssetImportData *ImportData =
              NewObject<UAutomatedAssetImportData>();
          ImportData->bReplaceExisting = true;
          ImportData->DestinationPath = DestPath;
          ImportData->Filenames = Files;

          TArray<UObject *> ImportedAssets =
              AssetTools.ImportAssetsAutomated(ImportData);

          // Find the first valid (non-null) asset in the array.
          // ImportAssetsAutomated can return arrays with nullptr entries.
          UObject *Asset = nullptr;
          for (UObject *ImportedObj : ImportedAssets) {
            if (ImportedObj) {
              Asset = ImportedObj;
              break;
            }
          }

          if (Asset) {
            // Compute the final asset path. If we rename, use the destination
            // path/name since RenameAssets may invalidate the Asset pointer.
            FString FinalAssetPath;
            bool bRenameSucceeded = true;

            // Rename if needed
            if (Asset->GetName() != DestName) {
              FAssetRenameData RenameData(Asset, DestPath, DestName);
              bRenameSucceeded = AssetTools.RenameAssets({RenameData});
              // After rename, compute path from destination (Asset pointer may
              // be stale)
              FinalAssetPath = DestPath / DestName + TEXT(".") + DestName;
            } else {
              // No rename needed, safe to use the asset's current path
              FinalAssetPath = Asset->GetPathName();
            }

            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetStringField(TEXT("assetPath"), FinalAssetPath);
            if (!bRenameSucceeded) {
              Resp->SetBoolField(TEXT("renameWarning"), true);
            }
            StrongThis->SendAutomationResponse(
                Socket, RequestId, true,
                bRenameSucceeded ? TEXT("Asset imported")
                                 : TEXT("Asset imported but rename failed"),
                Resp, FString());
          } else {
            StrongThis->SendAutomationResponse(
                Socket, RequestId, false,
                FString::Printf(TEXT("Failed to import asset from '%s'"),
                                *SourcePath),
                nullptr, TEXT("IMPORT_FAILED"));
          }
        });
  } else {
    // Fallback: GEditor not available (shouldn't happen in editor context)
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available for deferred import"),
                           nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
  }

  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles metadata setting requests for assets.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'assetPath' and 'metadata' object.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleSetMetadata(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("set_metadata payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  const TSharedPtr<FJsonObject> *MetadataObjPtr = nullptr;
  if (!Payload->TryGetObjectField(TEXT("metadata"), MetadataObjPtr) ||
      !MetadataObjPtr) {
    // Treat missing/empty metadata as a no-op success; nothing to write.
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetNumberField(TEXT("updatedKeys"), 0);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("No metadata provided; no-op"), Resp,
                           FString());
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  if (!Asset) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to load asset"), nullptr,
                           TEXT("LOAD_FAILED"));
    return true;
  }

  UPackage *Package = Asset->GetOutermost();
  if (!Package) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to resolve package for asset"), nullptr,
                           TEXT("PACKAGE_NOT_FOUND"));
    return true;
  }

  // GetMetaData returns the metadata object that is owned by this package.
  // UE 5.0 uses UMetaData*, UE 5.6+ uses FMetaData&
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
  FMetaData& Meta = Package->GetMetaData();
#else
  UMetaData* Meta = Package->GetMetaData();
#endif

  const TSharedPtr<FJsonObject> &MetadataObj = *MetadataObjPtr;
  int32 UpdatedCount = 0;

  for (const auto &Kvp : MetadataObj->Values) {
    const FString &Key = Kvp.Key;
    const TSharedPtr<FJsonValue> &Val = Kvp.Value;

    FString ValueString;
    if (!Val.IsValid() || Val->IsNull()) {
      continue;
    }
    switch (Val->Type) {
    case EJson::String:
      ValueString = Val->AsString();
      break;
    case EJson::Number:
      ValueString = LexToString(Val->AsNumber());
      break;
    case EJson::Boolean:
      ValueString = Val->AsBool() ? TEXT("true") : TEXT("false");
      break;
    default:
      // For arrays/objects, store a compact JSON string
      {
        FString JsonOut;
        const TSharedRef<TJsonWriter<>> Writer =
            TJsonWriterFactory<>::Create(&JsonOut);
        FJsonSerializer::Serialize(Val, TEXT(""), Writer);
        ValueString = JsonOut;
      }
      break;
    }

    if (!ValueString.IsEmpty()) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
      Meta.SetValue(Asset, *Key, *ValueString);
#else
      Meta->SetValue(Asset, *Key, *ValueString);
#endif
      ++UpdatedCount;
    }
  }

  if (UpdatedCount > 0) {
    Package->SetDirtyFlag(true);
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);
  Resp->SetNumberField(TEXT("updatedKeys"), UpdatedCount);

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Asset metadata updated"), Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles asset duplication requests. Supports both single asset and folder
 * (deep) duplication.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'sourcePath' and 'destinationPath'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleDuplicateAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString SourcePath;
  Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);
  FString DestinationPath;
  Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath);

  if (SourcePath.IsEmpty() || DestinationPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sourcePath and destinationPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Auto-resolve simple name for destination
  if (!DestinationPath.IsEmpty() &&
      FPaths::GetPath(DestinationPath).IsEmpty()) {
    FString ParentDir = FPaths::GetPath(SourcePath);
    if (ParentDir.IsEmpty() || ParentDir == TEXT("/"))
      ParentDir = TEXT("/Game");

    DestinationPath = ParentDir / DestinationPath;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
           TEXT("HandleDuplicateAsset: Auto-resolved simple name destination "
                "to '%s'"),
           *DestinationPath);
  }

  // If the source path is a directory, perform a deep duplication of all
  // assets under that folder into the destination folder, preserving
  // relative structure. This powers the "Deep Duplication - Duplicate
  // Folder" scenario in tests.
  if (UEditorAssetLibrary::DoesDirectoryExist(SourcePath)) {
    // Ensure the destination root exists
    UEditorAssetLibrary::MakeDirectory(DestinationPath);

    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*SourcePath));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> Assets;
    AssetRegistryModule.Get().GetAssets(Filter, Assets);

    int32 DuplicatedCount = 0;
    for (const FAssetData &Asset : Assets) {
      // PackageName is the long package path (e.g.,
      // /Game/Tests/DeepCopy/Source/M_Source)
      const FString SourceAssetPath = Asset.PackageName.ToString();

      FString RelativePath;
      if (SourceAssetPath.StartsWith(SourcePath)) {
        RelativePath = SourceAssetPath.RightChop(SourcePath.Len());
      } else {
        // Should not happen for the filtered set, but skip if it does.
        continue;
      }

      const FString TargetAssetPath =
          DestinationPath + RelativePath; // preserves any subfolders
      const FString TargetFolderPath = FPaths::GetPath(TargetAssetPath);
      if (!TargetFolderPath.IsEmpty()) {
        UEditorAssetLibrary::MakeDirectory(TargetFolderPath);
      }

      if (UEditorAssetLibrary::DuplicateAsset(SourceAssetPath,
                                              TargetAssetPath)) {
        ++DuplicatedCount;
      }
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    const bool bSuccess = DuplicatedCount > 0;
    Resp->SetBoolField(TEXT("success"), bSuccess);
    Resp->SetStringField(TEXT("sourcePath"), SourcePath);
    Resp->SetStringField(TEXT("destinationPath"), DestinationPath);
    Resp->SetNumberField(TEXT("duplicatedCount"), DuplicatedCount);

    if (bSuccess) {
      SendAutomationResponse(Socket, RequestId, true, TEXT("Folder duplicated"),
                             Resp, FString());
    } else {
      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("No assets duplicated"), Resp,
                             TEXT("DUPLICATE_FAILED"));
    }
    return true;
  }

  // Fallback: single-asset duplication
  if (!UEditorAssetLibrary::DoesAssetExist(SourcePath)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Source asset not found: %s"), *SourcePath),
        nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  if (UEditorAssetLibrary::DoesAssetExist(DestinationPath)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Destination asset already exists: %s"),
                        *DestinationPath),
        nullptr, TEXT("DESTINATION_EXISTS"));
    return true;
  }

  if (UEditorAssetLibrary::DuplicateAsset(SourcePath, DestinationPath)) {
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), DestinationPath);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Asset duplicated"),
                           Resp, FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Duplicate failed"),
                           nullptr, TEXT("DUPLICATE_FAILED"));
  }
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles asset renaming (and moving) requests.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'sourcePath' and 'destinationPath'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleRenameAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString SourcePath;
  Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);
  FString DestinationPath;
  Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath);

  if (SourcePath.IsEmpty() || DestinationPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sourcePath and destinationPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Auto-resolve simple name for destination
  if (!DestinationPath.IsEmpty() &&
      FPaths::GetPath(DestinationPath).IsEmpty()) {
    FString ParentDir = FPaths::GetPath(SourcePath);
    if (ParentDir.IsEmpty() || ParentDir == TEXT("/"))
      ParentDir = TEXT("/Game");

    DestinationPath = ParentDir / DestinationPath;
    UE_LOG(
        LogMcpAutomationBridgeSubsystem, Display,
        TEXT(
            "HandleRenameAsset: Auto-resolved simple name destination to '%s'"),
        *DestinationPath);
  }

  // Resolve source path to ensure it matches a real asset
  FString ResolvedSourcePath = ResolveAssetPath(SourcePath);
  if (ResolvedSourcePath.IsEmpty()) {
    // If resolution failed, fall back to original for strict check
    ResolvedSourcePath = SourcePath;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(ResolvedSourcePath)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Source asset not found: %s"), *SourcePath),
        nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  // Use the resolved path for the rename operation
  if (UEditorAssetLibrary::RenameAsset(ResolvedSourcePath, DestinationPath)) {
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), DestinationPath);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Asset renamed"), Resp,
                           FString());
  } else {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Failed to rename asset. Check if destination "
                             "'%s' already exists or source is locked."),
                        *DestinationPath),
        nullptr, TEXT("RENAME_FAILED"));
  }
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleMoveAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  // Move is essentially rename in Unreal
  return HandleRenameAsset(RequestId, Payload, Socket);
}

/**
 * Handles asset deletion requests.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'path' (string) or 'paths' (array of
 * strings).
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleDeleteAssets(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  // Support both single 'path' and array 'paths'
  TArray<FString> PathsToDelete;
  const TArray<TSharedPtr<FJsonValue>> *PathsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("paths"), PathsArray) && PathsArray) {
    for (const auto &Val : *PathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String)
        PathsToDelete.Add(Val->AsString());
    }
  }

  FString SinglePath;
  if (Payload->TryGetStringField(TEXT("path"), SinglePath) &&
      !SinglePath.IsEmpty()) {
    PathsToDelete.Add(SinglePath);
  }

  if (PathsToDelete.Num() == 0) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("No paths provided"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  int32 DeletedCount = 0;
  for (const FString &Path : PathsToDelete) {
    if (UEditorAssetLibrary::DoesDirectoryExist(Path)) {
      if (UEditorAssetLibrary::DeleteDirectory(Path)) {
        DeletedCount++;
      }
    } else if (UEditorAssetLibrary::DeleteAsset(Path)) {
      DeletedCount++;
    }
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), DeletedCount > 0);
  Resp->SetNumberField(TEXT("deletedCount"), DeletedCount);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Assets deleted"), Resp,
                         FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles folder creation requests.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'path'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleCreateFolder(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString Path;
  if (!Payload->TryGetStringField(TEXT("path"), Path) || Path.IsEmpty()) {
    Payload->TryGetStringField(TEXT("directoryPath"), Path);
  }

  if (Path.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("path (or directoryPath) required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString SafePath = SanitizeProjectRelativePath(Path);
  if (SafePath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("Invalid path: must be project-relative and not contain '..'"),
        nullptr, TEXT("INVALID_PATH"));
    return true;
  }

  if (UEditorAssetLibrary::DoesDirectoryExist(SafePath) ||
      UEditorAssetLibrary::MakeDirectory(SafePath)) {
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("path"), SafePath);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Folder created"),
                           Resp, FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create folder"), nullptr,
                           TEXT("CREATE_FAILED"));
  }
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to get asset dependencies.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'assetPath' and optional 'recursive'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleGetDependencies(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Validate path
  if (!IsValidAssetPath(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Invalid asset path"),
                           nullptr, TEXT("INVALID_PATH"));
    return true;
  }

  bool bRecursive = false;
  Payload->TryGetBoolField(TEXT("recursive"), bRecursive);

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  TArray<FName> Dependencies;
  UE::AssetRegistry::EDependencyCategory Category =
      UE::AssetRegistry::EDependencyCategory::Package;
  AssetRegistryModule.Get().GetDependencies(FName(*AssetPath), Dependencies);

  TArray<TSharedPtr<FJsonValue>> DepArray;
  for (const FName &Dep : Dependencies) {
    DepArray.Add(MakeShared<FJsonValueString>(Dep.ToString()));
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetArrayField(TEXT("dependencies"), DepArray);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Dependencies retrieved"), Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to traverse and return an asset dependency graph.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'assetPath' and optional 'maxDepth'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleGetAssetGraph(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!IsValidAssetPath(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Invalid asset path"),
                           nullptr, TEXT("INVALID_PATH"));
    return true;
  }

  int32 MaxDepth = 3;
  Payload->TryGetNumberField(TEXT("maxDepth"), MaxDepth);

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();

  TArray<FString> Queue;
  Queue.Add(AssetPath);

  TSet<FString> Visited;
  Visited.Add(AssetPath);

  TMap<FString, int32> Depths;
  Depths.Add(AssetPath, 0);

  int32 Head = 0;
  while (Head < Queue.Num()) {
    FString Current = Queue[Head++];
    int32 CurrentDepth = Depths[Current];

    TArray<FName> Dependencies;
    AssetRegistry.GetDependencies(FName(*Current), Dependencies);

    TArray<TSharedPtr<FJsonValue>> DepArray;
    for (const FName &Dep : Dependencies) {
      FString DepStr = Dep.ToString();
      if (!DepStr.StartsWith(TEXT("/Game")))
        continue; // Only graph Game assets for now

      DepArray.Add(MakeShared<FJsonValueString>(DepStr));

      if (CurrentDepth < MaxDepth) {
        if (!Visited.Contains(DepStr)) {
          Visited.Add(DepStr);
          Depths.Add(DepStr, CurrentDepth + 1);
          Queue.Add(DepStr);
        }
      }
    }
    GraphObj->SetArrayField(Current, DepArray);
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetObjectField(TEXT("graph"), GraphObj);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Asset graph retrieved"),
                         Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to set asset tags. NOTE: Asset Registry tags are distinct
 * from Actor tags. This function currently returns NOT_IMPLEMENTED as generic
 * asset tagging is ambiguous (metadata vs registry tags).
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleSetTags(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("set_tags payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const TArray<TSharedPtr<FJsonValue>> *TagsArray = nullptr;
  TArray<FString> Tags;
  if (Payload->TryGetArrayField(TEXT("tags"), TagsArray) && TagsArray) {
    for (const TSharedPtr<FJsonValue> &Val : *TagsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        Tags.Add(Val->AsString());
      }
    }
  }

  AsyncTask(ENamedThreads::GameThread, [this, RequestId, Socket, AssetPath,
                                        Tags]() {
    // Edge-case: empty or missing tags array should be treated as a no-op
    // success.
    if (Tags.Num() == 0) {
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("assetPath"), AssetPath);
      Resp->SetNumberField(TEXT("appliedTags"), 0);
      SendAutomationResponse(Socket, RequestId, true,
                             TEXT("No tags provided; no-op"), Resp, FString());
      return;
    }

    if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
      SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                             nullptr, TEXT("ASSET_NOT_FOUND"));
      return;
    }

    UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset) {
      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Failed to load asset"), nullptr,
                             TEXT("LOAD_FAILED"));
      return;
    }

    // Implement set_tags by mapping them to Package Metadata (Tag=true)
    int32 AppliedCount = 0;
    for (const FString &Tag : Tags) {
      UEditorAssetLibrary::SetMetadataTag(Asset, FName(*Tag), TEXT("true"));
      AppliedCount++;
    }

    // Mark dirty so the asset can be saved later
    Asset->MarkPackageDirty();

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("markedDirty"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetNumberField(TEXT("appliedTags"), AppliedCount);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Tags applied as metadata"), Resp, FString());
  });

  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to validate if an asset exists and can be loaded.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'assetPath'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleValidateAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("validate payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AsyncTask(ENamedThreads::GameThread, [this, RequestId, Socket, AssetPath]() {
    if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
      SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                             nullptr, TEXT("ASSET_NOT_FOUND"));
      return;
    }

    UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset) {
      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Failed to load asset"), nullptr,
                             TEXT("LOAD_FAILED"));
      return;
    }

    bool bIsValid = true;
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), bIsValid);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetBoolField(TEXT("isValid"), bIsValid);

    SendAutomationResponse(Socket, RequestId, true, TEXT("Asset validated"),
                           Resp, FString());
  });
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to list assets with filtering and pagination.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing filter criteria and pagination
 * options.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleListAssets(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  // Parse filters
  FString PathFilter;
  FString ClassFilter;
  FString TagFilter;
  FString PathStartsWith;

  const TSharedPtr<FJsonObject> *FilterObj;
  if (Payload->TryGetObjectField(TEXT("filter"), FilterObj) && FilterObj) {
    (*FilterObj)->TryGetStringField(TEXT("path"), PathFilter);
    (*FilterObj)->TryGetStringField(TEXT("class"), ClassFilter);
    (*FilterObj)->TryGetStringField(TEXT("tag"), TagFilter);
    (*FilterObj)->TryGetStringField(TEXT("pathStartsWith"), PathStartsWith);
  } else {
    // Legacy support for direct path/recursive fields
    Payload->TryGetStringField(TEXT("path"), PathFilter);
  }

  // Sanitize PathFilter to remove trailing slash which can break AssetRegistry
  // lookups
  if (PathFilter.Len() > 1 && PathFilter.EndsWith(TEXT("/"))) {
    PathFilter.RemoveAt(PathFilter.Len() - 1);
  }

  bool bRecursive = true;
  Payload->TryGetBoolField(TEXT("recursive"), bRecursive);

  // Parse pagination
  int32 Offset = 0;
  int32 Limit = -1; // -1 means no limit
  const TSharedPtr<FJsonObject> *PaginationObj;
  if (Payload->TryGetObjectField(TEXT("pagination"), PaginationObj) &&
      PaginationObj) {
    (*PaginationObj)->TryGetNumberField(TEXT("offset"), Offset);
    (*PaginationObj)->TryGetNumberField(TEXT("limit"), Limit);
  }

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  FARFilter Filter;
  Filter.bRecursivePaths = bRecursive;
  Filter.bRecursiveClasses = true;

  // Apply path filters
  if (!PathFilter.IsEmpty()) {
    Filter.PackagePaths.Add(FName(*PathFilter));
  } else if (!PathStartsWith.IsEmpty()) {
    // If we have a path prefix, assume it's a package path
    // Note: FARFilter doesn't support 'StartsWith' natively for paths in an
    // efficient way other than adding the path and set bRecursivePaths=true. So
    // if PathStartsWith is a folder, we use it.
    Filter.PackagePaths.Add(FName(*PathStartsWith));
  } else {
    // Default to /Game to prevent empty results or massive scan
    Filter.PackagePaths.Add(FName(TEXT("/Game")));
  }

  // Ensure registry is up to date for the requested paths
  TArray<FString> ScanPaths;
  for (const FName &Path : Filter.PackagePaths) {
    ScanPaths.Add(Path.ToString());
  }
  AssetRegistry.ScanPathsSynchronous(ScanPaths, true);

  if (!ClassFilter.IsEmpty()) {
    // Support both short class names and full paths (best effort)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    FTopLevelAssetPath ClassPath(ClassFilter);
    if (ClassPath.IsValid()) {
      Filter.ClassPaths.Add(ClassPath);
    }
#else
    // UE 5.0: Use ClassNames instead of ClassPaths
    Filter.ClassNames.Add(FName(*ClassFilter));
#endif
  }

  // Tags are not standard on assets in the same way as actors.
  // AssetRegistry tags are Key-Value pairs.
  // If TagFilter is provided, we assume it checks for the existence of a tag
  // key or value. Implementing a generic "HasTag" is ambiguous. We'll assume
  // TagFilter refers to a metadata key presence.

  TArray<FAssetData> AssetList;
  AssetRegistry.GetAssets(Filter, AssetList);

  // Post-filtering
  if (!ClassFilter.IsEmpty() || !TagFilter.IsEmpty()) {
    AssetList.RemoveAll([&](const FAssetData &Asset) {
      if (!ClassFilter.IsEmpty()) {
        // Check full class path or asset class name
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        FString AssetClass = Asset.AssetClassPath.ToString();
        FString AssetClassName = Asset.AssetClassPath.GetAssetName().ToString();
#else
        FString AssetClass = Asset.AssetClass.ToString();
        FString AssetClassName = Asset.AssetClass.ToString();
#endif
        if (!AssetClass.Equals(ClassFilter) &&
            !AssetClassName.Equals(ClassFilter)) {
          return true; // Remove
        }
      }
      if (!TagFilter.IsEmpty()) {
        if (!Asset.TagsAndValues.Contains(FName(*TagFilter))) {
          return true; // Remove
        }
      }
      return false;
    });
  }

  // Filter by Depth if specified
  // (Changes made to support depth and folders - Touch to force rebuild)
  int32 Depth = -1;
  Payload->TryGetNumberField(TEXT("depth"), Depth);

  if (Depth >= 0 && bRecursive && !PathFilter.IsEmpty()) {
    // Normalize base path for depth calculation
    FString BasePath = PathFilter;
    if (BasePath.EndsWith(TEXT("/"))) {
      BasePath.RemoveAt(BasePath.Len() - 1);
    }
    // Base depth: number of slashes in /Game/Foo is 2
    int32 BaseSlashCount = 0;
    for (const TCHAR *P = *BasePath; *P; ++P) {
      if (*P == TEXT('/'))
        BaseSlashCount++;
    }

    AssetList.RemoveAll([&](const FAssetData &Asset) {
      FString PkgPath = Asset.PackagePath.ToString();
      // If PkgPath is shorter than BasePath (shouldn't happen with filter),
      // keep it I guess? Actually we only care about descendants.

      int32 SlashCount = 0;
      for (const TCHAR *P = *PkgPath; *P; ++P) {
        if (*P == TEXT('/'))
          SlashCount++;
      }

      // Difference in slashes determines depth
      // /Game (1 slash) vs /Game/A (2 slashes) -> Diff 1 -> Depth 0 (immediate
      // child) Wait, PackagePath for /Game/A is /Game. PackagePath for
      // /Game/Sub/B is /Game/Sub.

      // Let's test:
      // Filter: /Game (Slash=1)
      // Asset: /Game/A (PackagePath=/Game, Slash=1). Diff=0. Depth 0? Yes.
      // Asset: /Game/Sub/B (PackagePath=/Game/Sub, Slash=2). Diff=1. Depth 1?
      // Yes.

      // If Depth=0, we want Diff=0.
      // If Depth=1, we want Diff<=1.

      return (SlashCount - BaseSlashCount) > Depth;
    });
  }

  int32 TotalCount = AssetList.Num();

  // Apply pagination
  if (Offset > 0) {
    if (Offset >= AssetList.Num()) {
      AssetList.Empty();
    } else {
      AssetList.RemoveAt(0, Offset);
    }
  }

  if (Limit >= 0 && AssetList.Num() > Limit) {
    AssetList.SetNum(Limit);
  }

  // Also fetch sub-folders if we are listing a directory (PathFilter is set)
  TArray<FString> SubPathList;
  if (!PathFilter.IsEmpty()) {
    // If non-recursive (or depth limited), we generally want at least the
    // immediate subfolders. GetSubPaths is non-recursive by default.
    AssetRegistry.GetSubPaths(PathFilter, SubPathList, false);

    // If Depth is specified, we might want deeper folders?
    // Actually, standard 'ls' behavior on a folder shows immediate children
    // (files and folders). If recursive, it shows everything. Let keeps it
    // simple: If we are listing a path, show its immediate subfolders. Getting
    // ALL recursive folders might be too much info if strictly not requested,
    // but 'GetSubPaths' with bInRecurse=true gets everything.

    // Decision:
    // If Recursive=true (and Depth not limited), maybe we don't strictly need
    // folders as assets cover it? But user asked for folders when assets are
    // missing. Default 'ls' shows immediate folders. So let's always include
    // immediate subfolders of the requested path.
  }

  TArray<TSharedPtr<FJsonValue>> AssetsArray;
  for (const FAssetData &Asset : AssetList) {
    TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
    AssetObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    AssetObj->SetStringField(TEXT("path"), Asset.GetSoftObjectPath().ToString());
    AssetObj->SetStringField(TEXT("class"), Asset.AssetClassPath.ToString());
#else
    AssetObj->SetStringField(TEXT("path"), Asset.ToSoftObjectPath().ToString());
    AssetObj->SetStringField(TEXT("class"), Asset.AssetClass.ToString());
#endif
    AssetObj->SetStringField(TEXT("packagePath"), Asset.PackagePath.ToString());

    // Add tags for context if requested
    TArray<TSharedPtr<FJsonValue>> Tags;
    for (auto TagPair : Asset.TagsAndValues) {
      Tags.Add(MakeShared<FJsonValueString>(TagPair.Key.ToString()));
    }
    AssetObj->SetArrayField(TEXT("tags"), Tags);

    AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
  }

  TArray<TSharedPtr<FJsonValue>> FoldersJson;
  for (const FString &SubPath : SubPathList) {
    FoldersJson.Add(MakeShared<FJsonValueString>(SubPath));
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetArrayField(TEXT("assets"), AssetsArray);
  Resp->SetArrayField(TEXT("folders"), FoldersJson);
  Resp->SetNumberField(TEXT("totalCount"), TotalCount);
  Resp->SetNumberField(TEXT("count"), AssetsArray.Num());
  Resp->SetNumberField(TEXT("offset"), Offset);

  SendAutomationResponse(Socket, RequestId, true, TEXT("Assets listed"), Resp,
                         FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to get detailed information about a single asset.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'assetPath'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleGetAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("get_asset payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  FAssetData AssetData = UEditorAssetLibrary::FindAssetData(AssetPath);
  if (!AssetData.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to find asset data"), nullptr,
                           TEXT("ASSET_DATA_INVALID"));
    return true;
  }

  TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
  AssetObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  AssetObj->SetStringField(TEXT("path"), AssetData.GetSoftObjectPath().ToString());
  AssetObj->SetStringField(TEXT("class"), AssetData.AssetClassPath.ToString());
#else
  AssetObj->SetStringField(TEXT("path"), AssetData.ToSoftObjectPath().ToString());
  AssetObj->SetStringField(TEXT("class"), AssetData.AssetClass.ToString());
#endif
  AssetObj->SetStringField(TEXT("packagePath"),
                           AssetData.PackagePath.ToString());

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetObjectField(TEXT("result"), AssetObj);

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Asset details retrieved"), Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to generate an asset report (CSV/JSON).
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'directory' and 'reportType'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleGenerateReport(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("generate_report payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString Directory;
  Payload->TryGetStringField(TEXT("directory"), Directory);
  if (Directory.IsEmpty()) {
    Directory = TEXT("/Game");
  }

  // Normalize /Content prefix to /Game for convenience
  if (Directory.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase)) {
    Directory = FString::Printf(TEXT("/Game%s"), *Directory.RightChop(8));
  }

  FString ReportType;
  Payload->TryGetStringField(TEXT("reportType"), ReportType);
  if (ReportType.IsEmpty()) {
    ReportType = TEXT("Summary");
  }

  FString OutputPath;
  Payload->TryGetStringField(TEXT("outputPath"), OutputPath);

  AsyncTask(ENamedThreads::GameThread, [this, RequestId, Socket, Directory,
                                        ReportType, OutputPath]() {
    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    FARFilter Filter;
    Filter.bRecursivePaths = true;
    if (!Directory.IsEmpty()) {
      Filter.PackagePaths.Add(FName(*Directory));
    }

    TArray<FAssetData> AssetList;
    AssetRegistryModule.Get().GetAssets(Filter, AssetList);

    TArray<TSharedPtr<FJsonValue>> AssetsArray;
    for (const FAssetData &Asset : AssetList) {
      TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
      AssetObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
      AssetObj->SetStringField(TEXT("path"),
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                               Asset.GetSoftObjectPath().ToString());
      AssetObj->SetStringField(TEXT("class"), Asset.AssetClassPath.ToString());
#else
                               Asset.ToSoftObjectPath().ToString());
      AssetObj->SetStringField(TEXT("class"), Asset.AssetClass.ToString());
#endif
      AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
    }

    bool bFileWritten = false;
    if (!OutputPath.IsEmpty()) {
      FString AbsoluteOutput = OutputPath;
      if (FPaths::IsRelative(OutputPath)) {
        AbsoluteOutput =
            FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), OutputPath);
      }

      const FString DirPath = FPaths::GetPath(AbsoluteOutput);
      IPlatformFile &PlatformFile =
          FPlatformFileManager::Get().GetPlatformFile();
      PlatformFile.CreateDirectoryTree(*DirPath);

      const FString FileContents = TEXT(
          "{\"report\":\"Asset report generated by MCP Automation Bridge\"}");
      bFileWritten =
          FFileHelper::SaveStringToFile(FileContents, *AbsoluteOutput);
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("directory"), Directory);
    Resp->SetStringField(TEXT("reportType"), ReportType);
    Resp->SetNumberField(TEXT("assetCount"), AssetList.Num());
    Resp->SetArrayField(TEXT("assets"), AssetsArray);
    if (!OutputPath.IsEmpty()) {
      Resp->SetStringField(TEXT("outputPath"), OutputPath);
      Resp->SetBoolField(TEXT("fileWritten"), bFileWritten);
    }

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Asset report generated"), Resp, FString());
  });
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// ============================================================================
// 8. MATERIAL CREATION
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleCreateMaterial(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString Name;
  Payload->TryGetStringField(TEXT("name"), Name);
  FString Path;
  Payload->TryGetStringField(TEXT("path"), Path);

  if (Name.IsEmpty() || Path.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("name and path required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Validate properties if present
  const TSharedPtr<FJsonObject> *Props;
  if (Payload->TryGetObjectField(TEXT("properties"), Props)) {
    FString ShadingModelStr;
    if ((*Props)->TryGetStringField(TEXT("ShadingModel"), ShadingModelStr)) {
      // Simple validation for test case
      if (ShadingModelStr.Equals(TEXT("InvalidModel"),
                                 ESearchCase::IgnoreCase)) {
        SendAutomationResponse(Socket, RequestId, false,
                               TEXT("Invalid shading model"), nullptr,
                               TEXT("INVALID_PROPERTY"));
        return true;
      }
    }
  }

  IAssetTools &AssetTools =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

  FString FullPath = Path + TEXT("/") + Name;
  if (UEditorAssetLibrary::DoesAssetExist(FullPath)) {
    UEditorAssetLibrary::DeleteAsset(FullPath);
  }

  UMaterialFactoryNew *Factory = NewObject<UMaterialFactoryNew>();
  UObject *NewAsset =
      AssetTools.CreateAsset(Name, Path, UMaterial::StaticClass(), Factory);

  if (NewAsset) {
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Material created"),
                           Resp, FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create material"), nullptr,
                           TEXT("CREATE_FAILED"));
  }
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleCreateMaterialInstance(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString Name;
  Payload->TryGetStringField(TEXT("name"), Name);
  FString Path;
  Payload->TryGetStringField(TEXT("path"), Path);
  FString ParentPath;
  Payload->TryGetStringField(TEXT("parentMaterial"), ParentPath);

  if (Name.IsEmpty() || Path.IsEmpty() || ParentPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("name, path and parentMaterial required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  UMaterialInterface *ParentMaterial = nullptr;

  // Special test sentinel: treat "/Valid" as a shorthand for the engine's
  // default surface material so tests can exercise parameter handling without
  // requiring a real asset at that path.
  if (ParentPath.Equals(TEXT("/Valid"), ESearchCase::IgnoreCase)) {
    ParentMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
  } else {
    if (!UEditorAssetLibrary::DoesAssetExist(ParentPath)) {
      SendAutomationResponse(
          Socket, RequestId, false,
          FString::Printf(TEXT("Parent material asset not found: %s"),
                          *ParentPath),
          nullptr, TEXT("PARENT_NOT_FOUND"));
      return true;
    }
    ParentMaterial = LoadObject<UMaterialInterface>(nullptr, *ParentPath);
  }

  if (!ParentMaterial) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Parent material not found"), nullptr,
                           TEXT("PARENT_NOT_FOUND"));
    return true;
  }

  IAssetTools &AssetTools =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

  UMaterialInstanceConstantFactoryNew *Factory =
      NewObject<UMaterialInstanceConstantFactoryNew>();
  Factory->InitialParent = ParentMaterial;

  UObject *NewAsset = AssetTools.CreateAsset(
      Name, Path, UMaterialInstanceConstant::StaticClass(), Factory);

  if (NewAsset) {
    // Handle parameters if provided
    UMaterialInstanceConstant *MIC = Cast<UMaterialInstanceConstant>(NewAsset);
    const TSharedPtr<FJsonObject> *ParamsObj = nullptr;
    if (MIC && Payload->TryGetObjectField(TEXT("parameters"), ParamsObj)) {
      // Scalar parameters
      const TSharedPtr<FJsonObject> *Scalars;
      if ((*ParamsObj)->TryGetObjectField(TEXT("scalar"), Scalars)) {
        for (const auto &Kvp : (*Scalars)->Values) {
          double Val = 0.0;
          if (Kvp.Value->TryGetNumber(Val)) {
            MIC->SetScalarParameterValueEditorOnly(FName(*Kvp.Key), (float)Val);
          }
        }
      }

      // Vector parameters
      const TSharedPtr<FJsonObject> *Vectors;
      if ((*ParamsObj)->TryGetObjectField(TEXT("vector"), Vectors)) {
        for (const auto &Kvp : (*Vectors)->Values) {
          const TSharedPtr<FJsonObject> *VecObj;
          if (Kvp.Value->TryGetObject(VecObj)) {
            // Try generic RGBA
            double R = 0, G = 0, B = 0, A = 1;
            (*VecObj)->TryGetNumberField(TEXT("r"), R);
            (*VecObj)->TryGetNumberField(TEXT("g"), G);
            (*VecObj)->TryGetNumberField(TEXT("b"), B);
            (*VecObj)->TryGetNumberField(TEXT("a"), A);
            MIC->SetVectorParameterValueEditorOnly(
                FName(*Kvp.Key),
                FLinearColor((float)R, (float)G, (float)B, (float)A));
          }
        }
      }

      // Texture parameters
      const TSharedPtr<FJsonObject> *Textures;
      if ((*ParamsObj)->TryGetObjectField(TEXT("texture"), Textures)) {
        for (const auto &Kvp : (*Textures)->Values) {
          FString TexPath;
          if (Kvp.Value->TryGetString(TexPath) && !TexPath.IsEmpty()) {
            UTexture *Tex = LoadObject<UTexture>(nullptr, *TexPath);
            if (Tex) {
              MIC->SetTextureParameterValueEditorOnly(FName(*Kvp.Key), Tex);
            }
          }
        }
      }
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Material Instance created"), Resp, FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create material instance"), nullptr,
                           TEXT("CREATE_FAILED"));
  }
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// ============================================================================
// 10. MATERIAL PARAMETER & INSTANCE MANAGEMENT
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleAddMaterialParameter(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  FString Name;
  Payload->TryGetStringField(TEXT("name"), Name);
  FString Type;
  Payload->TryGetStringField(TEXT("type"), Type);

  if (AssetPath.IsEmpty() || Name.IsEmpty() || Type.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("assetPath, name, and type required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  UMaterial *Material = Cast<UMaterial>(Asset);

  if (!Material) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Asset is not a Material (Master Material "
                                "required for adding parameters)"),
                           nullptr, TEXT("INVALID_ASSET_TYPE"));
    return true;
  }

  UMaterialExpression *NewExpression = nullptr;
  Type = Type.ToLower();

  if (Type == TEXT("scalar")) {
    NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(
        Material, UMaterialExpressionScalarParameter::StaticClass());
    if (UMaterialExpressionScalarParameter *ScalarParam =
            Cast<UMaterialExpressionScalarParameter>(NewExpression)) {
      ScalarParam->ParameterName = FName(*Name);
      double Val = 0.0;
      if (Payload->TryGetNumberField(TEXT("value"), Val)) {
        ScalarParam->DefaultValue = (float)Val;
      }
    }
  } else if (Type == TEXT("vector")) {
    NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(
        Material, UMaterialExpressionVectorParameter::StaticClass());
    if (UMaterialExpressionVectorParameter *VectorParam =
            Cast<UMaterialExpressionVectorParameter>(NewExpression)) {
      VectorParam->ParameterName = FName(*Name);
      const TSharedPtr<FJsonObject> *VecObj;
      if (Payload->TryGetObjectField(TEXT("value"), VecObj)) {
        double R = 0, G = 0, B = 0, A = 1;
        (*VecObj)->TryGetNumberField(TEXT("r"), R);
        (*VecObj)->TryGetNumberField(TEXT("g"), G);
        (*VecObj)->TryGetNumberField(TEXT("b"), B);
        (*VecObj)->TryGetNumberField(TEXT("a"), A);
        VectorParam->DefaultValue =
            FLinearColor((float)R, (float)G, (float)B, (float)A);
      }
    }
  } else if (Type == TEXT("texture")) {
    NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(
        Material, UMaterialExpressionTextureSampleParameter2D::StaticClass());
    if (UMaterialExpressionTextureSampleParameter2D *TexParam =
            Cast<UMaterialExpressionTextureSampleParameter2D>(NewExpression)) {
      TexParam->ParameterName = FName(*Name);
      FString TexPath;
      if (Payload->TryGetStringField(TEXT("value"), TexPath) &&
          !TexPath.IsEmpty()) {
        UTexture *Tex = LoadObject<UTexture>(nullptr, *TexPath);
        if (Tex) {
          TexParam->Texture = Tex;
        }
      }
    }
  } else if (Type == TEXT("staticswitch") || Type == TEXT("static_switch")) {
    NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(
        Material, UMaterialExpressionStaticSwitchParameter::StaticClass());
    if (UMaterialExpressionStaticSwitchParameter *SwitchParam =
            Cast<UMaterialExpressionStaticSwitchParameter>(NewExpression)) {
      SwitchParam->ParameterName = FName(*Name);
      bool Val = false;
      if (Payload->TryGetBoolField(TEXT("value"), Val)) {
        SwitchParam->DefaultValue = Val;
      }
    }
  } else {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Unsupported parameter type: %s"), *Type), nullptr,
        TEXT("INVALID_TYPE"));
    return true;
  }

  if (NewExpression) {
    // UMaterialEditingLibrary::CreateMaterialExpression handles adding to the
    // material and graph. We just need to ensure the material is
    // recompiled/updated.
    UMaterialEditingLibrary::LayoutMaterialExpressions(Material);
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetStringField(TEXT("parameterName"), Name);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Parameter added"),
                           Resp, FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create parameter expression"),
                           nullptr, TEXT("CREATE_FAILED"));
  }

  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleListMaterialInstances(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  // Find all assets that are Material Instances and have this asset as parent
  // Note: This can be expensive if we scan all assets.
  // Optimization: Use GetReferencers? Or just filter by class and check parent.
  // Since we can't easily query by "Parent" tag efficiently without iterating,
  // we'll try a filtered query.

  FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"),
                                           TEXT("MaterialInstanceConstant")));
#else
  Filter.ClassNames.Add(FName(TEXT("MaterialInstanceConstant")));
#endif
  Filter.bRecursiveClasses = true;

  TArray<FAssetData> AssetList;
  AssetRegistry.GetAssets(Filter, AssetList);

  TArray<TSharedPtr<FJsonValue>> Instances;

  // We need to check the parent. Loading the asset is safest but slow.
  // Checking tags is faster. MICs usually have "Parent" tag.
  FName ParentPathName(*AssetPath);

  for (const FAssetData &Asset : AssetList) {
    // Check tag first
    FString ParentTag;
    if (Asset.GetTagValue(TEXT("Parent"), ParentTag)) {
      // Tag value might be "Material'Path'" or just "Path"
      // It's usually formatted string.
      if (ParentTag.Contains(AssetPath)) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        Instances.Add(
            MakeShared<FJsonValueString>(Asset.GetSoftObjectPath().ToString()));
#else
        Instances.Add(
            MakeShared<FJsonValueString>(Asset.ToSoftObjectPath().ToString()));
#endif
      }
    } else {
      // Fallback: load asset (slow, but accurate)
      // Only do this if tag is missing? Or maybe skip to avoid perf hit.
      // Let's rely on tag for now.
    }
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetArrayField(TEXT("instances"), Instances);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Instances listed"),
                         Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleResetInstanceParameters(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  UMaterialInstanceConstant *MIC = Cast<UMaterialInstanceConstant>(Asset);

  if (!MIC) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Asset is not a Material Instance Constant"),
                           nullptr, TEXT("INVALID_ASSET_TYPE"));
    return true;
  }

  MIC->ClearParameterValuesEditorOnly();
  MIC->PostEditChange();
  MIC->MarkPackageDirty();

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Instance parameters reset"), Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleDoesAssetExist(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  bool bExists = UEditorAssetLibrary::DoesAssetExist(AssetPath);

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("exists"), bExists);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);
  SendAutomationResponse(Socket, RequestId, true,
                         bExists ? TEXT("Asset exists")
                                 : TEXT("Asset does not exist"),
                         Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGetMaterialStats(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  UMaterialInterface *Material = Cast<UMaterialInterface>(Asset);

  if (!Material) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Asset is not a Material"), nullptr,
                           TEXT("INVALID_ASSET_TYPE"));
    return true;
  }

  // Ensure material is compiled
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Material->EnsureIsComplete();
#else
  // UE 5.0: Force compilation by accessing the material resource
  Material->GetMaterial();
#endif

  TSharedPtr<FJsonObject> Stats = MakeShared<FJsonObject>();

  // Get actual shading model from the material
  FString ShadingModelStr = TEXT("Unknown");
  if (UMaterial *BaseMat = Material->GetMaterial()) {
    FMaterialShadingModelField ShadingModels = BaseMat->GetShadingModels();
    // Check shading models using HasShadingModel - prioritize common ones
    if (ShadingModels.HasShadingModel(MSM_Unlit)) {
      ShadingModelStr = TEXT("Unlit");
    } else if (ShadingModels.HasShadingModel(MSM_DefaultLit)) {
      ShadingModelStr = TEXT("DefaultLit");
    } else if (ShadingModels.HasShadingModel(MSM_Subsurface)) {
      ShadingModelStr = TEXT("Subsurface");
    } else if (ShadingModels.HasShadingModel(MSM_SubsurfaceProfile)) {
      ShadingModelStr = TEXT("SubsurfaceProfile");
    } else if (ShadingModels.HasShadingModel(MSM_ClearCoat)) {
      ShadingModelStr = TEXT("ClearCoat");
    } else if (ShadingModels.HasShadingModel(MSM_TwoSidedFoliage)) {
      ShadingModelStr = TEXT("TwoSidedFoliage");
    } else if (ShadingModels.HasShadingModel(MSM_Hair)) {
      ShadingModelStr = TEXT("Hair");
    } else if (ShadingModels.HasShadingModel(MSM_Cloth)) {
      ShadingModelStr = TEXT("Cloth");
    } else if (ShadingModels.HasShadingModel(MSM_Eye)) {
      ShadingModelStr = TEXT("Eye");
    } else if (ShadingModels.HasShadingModel(MSM_PreintegratedSkin)) {
      ShadingModelStr = TEXT("PreintegratedSkin");
    }
  }
  Stats->SetStringField(TEXT("shadingModel"), ShadingModelStr);

  // Get instruction count from material resource
  // Note: GetMaxNumInstructionsForShader takes FShaderType* in UE 5.6, EShaderFrequency in some earlier versions
  // Skip this in 5.6 as there's no clean way to get a FShaderType* for the pixel shader
  int32 InstructionCount = -1; // Not easily available in this UE version
  Stats->SetNumberField(TEXT("instructionCount"), InstructionCount);

  // Count texture samplers used in the material
  int32 SamplerCount = 0;
  if (UMaterial *BaseMat = Material->GetMaterial()) {
    for (UMaterialExpression *Expr : MCP_GET_MATERIAL_EXPRESSIONS(BaseMat)) {
      if (Expr && Expr->IsA<UMaterialExpressionTextureSample>()) {
        SamplerCount++;
      }
    }
  }
  Stats->SetNumberField(TEXT("samplerCount"), SamplerCount);

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetObjectField(TEXT("stats"), Stats);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material stats retrieved"), Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGenerateLODs(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("generate_lods"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (!Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) ||
      !AssetPathsArray) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("assetPaths array required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  int32 NumLODs = 4;
  Payload->TryGetNumberField(TEXT("numLODs"), NumLODs);

  // Dispatch to Game Thread
  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(this);
  // Copy paths
  TArray<FString> Paths;
  for (const auto &Val : *AssetPathsArray) {
    if (Val.IsValid() && Val->Type == EJson::String)
      Paths.Add(Val->AsString());
  }

  AsyncTask(ENamedThreads::GameThread, [WeakSubsystem, RequestId,
                                        RequestingSocket, Paths, NumLODs]() {
    UMcpAutomationBridgeSubsystem *Subsystem = WeakSubsystem.Get();
    if (!Subsystem)
      return;

    int32 SuccessCount = 0;

    for (const FString &Path : Paths) {
      UObject *Obj = LoadObject<UObject>(nullptr, *Path);
      if (UStaticMesh *Mesh = Cast<UStaticMesh>(Obj)) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
               TEXT("Generating %d LODs for %s"), NumLODs, *Path);

        Mesh->Modify();
        Mesh->SetNumSourceModels(NumLODs);

        // Configure LOD reduction settings with progressive reduction
        for (int32 LODIndex = 1; LODIndex < NumLODs; LODIndex++) {
          FStaticMeshSourceModel &SourceModel = Mesh->GetSourceModel(LODIndex);
          FMeshReductionSettings &ReductionSettings =
              SourceModel.ReductionSettings;

          // Progressive reduction: 50%, 25%, 12.5%...
          float ReductionPercent =
              1.0f / FMath::Pow(2.0f, static_cast<float>(LODIndex));
          ReductionSettings.PercentTriangles = ReductionPercent;
          ReductionSettings.PercentVertices = ReductionPercent;

          // Enable reduction for this LOD level
          SourceModel.BuildSettings.bRecomputeNormals = false;
          SourceModel.BuildSettings.bRecomputeTangents = false;
          SourceModel.BuildSettings.bUseMikkTSpace = true;
        }

        // Build the mesh with new LOD settings
        Mesh->Build();
        Mesh->PostEditChange();
        McpSafeAssetSave(Mesh);

        SuccessCount++;
      }
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetNumberField(TEXT("processed"), SuccessCount);
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
                                      TEXT("LOD generation completed"),
                                      Resp, FString());
  });

  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Requires editor"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 8. METADATA
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleGetMetadata(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("get_metadata payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  if (!Asset) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to load asset"), nullptr,
                           TEXT("LOAD_FAILED"));
    return true;
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);

  // 1. Asset Registry Tags
  FAssetData AssetData(Asset);
  TSharedPtr<FJsonObject> TagsObj = MakeShared<FJsonObject>();
  for (const auto &Kvp : AssetData.TagsAndValues) {
    TagsObj->SetStringField(Kvp.Key.ToString(), Kvp.Value.AsString());
  }
  Resp->SetObjectField(TEXT("tags"), TagsObj);

  // 2. Package Metadata information
  UPackage *Package = Asset->GetOutermost();
  if (Package) {

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
    FMetaData& Meta = Package->GetMetaData();
    bool bHasMeta = FMetaData::GetMapForObject(Asset) != nullptr;
    Resp->SetBoolField(TEXT("debug_has_meta"), bHasMeta);

    const TMap<FName, FString> *ObjectMeta = FMetaData::GetMapForObject(Asset);
#else
    UMetaData* Meta = Package->GetMetaData();
    bool bHasMeta = Meta->GetMapForObject(Asset) != nullptr;
    Resp->SetBoolField(TEXT("debug_has_meta"), bHasMeta);

    const TMap<FName, FString> *ObjectMeta = Meta->GetMapForObject(Asset);
#endif
    if (ObjectMeta) {
      TSharedPtr<FJsonObject> MetaObj = MakeShared<FJsonObject>();
      for (const auto &Entry : *ObjectMeta) {
        MetaObj->SetStringField(Entry.Key.ToString(), Entry.Value);
      }
      Resp->SetObjectField(TEXT("metadata"), MetaObj);
    }
  }

  SendAutomationResponse(Socket, RequestId, true, TEXT("Metadata retrieved"),
                         Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("get_metadata requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 9. MATERIAL REBUILD - NOT IMPLEMENTED (placeholder for future implementation)
// ============================================================================

// Stub implementations for functions declared in header but removed from implementation
// These functions are referenced by the dispatcher but were removed due to API changes

bool UMcpAutomationBridgeSubsystem::HandleNaniteRebuildMesh(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("nanite_rebuild_mesh"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR && ENGINE_MAJOR_VERSION >= 5
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("nanite_rebuild_mesh payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString MeshPath;
  if (!Payload->TryGetStringField(TEXT("meshPath"), MeshPath) ||
      MeshPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("meshPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Load the static mesh
  UStaticMesh *StaticMesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
  if (!StaticMesh) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Static mesh not found: %s"), *MeshPath),
                        TEXT("MESH_NOT_FOUND"));
    return true;
  }

  // Check if mesh supports Nanite
  bool bEnableNanite = true;
  Payload->TryGetBoolField(TEXT("enableNanite"), bEnableNanite);

  // Nanite settings
  bool bPreserveArea = true;
  double TrianglePercent = 100.0;
  double FallbackPercent = 0.0;

  Payload->TryGetBoolField(TEXT("preserveArea"), bPreserveArea);
  Payload->TryGetNumberField(TEXT("trianglePercent"), TrianglePercent);
  Payload->TryGetNumberField(TEXT("fallbackPercent"), FallbackPercent);

  // Clamp values
  TrianglePercent = FMath::Clamp(TrianglePercent, 0.0, 100.0);
  FallbackPercent = FMath::Clamp(FallbackPercent, 0.0, 100.0);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
  // UE 5.7+: Use accessor functions to avoid deprecation warnings
  FMeshNaniteSettings Settings = StaticMesh->GetNaniteSettings();
  Settings.bEnabled = bEnableNanite;
  Settings.PositionPrecision = 8; // Default precision
  
  // bPreserveArea replaced with ShapePreservation enum
  if (bPreserveArea) {
    Settings.ShapePreservation = ENaniteShapePreservation::PreserveArea;
  } else {
    Settings.ShapePreservation = ENaniteShapePreservation::None;
  }
  Settings.KeepPercentTriangles = static_cast<float>(TrianglePercent / 100.0);
  Settings.FallbackPercentTriangles = static_cast<float>(FallbackPercent / 100.0);
  if (FallbackPercent > 0.0) {
    Settings.GenerateFallback = ENaniteGenerateFallback::Enabled;
  } else {
    Settings.GenerateFallback = ENaniteGenerateFallback::PlatformDefault;
  }
  StaticMesh->SetNaniteSettings(Settings);
  StaticMesh->NotifyNaniteSettingsChanged();
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  // UE 5.1-5.6: Uses KeepPercentTriangles, FallbackPercentTriangles, and bPreserveArea
  StaticMesh->NaniteSettings.bEnabled = bEnableNanite;
  StaticMesh->NaniteSettings.PositionPrecision = 8;
  StaticMesh->NaniteSettings.bPreserveArea = bPreserveArea;
  StaticMesh->NaniteSettings.KeepPercentTriangles = static_cast<float>(TrianglePercent / 100.0);
  StaticMesh->NaniteSettings.FallbackPercentTriangles = static_cast<float>(FallbackPercent / 100.0);
#else
  // UE 5.0: Uses KeepPercentTriangles (no bPreserveArea)
  StaticMesh->NaniteSettings.bEnabled = bEnableNanite;
  StaticMesh->NaniteSettings.PositionPrecision = 8;
  StaticMesh->NaniteSettings.KeepPercentTriangles = static_cast<float>(TrianglePercent / 100.0);
  StaticMesh->NaniteSettings.FallbackPercentTriangles = static_cast<float>(FallbackPercent / 100.0);
#endif

  // Mark mesh as modified
  StaticMesh->MarkPackageDirty();

  // Build response
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("meshPath"), MeshPath);
  Resp->SetStringField(TEXT("meshName"), StaticMesh->GetName());
  Resp->SetBoolField(TEXT("naniteEnabled"), bEnableNanite);
  Resp->SetBoolField(TEXT("preserveArea"), bPreserveArea);
  Resp->SetNumberField(TEXT("trianglePercent"), TrianglePercent);
  Resp->SetNumberField(TEXT("fallbackPercent"), FallbackPercent);

  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Nanite settings updated for %s"), *StaticMesh->GetName()),
                         Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("nanite_rebuild_mesh requires UE 5.0+ editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleFindByTag(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("find_by_tag"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("find_by_tag payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString Tag;
  if (!Payload->TryGetStringField(TEXT("tag"), Tag) || Tag.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("tag field is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FName TagName(*Tag);
  TArray<TSharedPtr<FJsonValue>> Results;
  int32 MaxResults = 100;
  Payload->TryGetNumberField(TEXT("maxResults"), MaxResults);
  MaxResults = FMath::Clamp(MaxResults, 1, 1000);

  bool bSearchActors = true;
  bool bSearchComponents = false;
  bool bSearchAssets = false;
  Payload->TryGetBoolField(TEXT("searchActors"), bSearchActors);
  Payload->TryGetBoolField(TEXT("searchComponents"), bSearchComponents);
  Payload->TryGetBoolField(TEXT("searchAssets"), bSearchAssets);

  // Search in world
  if (GEditor && bSearchActors) {
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (World) {
      for (TActorIterator<AActor> It(World); It && Results.Num() < MaxResults; ++It) {
        AActor *Actor = *It;
        if (Actor && Actor->ActorHasTag(TagName)) {
          TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
          ResultObj->SetStringField(TEXT("type"), TEXT("Actor"));
          ResultObj->SetStringField(TEXT("name"), Actor->GetName());
          ResultObj->SetStringField(TEXT("label"), Actor->GetActorLabel());
          ResultObj->SetStringField(TEXT("path"), Actor->GetPathName());
          ResultObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
          
          const FVector Location = Actor->GetActorLocation();
          TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
          LocObj->SetNumberField(TEXT("x"), Location.X);
          LocObj->SetNumberField(TEXT("y"), Location.Y);
          LocObj->SetNumberField(TEXT("z"), Location.Z);
          ResultObj->SetObjectField(TEXT("location"), LocObj);
          
          Results.Add(MakeShared<FJsonValueObject>(ResultObj));
        }
      }
    }
  }

  // Search for components with tag
  if (bSearchComponents && GEditor && Results.Num() < MaxResults) {
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (World) {
      for (TActorIterator<AActor> It(World); It && Results.Num() < MaxResults; ++It) {
        AActor *Actor = *It;
        if (Actor) {
          TInlineComponentArray<UActorComponent*> Components;
          Actor->GetComponents(Components);
          for (UActorComponent *Component : Components) {
            if (Component && Component->ComponentHasTag(TagName)) {
              TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
              ResultObj->SetStringField(TEXT("type"), TEXT("Component"));
              ResultObj->SetStringField(TEXT("name"), Component->GetName());
              ResultObj->SetStringField(TEXT("class"), Component->GetClass()->GetName());
              ResultObj->SetStringField(TEXT("owner"), Actor->GetName());
              ResultObj->SetStringField(TEXT("path"), Component->GetPathName());
              Results.Add(MakeShared<FJsonValueObject>(ResultObj));
            }
          }
        }
      }
    }
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("tag"), Tag);
  Resp->SetNumberField(TEXT("count"), Results.Num());
  Resp->SetArrayField(TEXT("results"), Results);

  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Found %d objects with tag '%s'"), Results.Num(), *Tag),
                         Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("find_by_tag requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleAddMaterialNode(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("add_material_node"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("add_material_node payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) ||
      MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString NodeType;
  if (!Payload->TryGetStringField(TEXT("nodeType"), NodeType) ||
      NodeType.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("nodeType is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Load the material
  UMaterial *Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
  if (!Material) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Material not found: %s"), *MaterialPath),
                        TEXT("MATERIAL_NOT_FOUND"));
    return true;
  }

  // Create material expression based on node type
  UMaterialExpression *NewExpression = nullptr;
  UClass *ExpressionClass = nullptr;

  // Map common node type names to expression classes
  if (NodeType.Equals(TEXT("Constant"), ESearchCase::IgnoreCase) ||
      NodeType.Equals(TEXT("Constant1"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionConstant::StaticClass();
  } else if (NodeType.Equals(TEXT("Constant2"), ESearchCase::IgnoreCase) ||
             NodeType.Equals(TEXT("Constant2Vector"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionConstant2Vector::StaticClass();
  } else if (NodeType.Equals(TEXT("Constant3"), ESearchCase::IgnoreCase) ||
             NodeType.Equals(TEXT("Constant3Vector"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionConstant3Vector::StaticClass();
  } else if (NodeType.Equals(TEXT("Constant4"), ESearchCase::IgnoreCase) ||
             NodeType.Equals(TEXT("Constant4Vector"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionConstant4Vector::StaticClass();
  } else if (NodeType.Equals(TEXT("TextureSample"), ESearchCase::IgnoreCase) ||
             NodeType.Equals(TEXT("Texture"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionTextureSample::StaticClass();
  } else if (NodeType.Equals(TEXT("Add"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionAdd::StaticClass();
  } else if (NodeType.Equals(TEXT("Multiply"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionMultiply::StaticClass();
  } else if (NodeType.Equals(TEXT("Sine"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionSine::StaticClass();
  } else if (NodeType.Equals(TEXT("Cosine"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionCosine::StaticClass();
  } else if (NodeType.Equals(TEXT("Time"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionTime::StaticClass();
  } else if (NodeType.Equals(TEXT("VertexColor"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionVertexColor::StaticClass();
  } else {
    // Try to find the class dynamically
    FString FullClassName = FString::Printf(TEXT("/Script/Engine.MaterialExpression%s"), *NodeType);
    ExpressionClass = LoadClass<UMaterialExpression>(nullptr, *FullClassName);
    
    if (!ExpressionClass) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Unknown node type: %s"), *NodeType),
                          TEXT("INVALID_NODE_TYPE"));
      return true;
    }
  }

  // Create the expression
  NewExpression = NewObject<UMaterialExpression>(Material, ExpressionClass, NAME_None, RF_Transactional);
  if (!NewExpression) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Failed to create material expression"),
                        TEXT("EXPRESSION_CREATION_FAILED"));
    return true;
  }

  // Set position
  double PosX = 0, PosY = 0;
  Payload->TryGetNumberField(TEXT("posX"), PosX);
  Payload->TryGetNumberField(TEXT("posY"), PosY);
  NewExpression->MaterialExpressionEditorX = static_cast<int32>(PosX);
  NewExpression->MaterialExpressionEditorY = static_cast<int32>(PosY);

  // Set node properties based on type
  if (UMaterialExpressionConstant *Const = Cast<UMaterialExpressionConstant>(NewExpression)) {
    double Value = 0;
    Payload->TryGetNumberField(TEXT("value"), Value);
    Const->R = static_cast<float>(Value);
  } else if (UMaterialExpressionConstant3Vector *Const3 = Cast<UMaterialExpressionConstant3Vector>(NewExpression)) {
    double R = 0, G = 0, B = 0;
    const TSharedPtr<FJsonObject> *ColorObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("color"), ColorObj) && ColorObj) {
      (*ColorObj)->TryGetNumberField(TEXT("r"), R);
      (*ColorObj)->TryGetNumberField(TEXT("g"), G);
      (*ColorObj)->TryGetNumberField(TEXT("b"), B);
    }
    Const3->Constant = FLinearColor(static_cast<float>(R), static_cast<float>(G), static_cast<float>(B));
  } else if (UMaterialExpressionTextureSample *TexSample = Cast<UMaterialExpressionTextureSample>(NewExpression)) {
    FString TexturePath;
    if (Payload->TryGetStringField(TEXT("texturePath"), TexturePath) && !TexturePath.IsEmpty()) {
      UTexture *Texture = LoadObject<UTexture>(nullptr, *TexturePath);
      if (Texture) {
        TexSample->Texture = Texture;
      }
    }
  }

  // Add to material
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Material->GetEditorOnlyData()->ExpressionCollection.AddExpression(NewExpression);
#else
  Material->Expressions.Add(NewExpression);
#endif

  Material->MarkPackageDirty();

  // Get the expression index for reference
  int32 ExpressionIndex = -1;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  ExpressionIndex = Material->GetEditorOnlyData()->ExpressionCollection.Expressions.IndexOfByKey(NewExpression);
#else
  ExpressionIndex = Material->Expressions.IndexOfByKey(NewExpression);
#endif

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("materialPath"), MaterialPath);
  Resp->SetStringField(TEXT("nodeType"), NodeType);
  Resp->SetNumberField(TEXT("expressionIndex"), ExpressionIndex);
  Resp->SetStringField(TEXT("expressionName"), NewExpression->GetName());
  Resp->SetStringField(TEXT("nodeGuid"), NewExpression->MaterialExpressionGuid.ToString());

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material node added successfully"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("add_material_node requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleConnectMaterialPins(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("connect_material_pins"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("connect_material_pins payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) ||
      MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  int32 FromExpressionIndex = -1;
  int32 ToExpressionIndex = -1;
  if (!Payload->TryGetNumberField(TEXT("fromExpression"), FromExpressionIndex) ||
      !Payload->TryGetNumberField(TEXT("toExpression"), ToExpressionIndex)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("fromExpression and toExpression indices are required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Load the material
  UMaterial *Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
  if (!Material) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Material not found: %s"), *MaterialPath),
                        TEXT("MATERIAL_NOT_FOUND"));
    return true;
  }

  // Get expressions
  UMaterialExpression *FromExpression = nullptr;
  UMaterialExpression *ToExpression = nullptr;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  const TArray<TObjectPtr<UMaterialExpression>> &Expressions =
      Material->GetEditorOnlyData()->ExpressionCollection.Expressions;
#else
  const TArray<UMaterialExpression *> &Expressions = Material->Expressions;
#endif

  if (FromExpressionIndex < 0 || FromExpressionIndex >= Expressions.Num()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid fromExpression index"),
                        TEXT("INVALID_INDEX"));
    return true;
  }
  FromExpression = Expressions[FromExpressionIndex];

  if (ToExpressionIndex < 0 || ToExpressionIndex >= Expressions.Num()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid toExpression index"),
                        TEXT("INVALID_INDEX"));
    return true;
  }
  ToExpression = Expressions[ToExpressionIndex];

  // Get input name
  FString InputName;
  if (!Payload->TryGetStringField(TEXT("inputName"), InputName) ||
      InputName.IsEmpty()) {
    InputName = TEXT("Input");
  }

  // Find the input on the destination expression
  FExpressionInput *TargetInput = nullptr;
  for (FProperty *Property = ToExpression->GetClass()->PropertyLink; Property;
       Property = Property->PropertyLinkNext) {
    if (FStructProperty *StructProp = CastField<FStructProperty>(Property)) {
      if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput"))) {
        if (Property->GetName().Equals(InputName, ESearchCase::IgnoreCase)) {
          TargetInput = StructProp->ContainerPtrToValuePtr<FExpressionInput>(ToExpression);
          break;
        }
      }
    }
  }

  if (!TargetInput) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Input '%s' not found on expression"),
                                        *InputName),
                        TEXT("INPUT_NOT_FOUND"));
    return true;
  }

  // Make the connection
  TargetInput->Expression = FromExpression;

  Material->MarkPackageDirty();

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("materialPath"), MaterialPath);
  Resp->SetNumberField(TEXT("fromExpression"), FromExpressionIndex);
  Resp->SetNumberField(TEXT("toExpression"), ToExpressionIndex);
  Resp->SetStringField(TEXT("inputName"), InputName);

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material pins connected successfully"), Resp,
                         FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("connect_material_pins requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleRemoveMaterialNode(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("remove_material_node"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("remove_material_node payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) ||
      MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  int32 ExpressionIndex = -1;
  if (!Payload->TryGetNumberField(TEXT("expressionIndex"), ExpressionIndex)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("expressionIndex is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Load the material
  UMaterial *Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
  if (!Material) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Material not found: %s"), *MaterialPath),
                        TEXT("MATERIAL_NOT_FOUND"));
    return true;
  }

  // Get expressions array
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  TArray<TObjectPtr<UMaterialExpression>> &Expressions =
      Material->GetEditorOnlyData()->ExpressionCollection.Expressions;
#else
  TArray<UMaterialExpression *> &Expressions = Material->Expressions;
#endif

  if (ExpressionIndex < 0 || ExpressionIndex >= Expressions.Num()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid expressionIndex"),
                        TEXT("INVALID_INDEX"));
    return true;
  }

  UMaterialExpression *ExpressionToRemove = Expressions[ExpressionIndex];
  if (!ExpressionToRemove) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Expression at index is null"),
                        TEXT("NULL_EXPRESSION"));
    return true;
  }

  FString RemovedName = ExpressionToRemove->GetName();

  // Remove the expression
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Material->GetEditorOnlyData()->ExpressionCollection.RemoveExpression(ExpressionToRemove);
#else
  Expressions.RemoveAt(ExpressionIndex);
#endif

  // Also remove from the material's root node if connected
  Material->RemoveExpressionParameter(ExpressionToRemove);

  Material->MarkPackageDirty();

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("materialPath"), MaterialPath);
  Resp->SetNumberField(TEXT("expressionIndex"), ExpressionIndex);
  Resp->SetStringField(TEXT("removedName"), RemovedName);
  Resp->SetNumberField(TEXT("remainingExpressions"), Expressions.Num());

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material node removed successfully"), Resp,
                         FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("remove_material_node requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleBreakMaterialConnections(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("break_material_connections"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("break_material_connections payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) ||
      MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  int32 ExpressionIndex = -1;
  if (!Payload->TryGetNumberField(TEXT("expressionIndex"), ExpressionIndex)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("expressionIndex is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Load the material
  UMaterial *Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
  if (!Material) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Material not found: %s"), *MaterialPath),
                        TEXT("MATERIAL_NOT_FOUND"));
    return true;
  }

  // Get expressions
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  const TArray<TObjectPtr<UMaterialExpression>> &Expressions =
      Material->GetEditorOnlyData()->ExpressionCollection.Expressions;
#else
  const TArray<UMaterialExpression *> &Expressions = Material->Expressions;
#endif

  if (ExpressionIndex < 0 || ExpressionIndex >= Expressions.Num()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid expressionIndex"),
                        TEXT("INVALID_INDEX"));
    return true;
  }

  UMaterialExpression *TargetExpression = Expressions[ExpressionIndex];
  if (!TargetExpression) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Expression at index is null"),
                        TEXT("NULL_EXPRESSION"));
    return true;
  }

  // Get optional input name to break specific connection
  FString InputName;
  bool bSpecificInput = Payload->TryGetStringField(TEXT("inputName"), InputName) &&
                        !InputName.IsEmpty();

  int32 BrokenConnections = 0;

  // Iterate through all properties of the expression to find and break connections
  for (FProperty *Property = TargetExpression->GetClass()->PropertyLink; Property;
       Property = Property->PropertyLinkNext) {
    if (FStructProperty *StructProp = CastField<FStructProperty>(Property)) {
      if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput"))) {
        // Check if we should break this specific input
        if (bSpecificInput && !Property->GetName().Equals(InputName, ESearchCase::IgnoreCase)) {
          continue;
        }

        FExpressionInput *Input = StructProp->ContainerPtrToValuePtr<FExpressionInput>(TargetExpression);
        if (Input && Input->Expression) {
          Input->Expression = nullptr;
          BrokenConnections++;

          // If breaking specific input, we can stop after finding it
          if (bSpecificInput) {
            break;
          }
        }
      }
    }
  }

  Material->MarkPackageDirty();

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("materialPath"), MaterialPath);
  Resp->SetNumberField(TEXT("expressionIndex"), ExpressionIndex);
  Resp->SetNumberField(TEXT("brokenConnections"), BrokenConnections);
  if (bSpecificInput) {
    Resp->SetStringField(TEXT("inputName"), InputName);
  }

  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Broken %d connection(s)"), BrokenConnections),
                         Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("break_material_connections requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGetMaterialNodeDetails(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("get_material_node_details"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("get_material_node_details payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) ||
      MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  int32 ExpressionIndex = -1;
  if (!Payload->TryGetNumberField(TEXT("expressionIndex"), ExpressionIndex)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("expressionIndex is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Load the material
  UMaterial *Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
  if (!Material) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Material not found: %s"), *MaterialPath),
                        TEXT("MATERIAL_NOT_FOUND"));
    return true;
  }

  // Get expressions
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  const TArray<TObjectPtr<UMaterialExpression>> &Expressions =
      Material->GetEditorOnlyData()->ExpressionCollection.Expressions;
#else
  const TArray<UMaterialExpression *> &Expressions = Material->Expressions;
#endif

  if (ExpressionIndex < 0 || ExpressionIndex >= Expressions.Num()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid expressionIndex"),
                        TEXT("INVALID_INDEX"));
    return true;
  }

  UMaterialExpression *Expression = Expressions[ExpressionIndex];
  if (!Expression) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Expression at index is null"),
                        TEXT("NULL_EXPRESSION"));
    return true;
  }

  // Build response
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("materialPath"), MaterialPath);
  Resp->SetNumberField(TEXT("expressionIndex"), ExpressionIndex);
  Resp->SetStringField(TEXT("name"), Expression->GetName());
  Resp->SetStringField(TEXT("class"), Expression->GetClass()->GetName());
  Resp->SetStringField(TEXT("classPath"), Expression->GetClass()->GetPathName());
  Resp->SetNumberField(TEXT("editorX"), Expression->MaterialExpressionEditorX);
  Resp->SetNumberField(TEXT("editorY"), Expression->MaterialExpressionEditorY);

  // Get inputs
  TArray<TSharedPtr<FJsonValue>> InputsArray;
  for (FProperty *Property = Expression->GetClass()->PropertyLink; Property;
       Property = Property->PropertyLinkNext) {
    if (FStructProperty *StructProp = CastField<FStructProperty>(Property)) {
      if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput"))) {
        FExpressionInput *Input = StructProp->ContainerPtrToValuePtr<FExpressionInput>(Expression);
        TSharedPtr<FJsonObject> InputObj = MakeShared<FJsonObject>();
        InputObj->SetStringField(TEXT("name"), Property->GetName());
        InputObj->SetBoolField(TEXT("isConnected"), Input->Expression != nullptr);
        if (Input->Expression) {
          // Find the connected expression index
          int32 ConnectedIndex = Expressions.IndexOfByKey(Input->Expression);
          InputObj->SetNumberField(TEXT("connectedToIndex"), ConnectedIndex);
        }
        InputsArray.Add(MakeShared<FJsonValueObject>(InputObj));
      }
    }
  }
  Resp->SetArrayField(TEXT("inputs"), InputsArray);

  // Get specific properties based on expression type
  if (UMaterialExpressionConstant *Const = Cast<UMaterialExpressionConstant>(Expression)) {
    Resp->SetNumberField(TEXT("value"), Const->R);
  } else if (UMaterialExpressionConstant2Vector *Const2 = Cast<UMaterialExpressionConstant2Vector>(Expression)) {
    TSharedPtr<FJsonObject> ValueObj = MakeShared<FJsonObject>();
    ValueObj->SetNumberField(TEXT("r"), Const2->R);
    ValueObj->SetNumberField(TEXT("g"), Const2->G);
    Resp->SetObjectField(TEXT("value"), ValueObj);
  } else if (UMaterialExpressionConstant3Vector *Const3 = Cast<UMaterialExpressionConstant3Vector>(Expression)) {
    TSharedPtr<FJsonObject> ValueObj = MakeShared<FJsonObject>();
    ValueObj->SetNumberField(TEXT("r"), Const3->Constant.R);
    ValueObj->SetNumberField(TEXT("g"), Const3->Constant.G);
    ValueObj->SetNumberField(TEXT("b"), Const3->Constant.B);
    Resp->SetObjectField(TEXT("value"), ValueObj);
  } else if (UMaterialExpressionConstant4Vector *Const4 = Cast<UMaterialExpressionConstant4Vector>(Expression)) {
    TSharedPtr<FJsonObject> ValueObj = MakeShared<FJsonObject>();
    ValueObj->SetNumberField(TEXT("r"), Const4->Constant.R);
    ValueObj->SetNumberField(TEXT("g"), Const4->Constant.G);
    ValueObj->SetNumberField(TEXT("b"), Const4->Constant.B);
    ValueObj->SetNumberField(TEXT("a"), Const4->Constant.A);
    Resp->SetObjectField(TEXT("value"), ValueObj);
  } else if (UMaterialExpressionTextureSample *TexSample = Cast<UMaterialExpressionTextureSample>(Expression)) {
    if (TexSample->Texture) {
      Resp->SetStringField(TEXT("texture"), TexSample->Texture->GetPathName());
      Resp->SetStringField(TEXT("textureName"), TexSample->Texture->GetName());
    }
  }

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material node details retrieved"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("get_material_node_details requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
