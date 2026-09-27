#include "Domains/Level/McpAutomationBridge_LevelHandlersActions.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Domains/Level/Copy/McpAutomationBridge_LevelHandlersCopyOperations.h"
#include "Domains/Level/Lifecycle/McpAutomationBridge_LevelHandlersPathSafety.h"

#include "Editor.h"
#include "HAL/FileManager.h"

namespace McpLevelHandlers {
bool HandleImportLevelAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    FString DestinationPath = McpGetFirstStringField(Payload, {TEXT("destinationPath"), TEXT("targetPath")});
    FString SourcePath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);
    if (SourcePath.IsEmpty())
      if (Payload.IsValid())
        Payload->TryGetStringField(TEXT("packagePath"), SourcePath); // Mapping

    if (SourcePath.IsEmpty()) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("sourcePath/packagePath required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // If SourcePath is a package (starts with /Game), handle as Duplicate/Copy
    if (SourcePath.StartsWith(TEXT("/"))) {
      if (DestinationPath.IsEmpty()) {
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("destinationPath required for asset copy"),
                               nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
      }

      SourcePath = NormalizeLevelPackagePath(SanitizeProjectRelativePath(SourcePath));
      DestinationPath = NormalizeLevelPackagePath(SanitizeProjectRelativePath(DestinationPath));
      if (SourcePath.IsEmpty() || DestinationPath.IsEmpty()) {
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Invalid sourcePath or destinationPath"),
                               nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
      }

      bool bOverwrite = false;
      if (Payload.IsValid()) {
        Payload->TryGetBoolField(TEXT("overwrite"), bOverwrite);
      }

      FString DestinationFilename;
      const bool bDestinationFileExists =
          TryGetAbsoluteMapFilename(DestinationPath, DestinationFilename) &&
          IFileManager::Get().FileExists(*DestinationFilename);
      // Nothing is imported here, so this is not a success (it used to be).
      if (!bOverwrite && (bDestinationFileExists || FPackageName::DoesPackageExist(DestinationPath))) {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("sourcePath"), SourcePath);
        Result->SetStringField(TEXT("destinationPath"), DestinationPath);
        Result->SetBoolField(TEXT("alreadyExists"), true);
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                               FString::Printf(TEXT("Destination already exists: %s; nothing was imported. Pass overwrite=true to replace it, or choose another destinationPath"), *DestinationPath),
                               Result, TEXT("DESTINATION_EXISTS"));
        return true;
      }

      TSharedPtr<FJsonObject> Result;
      FString ErrorMessage;
      FString ErrorCode;
      const bool bCopied = CopyLevelMapPackageFile(SourcePath, DestinationPath,
                                                   bOverwrite, Result,
                                                   ErrorMessage, ErrorCode);
      if (bCopied) {
        Result->SetBoolField(TEXT("imported"), true);
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Level imported (copied)"), Result);
      } else {
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false, ErrorMessage,
                               Result,
                               ErrorCode.IsEmpty() ? TEXT("IMPORT_FAILED") : ErrorCode);
      }
      return true;
    }

    // Only the package-path copy is supported: MAP IMPORTADD adds no actors
    // from a .t3d in this engine, even for an engine-produced export.
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("sourcePath must be a level package path (/Game/...); importing .t3d files is not supported. Use the package-path form to copy a .umap."),
                           nullptr, TEXT("NOT_SUPPORTED"));
    return true;
  }
} // namespace McpLevelHandlers
