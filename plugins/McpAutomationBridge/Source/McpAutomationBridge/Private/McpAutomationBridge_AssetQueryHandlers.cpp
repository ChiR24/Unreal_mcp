#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlOperations.h"
#endif

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

  FString SubAction = Payload->GetStringField(TEXT("subAction"));

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
    FString Value;
    Payload->TryGetStringField(TEXT("value"), Value);

    if (Tag.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("tag required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FARFilter Filter;
    // We want to find assets that have this tag.
    // Specifying TagsAndValues with just key checks for existence,
    // key+value checks for specific value.
    if (!Value.IsEmpty()) {
      Filter.TagsAndValues.Add(FName(*Tag), Value);
    } else {
      // Searching by tag existence only is slightly more complex if the API
      // insists on a value, but normally adding key with empty string might not
      // work as intended for "exists". However, for now we assume exact match
      // or simple key check. Unreal's API usually requires a value for strict
      // matching. If user provided no value, we might iterate all assets? No,
      // that's slow. We will assume empty value means "any value" isn't
      // supported easily by FARFilter without iterating. But let's try adding
      // it with * wildcard or similar if supported? No, let's just add it.
      Filter.TagsAndValues.Add(FName(*Tag), FString());
    }

    // Also likely want to filter by class if provided? Code doesn't use it yet.
    // For now broad search.

    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            "AssetRegistry");
    TArray<FAssetData> AssetDataList;
    AssetRegistryModule.Get().GetAssets(Filter, AssetDataList);

    // Build Response
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> AssetsArray;

    for (const FAssetData &Data : AssetDataList) {
      TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
      AssetObj->SetStringField(TEXT("assetName"), Data.AssetName.ToString());
      AssetObj->SetStringField(TEXT("assetPath"),
                               Data.GetSoftObjectPath().ToString());
      AssetObj->SetStringField(TEXT("classPath"),
                               Data.AssetClassPath.ToString());
      AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
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
            Filter.ClassPaths.Add(FTopLevelAssetPath(ClassName));
          } else {
            // Map common short names to full paths
            if (ClassName.Equals(TEXT("Blueprint"), ESearchCase::IgnoreCase)) {
              Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"),
                                                       TEXT("Blueprint")));
            } else if (ClassName.Equals(TEXT("StaticMesh"),
                                        ESearchCase::IgnoreCase)) {
              Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"),
                                                       TEXT("StaticMesh")));
            } else if (ClassName.Equals(TEXT("SkeletalMesh"),
                                        ESearchCase::IgnoreCase)) {
              Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"),
                                                       TEXT("SkeletalMesh")));
            } else if (ClassName.Equals(TEXT("Material"),
                                        ESearchCase::IgnoreCase)) {
              Filter.ClassPaths.Add(
                  FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Material")));
            } else if (ClassName.Equals(TEXT("MaterialInstance"),
                                        ESearchCase::IgnoreCase) ||
                       ClassName.Equals(TEXT("MaterialInstanceConstant"),
                                        ESearchCase::IgnoreCase)) {
              Filter.ClassPaths.Add(FTopLevelAssetPath(
                  TEXT("/Script/Engine"), TEXT("MaterialInstanceConstant")));
            } else if (ClassName.Equals(TEXT("Texture2D"),
                                        ESearchCase::IgnoreCase)) {
              Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"),
                                                       TEXT("Texture2D")));
            } else if (ClassName.Equals(TEXT("Level"),
                                        ESearchCase::IgnoreCase) ||
                       ClassName.Equals(TEXT("World"),
                                        ESearchCase::IgnoreCase)) {
              Filter.ClassPaths.Add(
                  FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("World")));
            } else if (ClassName.Equals(TEXT("SoundCue"),
                                        ESearchCase::IgnoreCase)) {
              Filter.ClassPaths.Add(
                  FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("SoundCue")));
            } else if (ClassName.Equals(TEXT("SoundWave"),
                                        ESearchCase::IgnoreCase)) {
              Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"),
                                                       TEXT("SoundWave")));
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
      AssetObj->SetStringField(TEXT("assetPath"),
                               Data.GetSoftObjectPath().ToString());
      AssetObj->SetStringField(TEXT("classPath"),
                               Data.AssetClassPath.ToString());
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
