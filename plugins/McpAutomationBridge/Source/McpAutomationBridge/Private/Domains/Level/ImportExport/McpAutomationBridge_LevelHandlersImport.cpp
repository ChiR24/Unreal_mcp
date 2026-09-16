#include "Domains/Level/McpAutomationBridge_LevelHandlersActions.h"
#include "Domains/Level/Copy/McpAutomationBridge_LevelHandlersCopyOperations.h"
#include "Domains/Level/Lifecycle/McpAutomationBridge_LevelHandlersPathSafety.h"

#include "Editor.h"
#include "HAL/FileManager.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
#define SendAutomationResponse(...) Subsystem.SendAutomationResponse(__VA_ARGS__)
#define SendAutomationError(...) Subsystem.SendAutomationError(__VA_ARGS__)
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

    // NOTE: this branch imports into the currently open level, so there is no destination to compute.
    // A dead DestPath used to be derived from the caller's destinationPath and then never read, which made
    // a supplied destination look honoured while it was silently dropped.
    const FString FullSource = FPaths::ConvertRelativePathToFull(SourcePath);
    const FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    if (!FullSource.StartsWith(ProjectRoot, ESearchCase::IgnoreCase)) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("sourcePath must be a file inside the project directory"),
                             nullptr, TEXT("SECURITY_VIOLATION"));
      return true;
    }
    if (!IFileManager::Get().FileExists(*FullSource)) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Source file not found: %s"), *FullSource),
                             nullptr, TEXT("FILE_NOT_FOUND"));
      return true;
    }
    if (!FullSource.EndsWith(TEXT(".t3d"), ESearchCase::IgnoreCase)) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Only .t3d exports can be imported into the current level; a .umap must be copied by package path"),
                             nullptr, TEXT("NOT_IMPLEMENTED"));
      return true;
    }
    UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
    const int32 ActorsBefore = EditorWorld ? EditorWorld->GetActorCount() : 0;
    const bool bExecuted = GEditor->Exec(EditorWorld, *FString::Printf(TEXT("MAP IMPORTADD FILE=\"%s\""), *FullSource));
    const int32 ActorsAfter = EditorWorld ? EditorWorld->GetActorCount() : 0;
    const int32 ActorsAdded = FMath::Max(0, ActorsAfter - ActorsBefore);
    const FString ImportedInto = EditorWorld ? EditorWorld->GetOutermost()->GetName() : FString();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("sourcePath"), FullSource);
    Result->SetStringField(TEXT("importedInto"), ImportedInto);
    Result->SetNumberField(TEXT("actorsAdded"), ActorsAdded);
    Result->SetBoolField(TEXT("commandExecuted"), bExecuted);
    if (!DestinationPath.IsEmpty())
    {
      TArray<TSharedPtr<FJsonValue>> Warnings;
      Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(
          TEXT("destinationPath '%s' was ignored: a .t3d import always lands in the currently open level (%s)."),
          *DestinationPath, *ImportedInto)));
      Result->SetArrayField(TEXT("warnings"), Warnings);
    }

    if (!bExecuted)
    {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("MAP IMPORTADD failed"), Result, TEXT("IMPORT_FAILED"));
      return true;
    }

    // GEditor::Exec reports whether the console command was recognized, not whether anything was
    // imported. A malformed or empty .t3d therefore came back as success:true with actorsAdded:0 -- a
    // write that did nothing, reported as done.
    //
    // A zero delta is NOT the caller's fault, and must not be reported as though it were: a .t3d
    // produced by export_level from this very level, imported straight back, also adds zero actors
    // (verified 2026-09-15). MAP IMPORTADD runs and does nothing for any input in this engine version,
    // so this mode is reported as unimplemented rather than as a bad file.
    if (ActorsAdded == 0)
    {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("T3D actor import is not functional in this engine version: 'MAP IMPORTADD' ran (%s) but added no actors to %s. An engine-produced .t3d export imported back into the same level also adds zero actors, so the file is not the problem. Use the package-path form to copy a .umap instead."),
                                             bExecuted ? TEXT("recognized") : TEXT("unrecognized"), *ImportedInto),
                             Result, TEXT("NOT_IMPLEMENTED"));
      return true;
    }

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("T3D imported into the current level"), Result);
    return true;
  }
  // Automation of Import is tricky without a factory wrapper.
  // Use AssetTools Import.
#endif
} // namespace McpLevelHandlers
