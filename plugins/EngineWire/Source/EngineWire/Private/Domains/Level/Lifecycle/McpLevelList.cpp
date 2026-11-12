#include "Domains/Level/McpLevelActions.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "Modules/ModuleManager.h"

namespace McpLevelHandlers {
bool HandleListLevelsAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> LevelsArray;

    UWorld *World = McpHandlerUtils::GetEditorWorld();

    if (World) {
      TSharedPtr<FJsonObject> CurrentLevel = McpHandlerUtils::CreateResultObject();
      CurrentLevel->SetStringField(TEXT("name"), World->GetMapName());
      CurrentLevel->SetStringField(TEXT("path"),
                                   World->GetOutermost()->GetName());
      CurrentLevel->SetBoolField(TEXT("isPersistent"), true);
      CurrentLevel->SetBoolField(TEXT("isLoaded"), true);
      CurrentLevel->SetBoolField(TEXT("isVisible"), true);
      LevelsArray.Add(MakeShared<FJsonValueObject>(CurrentLevel));

      for (const ULevelStreaming *StreamingLevel :
           World->GetStreamingLevels()) {
        if (!StreamingLevel)
          continue;

        TSharedPtr<FJsonObject> LevelEntry = McpHandlerUtils::CreateResultObject();
        LevelEntry->SetStringField(TEXT("name"),
                                   StreamingLevel->GetWorldAssetPackageName());
        LevelEntry->SetStringField(
            TEXT("path"),
            StreamingLevel->GetWorldAssetPackageFName().ToString());
        LevelEntry->SetBoolField(TEXT("isPersistent"), false);
        LevelEntry->SetBoolField(TEXT("isLoaded"),
                                 StreamingLevel->IsLevelLoaded());
        LevelEntry->SetBoolField(TEXT("isVisible"),
                                 StreamingLevel->IsLevelVisible());
        LevelEntry->SetStringField(
            TEXT("streamingState"),
            StreamingLevel->IsStreamingStatePending() ? TEXT("Pending")
            : StreamingLevel->IsLevelLoaded()         ? TEXT("Loaded")
                                                      : TEXT("Unloaded"));
        LevelsArray.Add(MakeShared<FJsonValueObject>(LevelEntry));
      }
    }

    IAssetRegistry &AssetRegistry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry")
            .Get();
    TArray<FAssetData> MapAssets;
    AssetRegistry.GetAssetsByClass(MCP_CLASS_ID(UWorld::StaticClass()), MapAssets, false);

    TArray<TSharedPtr<FJsonValue>> AllMapsArray;
    for (const FAssetData &MapAsset : MapAssets) {
      TSharedPtr<FJsonObject> MapEntry = McpHandlerUtils::CreateResultObject();
      MapEntry->SetStringField(TEXT("name"), MapAsset.AssetName.ToString());
      MapEntry->SetStringField(TEXT("path"), MapAsset.PackageName.ToString());
      MapEntry->SetStringField(TEXT("objectPath"), MCP_ASSET_DATA_GET_OBJECT_PATH(MapAsset));
      AllMapsArray.Add(MakeShared<FJsonValueObject>(MapEntry));
    }

    Resp->SetArrayField(TEXT("currentWorldLevels"), LevelsArray);
    Resp->SetNumberField(TEXT("currentWorldLevelCount"), LevelsArray.Num());
    Resp->SetArrayField(TEXT("allMaps"), AllMapsArray);
    Resp->SetNumberField(TEXT("allMapsCount"), AllMapsArray.Num());

    if (World) {
      Resp->SetStringField(TEXT("currentMap"), World->GetMapName());
      Resp->SetStringField(TEXT("currentMapPath"),
                           World->GetOutermost()->GetName());
    }

    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Levels listed"), Resp, FString());
    return true;
}
} // namespace McpLevelHandlers
