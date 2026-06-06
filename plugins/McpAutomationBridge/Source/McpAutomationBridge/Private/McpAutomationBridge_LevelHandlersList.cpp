#include "McpAutomationBridge_LevelHandlersActions.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "Modules/ModuleManager.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
#define SendAutomationResponse(...) Subsystem.SendAutomationResponse(__VA_ARGS__)
#define SendAutomationError(...) Subsystem.SendAutomationError(__VA_ARGS__)
#define HandleExecuteEditorFunction(...) Subsystem.HandleExecuteEditorFunction(__VA_ARGS__)
#define HandleManageLevelStructureAction(...) Subsystem.HandleManageLevelStructureAction(__VA_ARGS__)
#define HandleSetMetadata(...) Subsystem.HandleSetMetadata(__VA_ARGS__)
bool HandleListLevelsAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> LevelsArray;

    UWorld *World =
        GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

    // Add current persistent level
    if (World) {
      TSharedPtr<FJsonObject> CurrentLevel = McpHandlerUtils::CreateResultObject();
      CurrentLevel->SetStringField(TEXT("name"), World->GetMapName());
      CurrentLevel->SetStringField(TEXT("path"),
                                   World->GetOutermost()->GetName());
      CurrentLevel->SetBoolField(TEXT("isPersistent"), true);
      CurrentLevel->SetBoolField(TEXT("isLoaded"), true);
      CurrentLevel->SetBoolField(TEXT("isVisible"), true);
      LevelsArray.Add(MakeShared<FJsonValueObject>(CurrentLevel));

      // Add streaming levels
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

    // Also query Asset Registry for all map assets
    IAssetRegistry &AssetRegistry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry")
            .Get();
    TArray<FAssetData> MapAssets;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    AssetRegistry.GetAssetsByClass(
        FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("World")), MapAssets,
        false);
#else
    // UE 5.0: Use FName for class path
    AssetRegistry.GetAssetsByClass(
        FName(TEXT("World")), MapAssets,
        false);
#endif

    TArray<TSharedPtr<FJsonValue>> AllMapsArray;
    for (const FAssetData &MapAsset : MapAssets) {
      TSharedPtr<FJsonObject> MapEntry = McpHandlerUtils::CreateResultObject();
      MapEntry->SetStringField(TEXT("name"), MapAsset.AssetName.ToString());
      MapEntry->SetStringField(TEXT("path"), MapAsset.PackageName.ToString());
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      MapEntry->SetStringField(TEXT("objectPath"),
                               MapAsset.GetObjectPathString());
#else
      // UE 5.0: Construct object path from package and asset name
      MapEntry->SetStringField(TEXT("objectPath"),
                               FString::Printf(TEXT("%s.%s"), *MapAsset.PackageName.ToString(), *MapAsset.AssetName.ToString()));
#endif
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

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Levels listed"), Resp, FString());
    return true;
}
#undef SendAutomationResponse
#undef SendAutomationError
#undef HandleExecuteEditorFunction
#undef HandleManageLevelStructureAction
#undef HandleSetMetadata
#endif
} // namespace McpLevelHandlers
