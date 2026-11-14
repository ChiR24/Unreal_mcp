#include "Domains/Level/McpLevelActions.h"
#include "Domains/Level/Copy/McpLevelCopyOperations.h"
#include "Domains/Level/Lifecycle/McpLevelDeletion.h"
#include "Domains/Level/Lifecycle/McpLevelPathSafety.h"

#include "HAL/FileManager.h"
#include "UObject/UObjectGlobals.h"

namespace McpLevelHandlers {
bool HandleDeleteLevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    FString LevelPath;
    Payload->TryGetStringField(TEXT("levelPath"), LevelPath);

    if (LevelPath.IsEmpty()) {
      Subsystem.SendAutomationError(RequestingSocket, RequestId,
                             TEXT("levelPath required for delete_level"),
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Issue #8: Sanitize path to prevent traversal attacks
    FString SanitizedPath = SanitizeProjectRelativePath(LevelPath);
    if (SanitizedPath.IsEmpty()) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             McpPathRefusalMessage(TEXT("levelPath"), LevelPath),
                             nullptr, TEXT("SECURITY_VIOLATION"));
      return true;
    }
    LevelPath = SanitizedPath;

    FString LongPackageName = LevelPath;
    int32 ObjectPathDelimiter = INDEX_NONE;
    if (LongPackageName.FindChar(TEXT('.'), ObjectPathDelimiter)) {
      LongPackageName = LongPackageName.Left(ObjectPathDelimiter);
    }

    FString DeleteMapFilename;
    FString DeleteErrorMessage;
    FString DeleteErrorCode;
    if (!TryGetAbsoluteMapFilename(LongPackageName, DeleteMapFilename) ||
        !ValidateWritableGameMapPath(LongPackageName, DeleteMapFilename,
                                     TEXT("Delete target"),
                                     DeleteErrorMessage, DeleteErrorCode)) {
      if (DeleteErrorMessage.IsEmpty()) {
        DeleteErrorMessage = FString::Printf(
            TEXT("Could not convert delete target level to filename: %s"),
            *LongPackageName);
        DeleteErrorCode = TEXT("INVALID_LEVEL_PATH");
      }
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             DeleteErrorMessage, nullptr, DeleteErrorCode);
      return true;
    }

    const FString ObjectPath = McpToObjectPath(LongPackageName);
    FLevelFileDeletion Deletion;
    DeleteLevelFiles(LongPackageName, false, Deletion);
    ScanLevelPackagePath(LongPackageName, Deletion.MapFilename);

    bool bCurrentWorldMatchesTarget = false;
    const int32 RemovedStreamingRefs = RemoveStreamingReferencesForLevelDelete(
        LongPackageName, ObjectPath, bCurrentWorldMatchesTarget);

    UPackage* LoadedPackage = FindPackage(nullptr, *LongPackageName);
    const bool bWasLoaded = LoadedPackage != nullptr;
    bool bPackageUnloadAttempted = false;
    bool bPackageUnloadSucceeded = false;
    TryUnloadLoadedLevelPackageForDelete(LongPackageName, bCurrentWorldMatchesTarget,
                                         LoadedPackage, bPackageUnloadAttempted,
                                         bPackageUnloadSucceeded);
    const bool bPackageStillLoaded = LoadedPackage != nullptr;
    const bool bPackageExisted = FPackageName::DoesPackageExist(LongPackageName);

    if (!bCurrentWorldMatchesTarget && !bPackageStillLoaded) {
      UE_LOG(LogEngineWireSubsystem, Log,
             TEXT("delete_level: Deleting level files directly after registry/editor cleanup: %s"),
             *Deletion.MapFilename);
      DeleteLevelFiles(LongPackageName, true, Deletion);
    }

    ScanLevelPackagePath(LongPackageName, Deletion.MapFilename);

    const bool bMapFileStillExists = !Deletion.MapFilename.IsEmpty() &&
                                     IFileManager::Get().FileExists(*Deletion.MapFilename);
    // A level that is nowhere (on disk, in the registry, in memory) stays a success, as a cleanup may repeat, and says
    // nothing was deleted.
    const bool bNowhere = !Deletion.bMapExisted && !bPackageExisted && !bPackageStillLoaded;
    const bool bDeleted = (Deletion.bDeletedMap && !bMapFileStillExists) || bNowhere;
    const bool bDeletedWithSidecars = bDeleted && Deletion.SidecarsRemoved();
    const bool bExternalSidecarDeleteFailed = !Deletion.SidecarErrorMessage.IsEmpty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("levelPath"), LongPackageName);
    Result->SetBoolField(TEXT("deleted"), bDeletedWithSidecars && !bNowhere);
    Result->SetBoolField(TEXT("deletedMapFile"), Deletion.bDeletedMap);
    Result->SetBoolField(TEXT("builtDataExists"), Deletion.bBuiltDataExists);
    Result->SetBoolField(TEXT("deletedBuiltData"), Deletion.bDeletedBuiltData);
    Result->SetBoolField(TEXT("externalActorsExists"), Deletion.bExternalActorsExists);
    Result->SetBoolField(TEXT("deletedExternalActors"), Deletion.bDeletedExternalActors);
    Result->SetBoolField(TEXT("externalObjectsExists"), Deletion.bExternalObjectsExists);
    Result->SetBoolField(TEXT("deletedExternalObjects"), Deletion.bDeletedExternalObjects);
    if (bExternalSidecarDeleteFailed) {
      Result->SetStringField(TEXT("externalDeleteError"), Deletion.SidecarErrorMessage);
      Result->SetStringField(TEXT("externalDeleteErrorCode"), Deletion.SidecarErrorCode);
    }
    Result->SetBoolField(TEXT("wasLoaded"), bWasLoaded);
    Result->SetBoolField(TEXT("packageStillLoaded"), bPackageStillLoaded);
    Result->SetBoolField(TEXT("currentWorldMatchesTarget"), bCurrentWorldMatchesTarget);
    Result->SetNumberField(TEXT("removedStreamingRefs"), RemovedStreamingRefs);

    if (bExternalSidecarDeleteFailed) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             Deletion.SidecarErrorMessage, Result,
                             Deletion.SidecarErrorCode.IsEmpty()
                                 ? TEXT("SOURCE_EXTERNAL_DELETE_FAILED")
                                 : Deletion.SidecarErrorCode);
    } else if (bDeletedWithSidecars) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
                             bNowhere ? FString::Printf(TEXT("No level at %s; nothing was deleted"), *LongPackageName)
                                      : FString::Printf(TEXT("Level file deleted: %s"), *LongPackageName),
                             Result);
    } else if (bCurrentWorldMatchesTarget || bPackageStillLoaded) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Level is still loaded and cannot be deleted safely: %s"), *LongPackageName),
                             Result, TEXT("LEVEL_LOADED"));
    } else {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Failed to delete level: %s"), *LongPackageName),
                             Result, TEXT("DELETE_FAILED"));
    }
    return true;
}
} // namespace McpLevelHandlers
