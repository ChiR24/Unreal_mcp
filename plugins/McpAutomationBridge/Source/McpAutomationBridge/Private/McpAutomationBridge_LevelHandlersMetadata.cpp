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
      SendAutomationResponse(RequestingSocket, RequestId, false, ErrorMessage,
                             nullptr, ErrorCode);
      return true;
    }
    if (!IFileManager::Get().FileExists(*MapFilename) &&
        !FPackageName::DoesPackageExist(AssetPath)) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Level not found: %s"), *AssetPath),
                             nullptr, TEXT("NOT_FOUND"));
      return true;
    }

    AssetPayload->SetStringField(TEXT("assetPath"), AssetPath);
    return FMcpLevelHandlerAccess::SetMetadata(
        Subsystem, RequestId, AssetPayload, RequestingSocket);
}
#undef SendAutomationResponse
#undef SendAutomationError
#undef HandleExecuteEditorFunction
#undef HandleManageLevelStructureAction
#undef HandleSetMetadata
#endif
} // namespace McpLevelHandlers
