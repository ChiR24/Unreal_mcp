#include "McpAutomationBridge_LevelHandlersActions.h"
#include "McpAutomationBridge_LevelHandlersWorldAccess.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Modules/ModuleManager.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
#define SendAutomationResponse(...) Subsystem.SendAutomationResponse(__VA_ARGS__)
#define SendAutomationError(...) Subsystem.SendAutomationError(__VA_ARGS__)
#define HandleExecuteEditorFunction(...) Subsystem.HandleExecuteEditorFunction(__VA_ARGS__)
#define HandleManageLevelStructureAction(...) Subsystem.HandleManageLevelStructureAction(__VA_ARGS__)
#define HandleSetMetadata(...) Subsystem.HandleSetMetadata(__VA_ARGS__)
bool HandleGetLevelInfoAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    FString LevelPath;
    if (Payload.IsValid()) {
      Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
      if (LevelPath.IsEmpty()) Payload->TryGetStringField(TEXT("level_path"), LevelPath);
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
      return true;
    }

    ULevel* TargetLevel = nullptr;
    if (!LevelPath.IsEmpty()) {
      LevelPath = SanitizeProjectRelativePath(LevelPath);
      if (LevelPath.IsEmpty()) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
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
      Result->SetStringField(TEXT("levelPath"), TargetLevel->GetOutermost() ? TargetLevel->GetOutermost()->GetName() : TEXT(""));
      Result->SetStringField(TEXT("levelName"), TargetLevel->GetName());
      Result->SetNumberField(TEXT("actorCount"), TargetLevel->Actors.Num());
      Result->SetBoolField(TEXT("loaded"), true);

      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Level info retrieved"), Result);
      return true;
    }

    // Not loaded as a UWorld — fall back to AssetRegistry lookup so callers can
    // query metadata for any map asset without forcing a load. We do NOT auto-load.
    if (!LevelPath.IsEmpty()) {
      IAssetRegistry& AssetRegistry =
          FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

      // Accept either a package path ("/Game/Maps/Foo") or a full object path
      // ("/Game/Maps/Foo.Foo"). FSoftObjectPath handles both forms.
      FAssetData AssetData;
      #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(LevelPath));
      if (!AssetData.IsValid()) {
        // Try appending `.<ShortName>` to a bare package path.
        const FString ShortName = FPackageName::GetShortName(LevelPath);
        if (!ShortName.IsEmpty() && !LevelPath.Contains(TEXT("."))) {
          const FString ObjectPath = LevelPath + TEXT(".") + ShortName;
          AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
        }
      }
      #else
      AssetData = AssetRegistry.GetAssetByObjectPath(FName(*LevelPath));
      if (!AssetData.IsValid()) {
        const FString ShortName = FPackageName::GetShortName(LevelPath);
        if (!ShortName.IsEmpty() && !LevelPath.Contains(TEXT("."))) {
          const FString ObjectPath = LevelPath + TEXT(".") + ShortName;
          AssetData = AssetRegistry.GetAssetByObjectPath(FName(*ObjectPath));
        }
      }
      #endif

      if (AssetData.IsValid()) {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetBoolField(TEXT("loaded"), false);
        Result->SetStringField(TEXT("levelPath"), AssetData.PackageName.ToString());
        Result->SetStringField(TEXT("levelName"), AssetData.AssetName.ToString());
        Result->SetStringField(TEXT("packageName"), AssetData.PackageName.ToString());
        Result->SetStringField(TEXT("assetName"), AssetData.AssetName.ToString());
        Result->SetStringField(TEXT("objectPath"), MCP_ASSET_DATA_GET_OBJECT_PATH(AssetData));
        Result->SetStringField(TEXT("assetClass"), MCP_ASSET_DATA_GET_CLASS_PATH(AssetData));

        TSharedPtr<FJsonObject> TagsObj = McpHandlerUtils::CreateResultObject();
        for (const auto& Kvp : AssetData.TagsAndValues) {
          TagsObj->SetStringField(Kvp.Key.ToString(), Kvp.Value.AsString());
        }
        Result->SetObjectField(TEXT("tagsAndValues"), TagsObj);

        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Level info retrieved (asset registry, not loaded)"), Result);
        return true;
      }
    }

    SendAutomationResponse(RequestingSocket, RequestId, false,
                           FString::Printf(TEXT("Level not found: %s"), *LevelPath),
                           nullptr, TEXT("LEVEL_NOT_FOUND"));
    return true;
}
#undef SendAutomationResponse
#undef SendAutomationError
#undef HandleExecuteEditorFunction
#undef HandleManageLevelStructureAction
#undef HandleSetMetadata
#endif
} // namespace McpLevelHandlers
