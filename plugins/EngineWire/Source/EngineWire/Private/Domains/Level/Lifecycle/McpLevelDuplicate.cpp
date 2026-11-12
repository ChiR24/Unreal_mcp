#include "Domains/Level/McpLevelActions.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Domains/Level/Copy/McpLevelCopyOperations.h"
#include "Domains/Level/Lifecycle/McpLevelPathSafety.h"

namespace McpLevelHandlers {
bool HandleDuplicateLevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    FString SourcePath = McpGetFirstStringField(Payload, {TEXT("sourcePath"), TEXT("levelPath")});

    FString DestinationPath = McpGetFirstStringField(Payload, {TEXT("destinationPath")});

    if (SourcePath.IsEmpty()) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("sourcePath or levelPath required for duplicate_level"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (DestinationPath.IsEmpty()) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("destinationPath required for duplicate_level"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (!SanitizeLevelPathPairOrReply(Subsystem, RequestingSocket, RequestId, SourcePath, DestinationPath,
                                      TEXT("source path"), TEXT("destination path"))) {
      return true;
    }

    const bool bOverwrite = GetJsonBoolField(Payload, TEXT("overwrite"), false);

    TSharedPtr<FJsonObject> Result;
    FString ErrorMessage;
    FString ErrorCode;
    const bool bDuplicated = CopyLevelMapPackageFile(SourcePath, DestinationPath, bOverwrite, Result, ErrorMessage, ErrorCode);
    if (bDuplicated) {
      Result->SetBoolField(TEXT("duplicated"), true);
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
                             FString::Printf(TEXT("Level duplicated to: %s"), *DestinationPath), Result);
    } else {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false, ErrorMessage, Result,
                             ErrorCode.IsEmpty() ? TEXT("DUPLICATE_FAILED") : ErrorCode);
    }
    return true;
}
} // namespace McpLevelHandlers
