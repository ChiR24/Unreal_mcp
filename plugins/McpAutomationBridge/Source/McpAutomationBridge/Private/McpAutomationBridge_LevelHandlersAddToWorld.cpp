#include "McpAutomationBridge_LevelHandlersActions.h"

#include "Editor.h"
#include "EditorLevelUtils.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
#define SendAutomationResponse(...) Subsystem.SendAutomationResponse(__VA_ARGS__)
#define SendAutomationError(...) Subsystem.SendAutomationError(__VA_ARGS__)
#define HandleExecuteEditorFunction(...) Subsystem.HandleExecuteEditorFunction(__VA_ARGS__)
#define HandleManageLevelStructureAction(...) Subsystem.HandleManageLevelStructureAction(__VA_ARGS__)
#define HandleSetMetadata(...) Subsystem.HandleSetMetadata(__VA_ARGS__)
bool HandleAddLevelToWorldAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    FString LevelPath;
    if (Payload.IsValid()) {
      Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
      if (LevelPath.IsEmpty()) Payload->TryGetStringField(TEXT("level_path"), LevelPath);
    }

    if (LevelPath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("levelPath required"), nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    LevelPath = SanitizeProjectRelativePath(LevelPath);
    if (LevelPath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Invalid levelPath"), nullptr,
                             TEXT("SECURITY_VIOLATION"));
      return true;
    }

    // Verify level package exists before adding to avoid false positives
    FString FilenameToCheck;
    bool bFileExists = false;
    if (FPackageName::TryConvertLongPackageNameToFilename(
            LevelPath, FilenameToCheck, FPackageName::GetMapPackageExtension())) {
      bFileExists = IFileManager::Get().FileExists(*FilenameToCheck);
    }
    if (!bFileExists && !FPackageName::DoesPackageExist(LevelPath)) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Level file not found: %s"), *LevelPath),
          nullptr, TEXT("PACKAGE_NOT_FOUND"));
      return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
      return true;
    }

    ULevelStreaming* StreamingLevel = UEditorLevelUtils::AddLevelToWorld(World, *LevelPath, ULevelStreamingDynamic::StaticClass());
    if (StreamingLevel) {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("levelPath"), LevelPath);
      Result->SetBoolField(TEXT("added"), true);
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Level added to world"), Result);
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Failed to add level: %s"), *LevelPath),
                             nullptr, TEXT("ADD_FAILED"));
    }
    return true;
}
#undef SendAutomationResponse
#undef SendAutomationError
#undef HandleExecuteEditorFunction
#undef HandleManageLevelStructureAction
#undef HandleSetMetadata
#endif
} // namespace McpLevelHandlers
