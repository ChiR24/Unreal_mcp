#include "McpAutomationBridge_LevelHandlersActions.h"
#include "McpAutomationBridge_LevelHandlersCopyOperations.h"
#include "McpAutomationBridge_LevelHandlersPathSafety.h"

#include "Editor.h"
#include "HAL/FileManager.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
#define SendAutomationResponse(...) Subsystem.SendAutomationResponse(__VA_ARGS__)
#define SendAutomationError(...) Subsystem.SendAutomationError(__VA_ARGS__)
#define HandleExecuteEditorFunction(...) Subsystem.HandleExecuteEditorFunction(__VA_ARGS__)
#define HandleManageLevelStructureAction(...) Subsystem.HandleManageLevelStructureAction(__VA_ARGS__)
#define HandleSetMetadata(...) Subsystem.HandleSetMetadata(__VA_ARGS__)
bool HandleImportLevelAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    FString DestinationPath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath);
    FString SourcePath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);
    if (SourcePath.IsEmpty())
      if (Payload.IsValid())
        Payload->TryGetStringField(TEXT("packagePath"), SourcePath); // Mapping

    if (SourcePath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("sourcePath/packagePath required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // If SourcePath is a package (starts with /Game), handle as Duplicate/Copy
    if (SourcePath.StartsWith(TEXT("/"))) {
      if (DestinationPath.IsEmpty()) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("destinationPath required for asset copy"),
                               nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
      }

      SourcePath = NormalizeLevelPackagePath(SanitizeProjectRelativePath(SourcePath));
      DestinationPath = NormalizeLevelPackagePath(SanitizeProjectRelativePath(DestinationPath));
      if (SourcePath.IsEmpty() || DestinationPath.IsEmpty()) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
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
      if (!bOverwrite && (bDestinationFileExists || FPackageName::DoesPackageExist(DestinationPath))) {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("sourcePath"), SourcePath);
        Result->SetStringField(TEXT("destinationPath"), DestinationPath);
        Result->SetBoolField(TEXT("alreadyExists"), true);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               FString::Printf(TEXT("Destination already exists: %s"), *DestinationPath), Result);
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
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Level imported (copied)"), Result);
      } else {
        SendAutomationResponse(RequestingSocket, RequestId, false, ErrorMessage,
                               Result,
                               ErrorCode.IsEmpty() ? TEXT("IMPORT_FAILED") : ErrorCode);
      }
      return true;
    }

    // If SourcePath is file, try Import
    if (!GEditor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor not available"), nullptr,
                             TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    FString DestPath = DestinationPath.IsEmpty()
                           ? TEXT("/Game/Maps")
                           : FPaths::GetPath(DestinationPath);
    FString DestName = FPaths::GetBaseFilename(
        DestinationPath.IsEmpty() ? SourcePath : DestinationPath);

    TArray<FString> Files;
    Files.Add(SourcePath);
    // FEditorFileUtils::Import(DestPath, DestName); // Ambiguous/Removed
    // Use GEditor->ImportMap or handle via AssetTools
    // Simple fallback:
    if (GEditor) {
      // ImportMap is usually for T3D. If SourcePath is .umap, we should
      // Copy/Load. Assuming T3D import or similar:
      // GEditor->ImportMap(*DestPath, *DestName, *SourcePath);
      // ImportMap is deprecated/removed. For .umap files, manual import or Copy
      // is preferred.
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Direct map file import not supported. Use "
                                  "import_level with a package path to copy."),
                             nullptr, TEXT("NOT_IMPLEMENTED"));
      return true;
    }
    // Automation of Import is tricky without a factory wrapper.
    // Use AssetTools Import.

    SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("File-based level import not fully automatic yet"), nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
}
#undef SendAutomationResponse
#undef SendAutomationError
#undef HandleExecuteEditorFunction
#undef HandleManageLevelStructureAction
#undef HandleSetMetadata
#endif
} // namespace McpLevelHandlers
