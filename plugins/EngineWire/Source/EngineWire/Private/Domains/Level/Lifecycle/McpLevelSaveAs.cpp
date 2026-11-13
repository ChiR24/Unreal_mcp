#include "Domains/Level/McpLevelActions.h"
#include "Domains/Level/Lifecycle/McpLevelPathSafety.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"

#include "Safety/McpSafeOperationsLevelSave.h"

using McpSafeOperations::McpSafeLevelSave;

namespace McpLevelHandlers {
bool HandleSaveLevelAsAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    // Force cleanup to prevent potential deadlocks with HLODs/WorldPartition
    // during save
    FlushRenderingCommands();
    GEditor->ForceGarbageCollection(true);
    FlushRenderingCommands();

    FString SavePath = GetJsonStringField(Payload, TEXT("savePath"));
    if (SavePath.IsEmpty()) {
      Subsystem.SendAutomationError(RequestingSocket, RequestId,
                             TEXT("savePath required for save_level_as"),
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    const FString LevelPathError = CheckLevelPathIsOpenLevel(Payload);
    if (!LevelPathError.IsEmpty()) {
      Subsystem.SendAutomationError(RequestingSocket, RequestId, LevelPathError, TEXT("LEVEL_NOT_LOADED"));
      return true;
    }

    const FString SavePathAsGiven = SavePath;
    SavePath = SanitizeProjectRelativePath(SavePath);
    if (SavePath.IsEmpty()) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             McpPathRefusalMessage(TEXT("savePath"), SavePathAsGiven),
                             nullptr, TEXT("SECURITY_VIOLATION"));
      return true;
    }
    // McpSafeLevelSave never overwrites another package, but its refusal reached only the log and the reply
    // said "check Output Log for details". Name it here.
    const FString SavePackage = FPackageName::ObjectPathToPackageName(SavePath);
    if (FPackageName::DoesPackageExist(SavePackage) && !IsCurrentEditorWorldPackage(SavePackage)) {
      Subsystem.SendAutomationError(RequestingSocket, RequestId,
          FString::Printf(TEXT("Something already exists at %s, and save_as never overwrites it: pick another savePath."), *SavePackage),
          TEXT("ASSET_ALREADY_EXISTS"));
      return true;
    }

    // Validate path length BEFORE attempting save to prevent silent hangs
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
          Subsystem.SendAutomationResponse(
              RequestingSocket, RequestId, false,
              FString::Printf(TEXT("Path too long (%d chars, max %d): %s"),
                  AbsoluteFilePath.Len(), SafePathLength, *SavePath),
              ErrorDetail, TEXT("PATH_TOO_LONG"));
          return true;
        }
      }
    }

    bool bSaved = false;
    if (UWorld *World = McpHandlerUtils::GetEditorWorld()) {
      if (World->GetWorldPartition() && World->GetOutermost()->GetName().StartsWith(TEXT("/Temp/"))) {
        // Saving the unsaved Open World template as a new package fails on its
        // private template references and then asserts in the world partition
        // subsystem, so refuse before touching it.
        Subsystem.SendAutomationError(RequestingSocket, RequestId,
                               TEXT("save_level_as cannot save an unsaved World Partition template level; create_level or load a level first"),
                               TEXT("UNSAVED_TEMPLATE_LEVEL"));
        return true;
      }
      // Use McpSafeLevelSave to prevent Intel GPU driver crashes
      // Explicitly use 5 retries for Intel GPU resilience (max 7.75s total retry time)
      bSaved = McpSafeLevelSave(World->PersistentLevel, SavePath);
    }
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
      // levelPath is not a field the receipt reads; savedAssetPath gives it the new level's handle and change.
      Resp->SetStringField(TEXT("savedAssetPath"), SavePath);
      Subsystem.SendAutomationResponse(
          RequestingSocket, RequestId, true,
          FString::Printf(TEXT("Level saved as %s"), *SavePath), Resp,
          FString());
    } else {
      // A failed save is answered (it used to send nothing, so the caller hung)
      TSharedPtr<FJsonObject> ErrorDetail = McpHandlerUtils::CreateResultObject();
      ErrorDetail->SetStringField(TEXT("attemptedPath"), SavePath);
      ErrorDetail->SetStringField(TEXT("reason"), TEXT("Save operation failed - check Output Log for details"));
      Subsystem.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Failed to save level as: %s"), *SavePath),
          ErrorDetail, TEXT("SAVE_FAILED"));
    }
    return true;
}
} // namespace McpLevelHandlers
