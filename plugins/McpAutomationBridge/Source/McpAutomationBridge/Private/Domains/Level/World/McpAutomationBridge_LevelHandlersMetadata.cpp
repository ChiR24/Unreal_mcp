#include "Domains/Level/McpAutomationBridge_LevelHandlersActions.h"
#include "Domains/Level/Lifecycle/McpAutomationBridge_LevelHandlersPathSafety.h"

#include "HAL/FileManager.h"

namespace McpLevelHandlers {
bool HandleSetMetadataAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    TSharedPtr<FJsonObject> AssetPayload = MakeShared<FJsonObject>();
    FString AssetPath;
    if (Payload.IsValid()) {
      AssetPayload->Values = Payload->Values;
      if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
        Payload->TryGetStringField(TEXT("levelPath"), AssetPath);
      }
    }

    AssetPath = NormalizeLevelPackagePath(AssetPath);
    FString MapFilename;
    FString ErrorMessage;
    FString ErrorCode;
    if (AssetPath.IsEmpty() || !TryGetAbsoluteMapFilename(AssetPath, MapFilename) ||
        !ValidateWritableGameMapPath(AssetPath, MapFilename, TEXT("Metadata"),
                                     ErrorMessage, ErrorCode)) {
      if (ErrorMessage.IsEmpty()) {
        ErrorMessage = FString::Printf(TEXT("metadata target must be a /Game level path: %s"), *AssetPath);
        ErrorCode = TEXT("SECURITY_VIOLATION");
      }
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false, ErrorMessage,
                             nullptr, ErrorCode);
      return true;
    }
    if (!IFileManager::Get().FileExists(*MapFilename) &&
        !FPackageName::DoesPackageExist(AssetPath)) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Level not found: %s"), *AssetPath),
                             nullptr, TEXT("NOT_FOUND"));
      return true;
    }

    // The shared metadata writer answers a missing or empty bag with a
    // success that wrote nothing; a level metadata call must write something.
    const TSharedPtr<FJsonObject>* Metadata = nullptr;
    if (!Payload.IsValid() || !Payload->TryGetObjectField(TEXT("metadata"), Metadata) ||
        !Metadata || !(*Metadata).IsValid() || (*Metadata)->Values.Num() == 0) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("metadata must be an object with at least one key to write, e.g. {\"author\": \"MCP\"}"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    AssetPayload->SetStringField(TEXT("assetPath"), AssetPath);
    return FMcpLevelHandlerAccess::SetMetadata(
        Subsystem, RequestId, AssetPayload, RequestingSocket);
}
} // namespace McpLevelHandlers
