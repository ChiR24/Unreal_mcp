#include "Domains/Level/McpAutomationBridge_LevelHandlersActions.h"
#include "Domains/Level/Copy/McpAutomationBridge_LevelHandlersCopyOperations.h"
#include "Domains/Level/Lifecycle/McpAutomationBridge_LevelHandlersDeletion.h"
#include "Domains/Level/Lifecycle/McpAutomationBridge_LevelHandlersPathSafety.h"

#include "HAL/FileManager.h"
#include "UObject/UObjectGlobals.h"

namespace McpLevelHandlers {
bool HandleDeleteLevelAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    FString LevelPath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
    if (LevelPath.IsEmpty() && Payload.IsValid())
      Payload->TryGetStringField(TEXT("path"), LevelPath);

    // `levelPaths` used to be silently ignored while the call reported success
    // (dogfood #157b). One level is deleted per call: accept a single-entry
    // array, and refuse a batch loudly instead of pretending.
    const TArray<TSharedPtr<FJsonValue>>* LevelPathsArray = nullptr;
    if (Payload.IsValid() && Payload->TryGetArrayField(TEXT("levelPaths"), LevelPathsArray) && LevelPathsArray) {
      if (LevelPathsArray->Num() > 1 || (LevelPathsArray->Num() == 1 && !LevelPath.IsEmpty())) {
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("delete_level removes one level per call; pass a single levelPath (or a one-entry levelPaths) and repeat for each level"),
                               nullptr, TEXT("BATCH_NOT_SUPPORTED"));
        return true;
      }
      if (LevelPathsArray->Num() == 1 && (*LevelPathsArray)[0].IsValid()) {
        LevelPath = (*LevelPathsArray)[0]->AsString();
      }
    }
    if (LevelPath.IsEmpty()) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("levelPath required for delete_level"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
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

    const FString AssetName = FPaths::GetBaseFilename(LongPackageName);
    const FString ObjectPath = AssetName.IsEmpty()
                                   ? LongPackageName
                                   : FString::Printf(TEXT("%s.%s"), *LongPackageName, *AssetName);
    FLevelFileDeletion Deletion;
    DeleteLevelFiles(LongPackageName, false, Deletion);
    ScanLevelPackagePath(LongPackageName, Deletion.MapFilename, true);

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
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
             TEXT("delete_level: Deleting level files directly after registry/editor cleanup: %s"),
             *Deletion.MapFilename);
      DeleteLevelFiles(LongPackageName, true, Deletion);
    }

    ScanLevelPackagePath(LongPackageName, Deletion.MapFilename, true);

    const bool bMapFileStillExists = !Deletion.MapFilename.IsEmpty() &&
                                     IFileManager::Get().FileExists(*Deletion.MapFilename);
    const bool bPackageStillExists = FPackageName::DoesPackageExist(LongPackageName);
    const bool bDeleted = (Deletion.bDeletedMap && !bMapFileStillExists) ||
                          (!Deletion.bMapExisted && !bPackageExisted && !bPackageStillLoaded);
    const bool bDeletedWithSidecars = bDeleted && Deletion.SidecarsRemoved();
    const bool bExternalSidecarDeleteFailed = !Deletion.SidecarErrorMessage.IsEmpty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("levelPath"), LongPackageName);
    Result->SetStringField(TEXT("objectPath"), ObjectPath);
    Result->SetStringField(TEXT("mapFilename"), Deletion.MapFilename);
    Result->SetBoolField(TEXT("deleted"), bDeletedWithSidecars);
    Result->SetBoolField(TEXT("deletedMapFile"), Deletion.bDeletedMap);
    Result->SetBoolField(TEXT("mapDeletedOrAlreadyAbsent"), bDeleted);
    Result->SetBoolField(TEXT("builtDataExists"), Deletion.bBuiltDataExists);
    Result->SetBoolField(TEXT("deletedBuiltData"), Deletion.bDeletedBuiltData);
    Result->SetBoolField(TEXT("externalSidecarDeleteAttempted"), Deletion.bSidecarDeleteAttempted);
    Result->SetBoolField(TEXT("externalSidecarDeleteFailed"), bExternalSidecarDeleteFailed);
    Result->SetBoolField(TEXT("externalActorsExists"), Deletion.bExternalActorsExists);
    Result->SetBoolField(TEXT("deletedExternalActors"), Deletion.bDeletedExternalActors);
    Result->SetBoolField(TEXT("externalObjectsExists"), Deletion.bExternalObjectsExists);
    Result->SetBoolField(TEXT("deletedExternalObjects"), Deletion.bDeletedExternalObjects);
    if (bExternalSidecarDeleteFailed) {
      Result->SetStringField(TEXT("externalDeleteError"), Deletion.SidecarErrorMessage);
      Result->SetStringField(TEXT("externalDeleteErrorCode"), Deletion.SidecarErrorCode);
    }
    Result->SetBoolField(TEXT("wasLoaded"), bWasLoaded);
    Result->SetBoolField(TEXT("packageUnloadAttempted"), bPackageUnloadAttempted);
    Result->SetBoolField(TEXT("packageUnloadSucceeded"), bPackageUnloadSucceeded);
    Result->SetBoolField(TEXT("packageStillLoaded"), bPackageStillLoaded);
    Result->SetBoolField(TEXT("currentWorldMatchesTarget"), bCurrentWorldMatchesTarget);
    Result->SetNumberField(TEXT("removedStreamingRefs"), RemovedStreamingRefs);
    Result->SetBoolField(TEXT("fileExistsAfter"), bMapFileStillExists);
    Result->SetBoolField(TEXT("packageExistsAfter"), bPackageStillExists);

    if (bExternalSidecarDeleteFailed) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             Deletion.SidecarErrorMessage, Result,
                             Deletion.SidecarErrorCode.IsEmpty()
                                 ? TEXT("SOURCE_EXTERNAL_DELETE_FAILED")
                                 : Deletion.SidecarErrorCode);
    } else if (bDeletedWithSidecars) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
                             FString::Printf(TEXT("Level file deleted: %s"), *LongPackageName), Result);
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
