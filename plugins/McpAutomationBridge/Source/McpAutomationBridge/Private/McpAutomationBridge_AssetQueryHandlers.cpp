#include "AssetRegistry/ARFilter.h"
#include "Dom/JsonObject.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "EditorAssetLibrary.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlOperations.h"
#endif

/**
 * @brief Handles "asset_query" actions from a websocket request and sends a JSON response or error back.
 *
 * Processes subActions such as "get_dependencies", "find_by_tag", "search_assets", and (editor-only)
 * "get_source_control_state", and sends the corresponding success or error response over the provided websocket.
 *
 * @param RequestId Identifier for the incoming request; echoed in responses.
 * @param Action Top-level action name (function only handles when this equals "asset_query").
 * @param Payload JSON payload containing the subAction and its parameters.
 * @param RequestingSocket Websocket to which the response or error will be sent.
 * @return true if the function handled the request (either processed a subAction or sent an error response),
 *         false if the Action did not match "asset_query" and the request was not handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleAssetQueryAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("asset_query"), ESearchCase::IgnoreCase))
    return false;

  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));

  if (SubAction == TEXT("get_dependencies")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    bool bRecursive = false;
    Payload->TryGetBoolField(TEXT("recursive"), bRecursive);

    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            "AssetRegistry");
    TArray<FName> Dependencies;
    UE::AssetRegistry::EDependencyQuery Query =
        bRecursive ? UE::AssetRegistry::EDependencyQuery::Hard
                   : UE::AssetRegistry::EDependencyQuery::Hard; // Simplified

    AssetRegistryModule.Get().GetDependencies(
        FName(*AssetPath), Dependencies,
        UE::AssetRegistry::EDependencyCategory::Package, Query);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> DepArray;
    for (const FName &Dep : Dependencies) {
      DepArray.Add(MakeShared<FJsonValueString>(Dep.ToString()));
    }
    Result->SetArrayField(TEXT("dependencies"), DepArray);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Dependencies retrieved."), Result);
    return true;
  } else if (SubAction == TEXT("find_by_tag")) {
    FString Tag;
    Payload->TryGetStringField(TEXT("tag"), Tag);
    FString ExpectedValue;
    Payload->TryGetStringField(TEXT("value"), ExpectedValue);

    if (Tag.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("tag required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Optional path filter to narrow search scope
    FString Path;
    Payload->TryGetStringField(TEXT("path"), Path);
    if (Path.IsEmpty()) {
      Path = TEXT("/Game"); // Default search path
    }

    // Get all assets in the specified path
    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            "AssetRegistry");
    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*Path));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> AssetDataList;
    AssetRegistryModule.Get().GetAssets(Filter, AssetDataList);

    // Filter assets by checking their package metadata
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> AssetsArray;

    for (const FAssetData &Data : AssetDataList) {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
      const FString AssetPath = Data.GetSoftObjectPath().ToString();
#else
      const FString AssetPath = Data.ToSoftObjectPath().ToString();
#endif
      UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
      if (!Asset)
        continue;

      // Check if the asset has the metadata tag
      FString MetadataValue =
          UEditorAssetLibrary::GetMetadataTag(Asset, FName(*Tag));

      // If we found metadata, check if it matches expected value (or just
      // existence)
      bool bMatches = !MetadataValue.IsEmpty();
      if (bMatches && !ExpectedValue.IsEmpty()) {
        bMatches = MetadataValue.Equals(ExpectedValue, ESearchCase::IgnoreCase);
      }

      if (bMatches) {
        TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
        AssetObj->SetStringField(TEXT("assetName"), Data.AssetName.ToString());
        AssetObj->SetStringField(TEXT("assetPath"), AssetPath);
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        AssetObj->SetStringField(TEXT("classPath"),
                                 Data.AssetClassPath.ToString());
#else
        AssetObj->SetStringField(TEXT("classPath"),
                                 Data.AssetClass.ToString());
#endif
        AssetObj->SetStringField(TEXT("tagValue"), MetadataValue);
        AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
      }
    }

    Result->SetArrayField(TEXT("assets"), AssetsArray);
    Result->SetNumberField(TEXT("count"), AssetsArray.Num());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Assets found by tag"), Result);
    return true;
  } else if (SubAction == TEXT("search_assets")) {
    FARFilter Filter;

    // Parse Class Names
    const TArray<TSharedPtr<FJsonValue>> *ClassNamesPtr;
    if (Payload->TryGetArrayField(TEXT("classNames"), ClassNamesPtr) &&
        ClassNamesPtr) {
      for (const TSharedPtr<FJsonValue> &Val : *ClassNamesPtr) {
        const FString ClassName = Val->AsString();
        if (!ClassName.IsEmpty()) {
          // Support both full paths and short names
          if (ClassName.Contains(TEXT("/"))) {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
            Filter.ClassPaths.Add(FTopLevelAssetPath(ClassName));
#else
            // UE 5.0: Extract class name from path like "/Script/Engine.Blueprint"
            int32 DotIndex;
            if (ClassName.FindLastChar(TEXT('.'), DotIndex))
            {
              Filter.ClassNames.Add(FName(*ClassName.Mid(DotIndex + 1)));
            }
            else
            {
              Filter.ClassNames.Add(FName(*ClassName));
            }
#endif
          } else {
            // Map common short names to full paths
            if (ClassName.Equals(TEXT("Blueprint"), ESearchCase::IgnoreCase)) {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
              Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"),
                                                       TEXT("Blueprint")));
#else
              Filter.ClassNames.Add(TEXT("Blueprint"));
#endif
            } else if (ClassName.Equals(TEXT("StaticMesh"),
                                        ESearchCase::IgnoreCase)) {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
              Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"),
                                                       TEXT("StaticMesh")));
#else
              Filter.ClassNames.Add(TEXT("StaticMesh"));
#endif
            } else if (ClassName.Equals(TEXT("SkeletalMesh"),
                                        ESearchCase::IgnoreCase)) {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
              Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"),
                                                       TEXT("SkeletalMesh")));
#else
              Filter.ClassNames.Add(TEXT("SkeletalMesh"));
#endif
            } else if (ClassName.Equals(TEXT("Material"),
                                        ESearchCase::IgnoreCase)) {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
              Filter.ClassPaths.Add(
                  FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Material")));
#else
              Filter.ClassNames.Add(TEXT("Material"));
#endif
            } else if (ClassName.Equals(TEXT("MaterialInstance"),
                                        ESearchCase::IgnoreCase) ||
                       ClassName.Equals(TEXT("MaterialInstanceConstant"),
                                        ESearchCase::IgnoreCase)) {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
              Filter.ClassPaths.Add(FTopLevelAssetPath(
                  TEXT("/Script/Engine"), TEXT("MaterialInstanceConstant")));
#else
              Filter.ClassNames.Add(TEXT("MaterialInstanceConstant"));
#endif
            } else if (ClassName.Equals(TEXT("Texture2D"),
                                        ESearchCase::IgnoreCase)) {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
              Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"),
                                                       TEXT("Texture2D")));
#else
              Filter.ClassNames.Add(TEXT("Texture2D"));
#endif
            } else if (ClassName.Equals(TEXT("Level"),
                                        ESearchCase::IgnoreCase) ||
                       ClassName.Equals(TEXT("World"),
                                        ESearchCase::IgnoreCase)) {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
              Filter.ClassPaths.Add(
                  FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("World")));
#else
              Filter.ClassNames.Add(TEXT("World"));
#endif
            } else if (ClassName.Equals(TEXT("SoundCue"),
                                        ESearchCase::IgnoreCase)) {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
              Filter.ClassPaths.Add(
                  FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("SoundCue")));
#else
              Filter.ClassNames.Add(TEXT("SoundCue"));
#endif
            } else if (ClassName.Equals(TEXT("SoundWave"),
                                        ESearchCase::IgnoreCase)) {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
              Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"),
                                                       TEXT("SoundWave")));
#else
              Filter.ClassNames.Add(TEXT("SoundWave"));
#endif
            } else {
              UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                     TEXT("HandleAssetQueryAction: Could not resolve short "
                          "class name '%s' to a TopLevelAssetPath. Please use "
                          "full class path (e.g. /Script/Engine.Blueprint)."),
                     *ClassName);
            }
          }
        }
      }
    }

    // Parse Package Paths
    const TArray<TSharedPtr<FJsonValue>> *PackagePathsPtr;
    if (Payload->TryGetArrayField(TEXT("packagePaths"), PackagePathsPtr) &&
        PackagePathsPtr) {
      for (const TSharedPtr<FJsonValue> &Val : *PackagePathsPtr) {
        Filter.PackagePaths.Add(FName(*Val->AsString()));
      }
    }

    // Parse Recursion
    bool bRecursivePaths = true;
    if (Payload->HasField(TEXT("recursivePaths")))
      Payload->TryGetBoolField(TEXT("recursivePaths"), bRecursivePaths);
    Filter.bRecursivePaths = bRecursivePaths;

    bool bRecursiveClasses = false;
    if (Payload->HasField(TEXT("recursiveClasses")))
      Payload->TryGetBoolField(TEXT("recursiveClasses"), bRecursiveClasses);
    Filter.bRecursiveClasses = bRecursiveClasses;

    // Execute Query
    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            "AssetRegistry");
    TArray<FAssetData> AssetDataList;
    AssetRegistryModule.Get().GetAssets(Filter, AssetDataList);

    // Apply Limit
    int32 Limit = 100;
    if (Payload->HasField(TEXT("limit")))
      Payload->TryGetNumberField(TEXT("limit"), Limit);
    if (Limit > 0 && AssetDataList.Num() > Limit) {
      AssetDataList.SetNum(Limit);
    }

    // Build Response
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> AssetsArray;

    for (const FAssetData &Data : AssetDataList) {
      TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
      AssetObj->SetStringField(TEXT("assetName"), Data.AssetName.ToString());
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
      AssetObj->SetStringField(TEXT("assetPath"),
                               Data.GetSoftObjectPath().ToString());
      AssetObj->SetStringField(TEXT("classPath"),
                               Data.AssetClassPath.ToString());
#else
      AssetObj->SetStringField(TEXT("assetPath"),
                               Data.ToSoftObjectPath().ToString());
      AssetObj->SetStringField(TEXT("classPath"),
                               Data.AssetClass.ToString());
#endif
      AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
    }

    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("assets"), AssetsArray);
    Result->SetNumberField(TEXT("count"), AssetsArray.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Assets found."), Result);
    return true;
  }
#if WITH_EDITOR
  else if (SubAction == TEXT("get_source_control_state")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    if (ISourceControlModule::Get().IsEnabled()) {
      ISourceControlProvider &Provider =
          ISourceControlModule::Get().GetProvider();
      FSourceControlStatePtr State =
          Provider.GetState(AssetPath, EStateCacheUsage::Use);

      if (State.IsValid()) {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("isCheckedOut"), State->IsCheckedOut());
        Result->SetBoolField(TEXT("isAdded"), State->IsAdded());
        Result->SetBoolField(TEXT("isDeleted"), State->IsDeleted());
        Result->SetBoolField(TEXT("isModified"), State->IsModified());
        // Result->SetStringField(TEXT("whoCheckedOut"),
        // State->GetCheckOutUser());

        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Source control state retrieved."), Result);
      } else {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Could not get source control state."),
                            TEXT("STATE_FAILED"));
      }
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Source control not enabled."),
                          TEXT("SC_DISABLED"));
    }
    return true;
  }
#endif

  SendAutomationError(RequestingSocket, RequestId, TEXT("Unknown subAction."),
                      TEXT("INVALID_SUBACTION"));
  return true;
}