#include "McpAutomationBridge_LevelHandlersActions.h"
#include "McpAutomationBridge_LevelHandlersCopyOperations.h"
#include "McpAutomationBridge_LevelHandlersPathSafety.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
#define SendAutomationResponse(...) Subsystem.SendAutomationResponse(__VA_ARGS__)
#define SendAutomationError(...) Subsystem.SendAutomationError(__VA_ARGS__)
#define HandleExecuteEditorFunction(...) Subsystem.HandleExecuteEditorFunction(__VA_ARGS__)
#define HandleManageLevelStructureAction(...) Subsystem.HandleManageLevelStructureAction(__VA_ARGS__)
#define HandleSetMetadata(...) Subsystem.HandleSetMetadata(__VA_ARGS__)
bool HandleDuplicateLevelAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    FString SourcePath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);
    if (SourcePath.IsEmpty() && Payload.IsValid())
      Payload->TryGetStringField(TEXT("levelPath"), SourcePath);

    FString DestinationPath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath);

    if (SourcePath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("sourcePath or levelPath required for duplicate_level"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (DestinationPath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("destinationPath required for duplicate_level"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Issue #8: Sanitize paths to prevent traversal attacks
    FString SanitizedSource = SanitizeProjectRelativePath(SourcePath);
    if (SanitizedSource.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Invalid source path (traversal/security violation): %s"), *SourcePath),
                             nullptr, TEXT("SECURITY_VIOLATION"));
      return true;
    }
    FString SanitizedDest = SanitizeProjectRelativePath(DestinationPath);
    if (SanitizedDest.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Invalid destination path (traversal/security violation): %s"), *DestinationPath),
                             nullptr, TEXT("SECURITY_VIOLATION"));
      return true;
    }
    SourcePath = NormalizeLevelPackagePath(SanitizedSource);
    DestinationPath = NormalizeLevelPackagePath(SanitizedDest);

    bool bOverwrite = false;
    if (Payload.IsValid()) {
      Payload->TryGetBoolField(TEXT("overwrite"), bOverwrite);
    }

    TSharedPtr<FJsonObject> Result;
    FString ErrorMessage;
    FString ErrorCode;
    const bool bDuplicated = CopyLevelMapPackageFile(SourcePath, DestinationPath, bOverwrite, Result, ErrorMessage, ErrorCode);
    if (bDuplicated) {
      Result->SetBoolField(TEXT("duplicated"), true);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             FString::Printf(TEXT("Level duplicated to: %s"), *DestinationPath), Result);
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false, ErrorMessage, Result,
                             ErrorCode.IsEmpty() ? TEXT("DUPLICATE_FAILED") : ErrorCode);
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
