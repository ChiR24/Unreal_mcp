#include "Domains/Level/McpAutomationBridge_LevelHandlersActions.h"
#include "Domains/Level/Lifecycle/McpAutomationBridge_LevelHandlersDirtyPackageLoad.h"
#include "Domains/Level/World/McpAutomationBridge_LevelHandlersWorldAccess.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Modules/ModuleManager.h"

namespace McpLevelHandlers {
namespace {
// The registry entry for a package or object path. A bare package path tries the map asset
// "<path>.<ShortName>" first: for a loaded level the bare path finds the in-memory UPackage,
// whose entry reads class Package with no tags.
FAssetData FindLevelAssetData(const FString& Path) {
  IAssetRegistry& AssetRegistry = FAssetRegistryModule::GetRegistry();
  const FString ShortName = FPackageName::GetShortName(Path);
  if (!ShortName.IsEmpty() && !Path.Contains(TEXT("."))) {
    const FAssetData MapData = AssetRegistry.GetAssetByObjectPath(MCP_ASSET_REGISTRY_OBJECT_PATH(Path + TEXT(".") + ShortName));
    if (MapData.IsValid()) {
      return MapData;
    }
  }
  return AssetRegistry.GetAssetByObjectPath(MCP_ASSET_REGISTRY_OBJECT_PATH(Path));
}

// The entry's tags, minus FiBData: Blueprint bytecode that is kilobytes of non-UTF8 text, not metadata.
TSharedPtr<FJsonObject> TagsWithoutFiBData(const FAssetData& Data) {
  TSharedPtr<FJsonObject> Tags = McpHandlerUtils::CreateResultObject();
  for (const auto& Kvp : Data.TagsAndValues) {
    const FString TagKey = Kvp.Key.ToString();
    if (!TagKey.Equals(TEXT("FiBData"), ESearchCase::IgnoreCase)) {
      Tags->SetStringField(TagKey, Kvp.Value.AsString());
    }
  }
  return Tags;
}
} // namespace

bool HandleGetLevelInfoAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    FString LevelPath;
    if (Payload.IsValid()) {
      Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
      return true;
    }

    ULevel* TargetLevel = nullptr;
    if (!LevelPath.IsEmpty()) {
      LevelPath = SanitizeProjectRelativePath(LevelPath);
      if (LevelPath.IsEmpty()) {
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Invalid levelPath"), nullptr,
                               TEXT("SECURITY_VIOLATION"));
        return true;
      }

      TArray<ULevel*> Levels = GetAllLevelsFromWorld(World);
      for (ULevel* Level : Levels) {
        if (Level && Level->GetOutermost() && Level->GetOutermost()->GetName() == LevelPath) {
          TargetLevel = Level;
          break;
        }
      }
    } else {
      TargetLevel = World->GetCurrentLevel();
    }

    if (TargetLevel) {
      // Loaded path: preserve existing JSON shape, only ADD `loaded: true`.
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      const FString PackageName =
          TargetLevel->GetOutermost() ? TargetLevel->GetOutermost()->GetName() : TEXT("");
      const FString AssetName = FPackageName::GetShortName(PackageName);
      Result->SetStringField(TEXT("levelPath"), PackageName);
      Result->SetStringField(TEXT("levelName"), AssetName);
      Result->SetNumberField(TEXT("actorCount"), TargetLevel->Actors.Num());
      Result->SetBoolField(TEXT("loaded"), true);
      AddUnsavedState(Result, TargetLevel);

      // A loaded level must be identifiable as a map asset without a
      // follow-up list_levels call. The record already declares these fields;
      // the unloaded branch emits them, so mirror that identity here.
      if (!PackageName.IsEmpty()) {
        Result->SetStringField(TEXT("packageName"), PackageName);
        Result->SetStringField(TEXT("assetName"), AssetName);
        Result->SetStringField(TEXT("objectPath"), PackageName + TEXT(".") + AssetName);
        const FAssetData LevelAssetData = FindLevelAssetData(PackageName);
        if (LevelAssetData.IsValid()) {
          Result->SetStringField(TEXT("assetClass"),
                                 MCP_ASSET_DATA_GET_CLASS_PATH(LevelAssetData));
          Result->SetObjectField(TEXT("tagsAndValues"), TagsWithoutFiBData(LevelAssetData));
        }
      }

      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Level info retrieved"), Result);
      return true;
    }

    // Not loaded as a UWorld — fall back to AssetRegistry lookup so callers can
    // query metadata for any map asset without forcing a load. We do NOT auto-load.
    if (!LevelPath.IsEmpty()) {
      // Accept either a package path ("/Game/Maps/Foo") or a full object path ("/Game/Maps/Foo.Foo").
      const FAssetData AssetData = FindLevelAssetData(LevelPath);
      if (AssetData.IsValid()) {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetBoolField(TEXT("loaded"), false);
        Result->SetStringField(TEXT("levelPath"), AssetData.PackageName.ToString());
        Result->SetStringField(TEXT("levelName"), AssetData.AssetName.ToString());
        Result->SetStringField(TEXT("packageName"), AssetData.PackageName.ToString());
        Result->SetStringField(TEXT("assetName"), AssetData.AssetName.ToString());
        Result->SetStringField(TEXT("objectPath"), MCP_ASSET_DATA_GET_OBJECT_PATH(AssetData));
        Result->SetStringField(TEXT("assetClass"), MCP_ASSET_DATA_GET_CLASS_PATH(AssetData));

        Result->SetObjectField(TEXT("tagsAndValues"), TagsWithoutFiBData(AssetData));

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Level info retrieved (asset registry, not loaded)"), Result);
        return true;
      }
    }

    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                           FString::Printf(TEXT("Level not found: %s"), *LevelPath),
                           nullptr, TEXT("LEVEL_NOT_FOUND"));
    return true;
}
} // namespace McpLevelHandlers
