#include "Domains/Level/McpLevelActions.h"
#include "Domains/Level/Lifecycle/McpLevelPathSafety.h"

#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Exporters/Exporter.h"

#include "Safety/McpSafeOperationsLevelSave.h"

using McpSafeOperations::McpSafeLevelSave;

namespace McpLevelHandlers {
bool HandleExportLevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    FString LevelPath = GetJsonStringField(Payload, TEXT("levelPath"));
    FString ExportPath = McpGetFirstStringField(Payload, {TEXT("exportPath"), TEXT("destinationPath")});

    if (ExportPath.IsEmpty()) {
      Subsystem.SendAutomationError(RequestingSocket, RequestId,
                             TEXT("exportPath required"),
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UWorld *WorldToExport = McpHandlerUtils::GetEditorWorld();
    if (!WorldToExport) {
      Subsystem.SendAutomationError(RequestingSocket, RequestId,
                             TEXT("No world loaded"),
                             TEXT("NO_WORLD"));
      return true;
    }
    // levelPath must name the open level. It is compared before any file is looked for: the open level named before
    // its first save was LEVEL_NOT_FOUND where it is the level to export. A miss says whether a level is there at all.
    if (!LevelPath.IsEmpty() && !CheckLevelPathIsOpenLevel(Payload).IsEmpty()) {
      if (!FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(LevelPath))) {
        Subsystem.SendAutomationError(RequestingSocket, RequestId,
                               FString::Printf(TEXT("Source level not found: %s"), *LevelPath),
                               TEXT("LEVEL_NOT_FOUND"));
        return true;
      }
      Subsystem.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(
              TEXT("Requested level is not loaded: %s. Load the level before exporting it."),
              *LevelPath),
          TEXT("LEVEL_NOT_LOADED"));
      return true;
    }

    // A .t3d destination is a text export of that level, written with
    // the engine's T3D level exporter; it must stay inside the project
    // directory (Saved/... is fine). Anything else is a /Game package copy.
    if (ExportPath.EndsWith(TEXT(".t3d"), ESearchCase::IgnoreCase)) {
      // The one resolver for a file the plugin writes: a relative path is the project's, and a path that leaves the
      // project, names a device or crosses a symbolic link is refused. The hand-written test this replaces compared a
      // prefix without regard to case and knew neither links nor device names.
      FString FullT3D;
      FString PathError;
      if (!McpResolveProjectFilePath(ExportPath, FullT3D, PathError)) {
        PathError.RemoveFromStart(TEXT("SECURITY_VIOLATION: "));
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                               FString::Printf(TEXT("A .t3d exportPath must be a file inside the project directory (e.g. Saved/Exports/Level.t3d): %s"), *PathError),
                               nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
      }
      IFileManager::Get().MakeDirectory(*FPaths::GetPath(FullT3D), true);
      const bool bWrote = UExporter::ExportToFile(WorldToExport, nullptr, *FullT3D, false, false) != 0;
      TSharedPtr<FJsonObject> T3DResult = McpHandlerUtils::CreateResultObject();
      // Relative to the project, as the path is given: the absolute one told a caller where the project lives.
      FString ShownT3D = FullT3D;
      FPaths::MakePathRelativeTo(ShownT3D, *FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
      T3DResult->SetStringField(TEXT("exportPath"), ShownT3D);
      T3DResult->SetStringField(TEXT("format"), TEXT("t3d"));
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, bWrote,
                             bWrote ? TEXT("Level exported as T3D") : TEXT("T3D export failed"),
                             T3DResult, bWrote ? FString() : TEXT("EXPORT_FAILED"));
      return true;
    }
    // SECURITY: Sanitize export path as an asset path
    FString SafeExportPath = FPackageName::ObjectPathToPackageName(SanitizeProjectRelativePath(ExportPath));
    if (SafeExportPath.IsEmpty()) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             McpPathRefusalMessage(TEXT("exportPath"), ExportPath) +
                                 TEXT(" Use a /Game package path for a map copy, or a project-relative .t3d file for a text export."),
                             nullptr, TEXT("SECURITY_VIOLATION"));
      return true;
    }

    FString AbsoluteFilePath;
    FString ExportErrorMessage;
    FString ExportErrorCode;
    if (!TryResolveWritableGameMapFilename(SafeExportPath, AbsoluteFilePath,
                                           ExportErrorMessage,
                                           ExportErrorCode,
                                           TEXT("Export destination"))) {
      Subsystem.SendAutomationError(RequestingSocket, RequestId,
                             ExportErrorMessage,
                             ExportErrorCode.IsEmpty() ? TEXT("INVALID_ARGUMENT") : ExportErrorCode);
      return true;
    }

    // McpSafeLevelSave never saves over another level, but its refusal reached only the log and the reply blamed the
    // GPU driver: it is named here, as save_as names it.
    if (FPackageName::DoesPackageExist(SafeExportPath) && !IsCurrentEditorWorldPackage(SafeExportPath)) {
      Subsystem.SendAutomationError(RequestingSocket, RequestId,
          FString::Printf(TEXT("Something already exists at %s, and export_level never overwrites it: pick another exportPath."), *SafeExportPath),
          TEXT("ASSET_ALREADY_EXISTS"));
      return true;
    }
    if (McpSafeOperations::McpIsUnsavedWorldPartitionTemplate(WorldToExport)) {
      Subsystem.SendAutomationError(RequestingSocket, RequestId,
                             TEXT("export_level cannot copy an unsaved World Partition template level; create_level or load a level first"),
                             TEXT("UNSAVED_TEMPLATE_LEVEL"));
      return true;
    }

    // Ensure directory only after the export destination is validated as a
    // writable /Game map inside the project Content directory.
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsoluteFilePath), true);

    // FEditorFileUtils::SaveLevel duplicates a level saved before into the copy and leaves it open, but renames one never
    // saved to the copy, so the editor is left on it, as after save_as (SaveWorld, 5.0 to 5.8).
    const bool bWasSaved = !McpSafeOperations::McpIsUnsavedLevelPath(WorldToExport->GetOutermost()->GetName());
    if (!McpSafeLevelSave(WorldToExport->PersistentLevel, SafeExportPath)) {
      Subsystem.SendAutomationError(RequestingSocket, RequestId,
                             FString::Printf(TEXT("Failed to export the level to %s; the Output Log says why"), *SafeExportPath),
                             TEXT("EXPORT_FAILED"));
      return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("exportPath"), SafeExportPath);
    Result->SetStringField(TEXT("format"), TEXT("umap"));
    // exportPath is not a field the receipt reads; savedAssetPath gives it the copy's handle and change.
    Result->SetStringField(TEXT("savedAssetPath"), SafeExportPath);
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
                           bWasSaved ? FString::Printf(TEXT("Level exported to %s"), *SafeExportPath)
                                     : FString::Printf(TEXT("Level exported to %s; it had never been saved, so the editor now has the copy open"),
                                                       *SafeExportPath),
                           Result);
    return true;
}
} // namespace McpLevelHandlers
