#include "McpAutomationBridge_LevelHandlersActions.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"

#include "McpSafeOperationsLevelSave.h"

#if defined(__has_include)
#if __has_include("Subsystems/LevelEditorSubsystem.h")
#include "Subsystems/LevelEditorSubsystem.h"
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 1
#elif __has_include("LevelEditorSubsystem.h")
#include "LevelEditorSubsystem.h"
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 1
#else
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 0
#endif
#else
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 0
#endif

using McpSafeOperations::McpSafeLevelSave;

namespace McpLevelHandlers {
#if WITH_EDITOR
#define SendAutomationResponse(...) Subsystem.SendAutomationResponse(__VA_ARGS__)
#define SendAutomationError(...) Subsystem.SendAutomationError(__VA_ARGS__)
#define HandleExecuteEditorFunction(...) Subsystem.HandleExecuteEditorFunction(__VA_ARGS__)
#define HandleManageLevelStructureAction(...) Subsystem.HandleManageLevelStructureAction(__VA_ARGS__)
#define HandleSetMetadata(...) Subsystem.HandleSetMetadata(__VA_ARGS__)
bool HandleSaveLevelAsAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    // Force cleanup to prevent potential deadlocks with HLODs/WorldPartition
    // during save
    if (GEditor) {
      FlushRenderingCommands();
      GEditor->ForceGarbageCollection(true);
      FlushRenderingCommands();
    }

    FString SavePath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
    if (SavePath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("savePath required for save_level_as"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    SavePath = SanitizeProjectRelativePath(SavePath);
    if (SavePath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Invalid savePath: contains path traversal (..) or invalid characters"),
                             nullptr, TEXT("SECURITY_VIOLATION"));
      return true;
    }

    // CRITICAL: Validate path length BEFORE attempting save to prevent silent hangs
    // McpSafeLevelSave validates internally but may not send error response in all code paths
    {
      FString AbsoluteFilePath;
      if (FPackageName::TryConvertLongPackageNameToFilename(SavePath, AbsoluteFilePath, FPackageName::GetMapPackageExtension()))
      {
        AbsoluteFilePath = FPaths::ConvertRelativePathToFull(AbsoluteFilePath);
        const int32 SafePathLength = 240;
        if (AbsoluteFilePath.Len() > SafePathLength)
        {
          TSharedPtr<FJsonObject> ErrorDetail = McpHandlerUtils::CreateResultObject();
          ErrorDetail->SetStringField(TEXT("attemptedPath"), SavePath);
          ErrorDetail->SetStringField(TEXT("absolutePath"), AbsoluteFilePath);
          ErrorDetail->SetNumberField(TEXT("pathLength"), AbsoluteFilePath.Len());
          ErrorDetail->SetNumberField(TEXT("maxLength"), SafePathLength);
          SendAutomationResponse(
              RequestingSocket, RequestId, false,
              FString::Printf(TEXT("Path too long (%d chars, max %d): %s"),
                  AbsoluteFilePath.Len(), SafePathLength, *SavePath),
              ErrorDetail, TEXT("PATH_TOO_LONG"));
          return true;
        }
      }
    }

#if defined(MCP_HAS_LEVELEDITOR_SUBSYSTEM)
    if (ULevelEditorSubsystem *LevelEditorSS =
            GEditor->GetEditorSubsystem<ULevelEditorSubsystem>()) {
      bool bSaved = false;
#if __has_include("FileHelpers.h")
      if (UWorld *World = GEditor->GetEditorWorldContext().World()) {
        // Use McpSafeLevelSave to prevent Intel GPU driver crashes
        // Explicitly use 5 retries for Intel GPU resilience (max 7.75s total retry time)
        bSaved = McpSafeLevelSave(World->PersistentLevel, SavePath);
      }
#endif
      if (bSaved) {
        // Refresh Asset Registry so the saved level is immediately visible for rename/duplicate operations
        IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
        FString SavedFilename;
        if (FPackageName::TryConvertLongPackageNameToFilename(SavePath, SavedFilename, FPackageName::GetMapPackageExtension())) {
          TArray<FString> FilesToScan;
          FilesToScan.Add(SavedFilename);
          AssetRegistry.ScanFilesSynchronous(FilesToScan, true);
        }

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetStringField(TEXT("levelPath"), SavePath);
        SendAutomationResponse(
            RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Level saved as %s"), *SavePath), Resp,
            FString());
      } else {
        // CRITICAL FIX: Send error response when save fails (was missing before, causing silent hangs)
        TSharedPtr<FJsonObject> ErrorDetail = McpHandlerUtils::CreateResultObject();
        ErrorDetail->SetStringField(TEXT("attemptedPath"), SavePath);
        ErrorDetail->SetStringField(TEXT("reason"), TEXT("Save operation failed - check Output Log for details"));
        SendAutomationResponse(
            RequestingSocket, RequestId, false,
            FString::Printf(TEXT("Failed to save level as: %s"), *SavePath),
            ErrorDetail, TEXT("SAVE_FAILED"));
      }
      return true;
    }
#endif
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("LevelEditorSubsystem not available"), nullptr,
                           TEXT("SUBSYSTEM_MISSING"));
    return true;
}
#undef SendAutomationResponse
#undef SendAutomationError
#undef HandleExecuteEditorFunction
#undef HandleManageLevelStructureAction
#undef HandleSetMetadata
#endif
} // namespace McpLevelHandlers
