#include "McpAutomationBridge_LevelHandlersActions.h"
#include "McpAutomationBridge_LevelHandlersPathSafety.h"

#include "HAL/FileManager.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
#define SendAutomationResponse(...) Subsystem.SendAutomationResponse(__VA_ARGS__)
#define SendAutomationError(...) Subsystem.SendAutomationError(__VA_ARGS__)
#define HandleExecuteEditorFunction(...) Subsystem.HandleExecuteEditorFunction(__VA_ARGS__)
#define HandleManageLevelStructureAction(...) Subsystem.HandleManageLevelStructureAction(__VA_ARGS__)
#define HandleSetMetadata(...) Subsystem.HandleSetMetadata(__VA_ARGS__)
bool HandleValidateLevelAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    FString LevelPath;
    if (Payload.IsValid()) {
      Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
      if (LevelPath.IsEmpty()) Payload->TryGetStringField(TEXT("assetPath"), LevelPath);
    }
    if (LevelPath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("levelPath required for validate_level"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }
    LevelPath = NormalizeLevelPackagePath(LevelPath);
    LevelPath = SanitizeProjectRelativePath(LevelPath);
    if (LevelPath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Invalid levelPath"), nullptr,
                             TEXT("SECURITY_VIOLATION"));
      return true;
    }

    FString MapFilename;
    const bool bHasFilename = TryGetAbsoluteMapFilename(LevelPath, MapFilename);
    const bool bFileExists = bHasFilename && IFileManager::Get().FileExists(*MapFilename);
    if (bHasFilename) {
      ScanLevelPackagePath(LevelPath, MapFilename);
    }
    const bool bPackageExists = FPackageName::DoesPackageExist(LevelPath);
    const bool bExists = bPackageExists || bFileExists;

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), bExists);
    Result->SetBoolField(TEXT("exists"), bExists);
    Result->SetBoolField(TEXT("isValid"), bExists);
    Result->SetStringField(TEXT("levelPath"), LevelPath);
    if (!MapFilename.IsEmpty()) {
      Result->SetStringField(TEXT("mapFilename"), MapFilename);
    }
    Result->SetBoolField(TEXT("packageExists"), bPackageExists);
    Result->SetBoolField(TEXT("fileExists"), bFileExists);

    SendAutomationResponse(RequestingSocket, RequestId, bExists,
                           bExists ? TEXT("Level validated") : TEXT("Level not found"),
                           Result, bExists ? FString() : TEXT("NOT_FOUND"));
    return true;
}
#undef SendAutomationResponse
#undef SendAutomationError
#undef HandleExecuteEditorFunction
#undef HandleManageLevelStructureAction
#undef HandleSetMetadata
#endif
} // namespace McpLevelHandlers
