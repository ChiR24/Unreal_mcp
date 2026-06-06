#include "McpAutomationBridge_LevelHandlersActions.h"
#include "McpAutomationBridge_LevelHandlersPathSafety.h"

#include "Editor.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"

#include "McpSafeOperationsLevelSave.h"

using McpSafeOperations::McpSafeLevelSave;

namespace McpLevelHandlers {
#if WITH_EDITOR
#define SendAutomationResponse(...) Subsystem.SendAutomationResponse(__VA_ARGS__)
#define SendAutomationError(...) Subsystem.SendAutomationError(__VA_ARGS__)
#define HandleExecuteEditorFunction(...) Subsystem.HandleExecuteEditorFunction(__VA_ARGS__)
#define HandleManageLevelStructureAction(...) Subsystem.HandleManageLevelStructureAction(__VA_ARGS__)
#define HandleSetMetadata(...) Subsystem.HandleSetMetadata(__VA_ARGS__)
bool HandleExportLevelAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    FString LevelPath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
    FString ExportPath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("exportPath"), ExportPath);
    if (ExportPath.IsEmpty())
      if (Payload.IsValid())
        Payload->TryGetStringField(TEXT("destinationPath"), ExportPath);

    if (ExportPath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("exportPath required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // SECURITY: Sanitize export path as an asset path
    FString SafeExportPath = NormalizeLevelPackagePath(SanitizeProjectRelativePath(ExportPath));
    if (SafeExportPath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Invalid or unsafe exportPath"), nullptr,
                             TEXT("SECURITY_VIOLATION"));
      return true;
    }

    if (!GEditor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor not available"), nullptr,
                             TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    UWorld *WorldToExport = nullptr;
    if (!LevelPath.IsEmpty()) {
      // CRITICAL FIX: Validate source level exists before export
      if (!FPackageName::DoesPackageExist(LevelPath)) {
        // Also check file on disk as fallback
        FString Filename;
        bool bFileFound = false;
        if (FPackageName::TryConvertLongPackageNameToFilename(
                LevelPath, Filename, FPackageName::GetMapPackageExtension())) {
          bFileFound = IFileManager::Get().FileExists(*Filename);
        }
        if (!bFileFound) {
          SendAutomationResponse(RequestingSocket, RequestId, false,
                                 FString::Printf(TEXT("Source level not found: %s"), *LevelPath),
                                 nullptr, TEXT("LEVEL_NOT_FOUND"));
          return true;
        }
      }
      UWorld *Current = GEditor->GetEditorWorldContext().World();
      if (Current && (Current->GetOutermost()->GetName() == LevelPath ||
                      Current->GetPathName() == LevelPath)) {
        WorldToExport = Current;
      } else {
        SendAutomationResponse(
            RequestingSocket, RequestId, false,
            FString::Printf(
                TEXT("Requested level is not loaded: %s. Load the level before exporting it."),
                *LevelPath),
            nullptr, TEXT("LEVEL_NOT_LOADED"));
        return true;
      }
    }
    if (!WorldToExport)
      WorldToExport = GEditor->GetEditorWorldContext().World();

    if (!WorldToExport) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No world loaded"), nullptr,
                             TEXT("NO_WORLD"));
      return true;
    }

    FString AbsoluteFilePath;
    FString ExportErrorMessage;
    FString ExportErrorCode;
    if (!TryResolveWritableGameMapFilename(SafeExportPath, AbsoluteFilePath,
                                           ExportErrorMessage,
                                           ExportErrorCode,
                                           TEXT("Export destination"))) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             ExportErrorMessage, nullptr,
                             ExportErrorCode.IsEmpty() ? TEXT("INVALID_ARGUMENT") : ExportErrorCode);
      return true;
    }

    // Ensure directory only after the export destination is validated as a
    // writable /Game map inside the project Content directory.
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsoluteFilePath), true);

    // CRITICAL: Use McpSafeLevelSave instead of FEditorFileUtils::SaveMap
    // to prevent Intel GPU driver crashes (MONZA DdiThreadingContext)
    bool bExported = McpSafeLevelSave(WorldToExport->PersistentLevel, SafeExportPath);
    if (bExported) {
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Level exported"), nullptr);
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Failed to export level after 5 retries (check GPU driver stability)"), nullptr,
                             TEXT("EXPORT_FAILED"));
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
