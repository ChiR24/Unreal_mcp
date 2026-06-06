#include "McpAutomationBridge_LevelHandlersActions.h"
#include "McpAutomationBridge_LevelHandlersCopyOperations.h"
#include "McpAutomationBridge_LevelHandlersDeletion.h"
#include "McpAutomationBridge_LevelHandlersPathSafety.h"

#include "HAL/FileManager.h"
#include "UObject/UObjectGlobals.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
#define SendAutomationResponse(...) Subsystem.SendAutomationResponse(__VA_ARGS__)
#define SendAutomationError(...) Subsystem.SendAutomationError(__VA_ARGS__)
#define HandleExecuteEditorFunction(...) Subsystem.HandleExecuteEditorFunction(__VA_ARGS__)
#define HandleManageLevelStructureAction(...) Subsystem.HandleManageLevelStructureAction(__VA_ARGS__)
#define HandleSetMetadata(...) Subsystem.HandleSetMetadata(__VA_ARGS__)
bool HandleDeleteLevelAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    FString LevelPath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
    if (LevelPath.IsEmpty() && Payload.IsValid())
      Payload->TryGetStringField(TEXT("path"), LevelPath);

    if (LevelPath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("levelPath required for delete_level"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Issue #8: Sanitize path to prevent traversal attacks
    FString SanitizedPath = SanitizeProjectRelativePath(LevelPath);
    if (SanitizedPath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Invalid path (traversal/security violation): %s"), *LevelPath),
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
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             DeleteErrorMessage, nullptr, DeleteErrorCode);
      return true;
    }

    const FString AssetName = FPaths::GetBaseFilename(LongPackageName);
    const FString ObjectPath = AssetName.IsEmpty()
                                   ? LongPackageName
                                   : FString::Printf(TEXT("%s.%s"), *LongPackageName, *AssetName);
    FString MapFilename;
    FString AbsoluteMapFilename;
    const bool bHasMapFilename = FPackageName::TryConvertLongPackageNameToFilename(
        LongPackageName, MapFilename, FPackageName::GetMapPackageExtension());
    if (bHasMapFilename) {
      AbsoluteMapFilename = FPaths::ConvertRelativePathToFull(MapFilename);
      FPaths::NormalizeFilename(AbsoluteMapFilename);
    }

    IFileManager& FileManager = IFileManager::Get();

    RescanLevelPackageForDelete(LongPackageName, bHasMapFilename, AbsoluteMapFilename);

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

    const bool bMapFileExisted = bHasMapFilename && FileManager.FileExists(*AbsoluteMapFilename);
    const bool bPackageExisted = FPackageName::DoesPackageExist(LongPackageName);
    bool bDeletedViaFileFallback = false;
    bool bDeletedBuiltData = false;
    bool bBuiltDataExists = false;
    bool bExternalSidecarDeleteAttempted = false;
    bool bExternalSidecarDeleteFailed = false;
    bool bExternalActorsExists = false;
    bool bDeletedExternalActors = false;
    bool bExternalObjectsExists = false;
    bool bDeletedExternalObjects = false;
    FString ExternalDeleteErrorMessage;
    FString ExternalDeleteErrorCode;

    const FString BuiltDataPackagePath = LongPackageName + TEXT("_BuiltData");
    FString BuiltDataFilename;
    FString AbsoluteBuiltDataFilename;
    if (FPackageName::TryConvertLongPackageNameToFilename(
            BuiltDataPackagePath, BuiltDataFilename, FPackageName::GetAssetPackageExtension())) {
      AbsoluteBuiltDataFilename = FPaths::ConvertRelativePathToFull(BuiltDataFilename);
      FPaths::NormalizeFilename(AbsoluteBuiltDataFilename);
      bBuiltDataExists = FileManager.FileExists(*AbsoluteBuiltDataFilename);
    }

    if (!bCurrentWorldMatchesTarget && !bPackageStillLoaded && bMapFileExisted) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
             TEXT("delete_level: Deleting map file directly after registry/editor cleanup: %s"),
             *AbsoluteMapFilename);
      bDeletedViaFileFallback = FileManager.Delete(*AbsoluteMapFilename, false, true, true);

      if (bBuiltDataExists) {
        bDeletedBuiltData = FileManager.Delete(*AbsoluteBuiltDataFilename, false, true, true);
      }
    }

    if (!bCurrentWorldMatchesTarget && !bPackageStillLoaded &&
        (!bMapFileExisted || bDeletedViaFileFallback)) {
      bExternalSidecarDeleteAttempted = true;

      FString ActorsErrorMessage;
      FString ActorsErrorCode;
      if (!DeleteExternalPackageDirectory(LongPackageName, TEXT("__ExternalActors__"),
                                          bExternalActorsExists,
                                          bDeletedExternalActors,
                                          ActorsErrorMessage, ActorsErrorCode)) {
        bExternalSidecarDeleteFailed = true;
        ExternalDeleteErrorMessage = ActorsErrorMessage;
        ExternalDeleteErrorCode = ActorsErrorCode;
      }

      FString ObjectsErrorMessage;
      FString ObjectsErrorCode;
      if (!DeleteExternalPackageDirectory(LongPackageName, TEXT("__ExternalObjects__"),
                                          bExternalObjectsExists,
                                          bDeletedExternalObjects,
                                          ObjectsErrorMessage, ObjectsErrorCode)) {
        bExternalSidecarDeleteFailed = true;
        if (ExternalDeleteErrorMessage.IsEmpty()) {
          ExternalDeleteErrorMessage = ObjectsErrorMessage;
          ExternalDeleteErrorCode = ObjectsErrorCode;
        }
      }
    }

    RescanLevelPackageForDelete(LongPackageName, bHasMapFilename, AbsoluteMapFilename);

    const bool bMapFileStillExists = bHasMapFilename && FileManager.FileExists(*AbsoluteMapFilename);
    const bool bPackageStillExists = FPackageName::DoesPackageExist(LongPackageName);
    const bool bRemovedBuiltData = !bBuiltDataExists || bDeletedBuiltData;
    const bool bRemovedExternalActors = !bExternalActorsExists || bDeletedExternalActors;
    const bool bRemovedExternalObjects = !bExternalObjectsExists || bDeletedExternalObjects;
    const bool bDeleted = (bDeletedViaFileFallback && !bMapFileStillExists) ||
                          (!bMapFileExisted && !bPackageExisted && !bPackageStillLoaded);
    const bool bDeletedWithSidecars = bDeleted && !bExternalSidecarDeleteFailed &&
                                       bRemovedBuiltData &&
                                       bRemovedExternalActors && bRemovedExternalObjects;

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("levelPath"), LongPackageName);
    Result->SetStringField(TEXT("objectPath"), ObjectPath);
    Result->SetStringField(TEXT("mapFilename"), AbsoluteMapFilename);
    Result->SetBoolField(TEXT("deleted"), bDeletedWithSidecars);
    Result->SetBoolField(TEXT("deletedMapFile"), bDeletedViaFileFallback);
    Result->SetBoolField(TEXT("mapDeletedOrAlreadyAbsent"), bDeleted);
    Result->SetBoolField(TEXT("deletedViaFileFallback"), bDeletedViaFileFallback);
    Result->SetBoolField(TEXT("builtDataExists"), bBuiltDataExists);
    Result->SetBoolField(TEXT("deletedBuiltData"), bDeletedBuiltData);
    Result->SetBoolField(TEXT("externalSidecarDeleteAttempted"), bExternalSidecarDeleteAttempted);
    Result->SetBoolField(TEXT("externalSidecarDeleteFailed"), bExternalSidecarDeleteFailed);
    Result->SetBoolField(TEXT("externalActorsExists"), bExternalActorsExists);
    Result->SetBoolField(TEXT("deletedExternalActors"), bDeletedExternalActors);
    Result->SetBoolField(TEXT("externalObjectsExists"), bExternalObjectsExists);
    Result->SetBoolField(TEXT("deletedExternalObjects"), bDeletedExternalObjects);
    if (!ExternalDeleteErrorMessage.IsEmpty()) {
      Result->SetStringField(TEXT("externalDeleteError"), ExternalDeleteErrorMessage);
    }
    if (!ExternalDeleteErrorCode.IsEmpty()) {
      Result->SetStringField(TEXT("externalDeleteErrorCode"), ExternalDeleteErrorCode);
    }
    Result->SetBoolField(TEXT("editorDeletionSkippedForMap"), true);
    Result->SetBoolField(TEXT("deleteAssetFailed"), false);
    Result->SetBoolField(TEXT("wasLoaded"), bWasLoaded);
    Result->SetBoolField(TEXT("packageUnloadAttempted"), bPackageUnloadAttempted);
    Result->SetBoolField(TEXT("packageUnloadSucceeded"), bPackageUnloadSucceeded);
    Result->SetBoolField(TEXT("packageStillLoaded"), bPackageStillLoaded);
    Result->SetBoolField(TEXT("currentWorldMatchesTarget"), bCurrentWorldMatchesTarget);
    Result->SetNumberField(TEXT("removedStreamingRefs"), RemovedStreamingRefs);
    Result->SetBoolField(TEXT("fileExistsAfter"), bMapFileStillExists);
    Result->SetBoolField(TEXT("packageExistsAfter"), bPackageStillExists);

    if (bExternalSidecarDeleteFailed) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             ExternalDeleteErrorMessage, Result,
                             ExternalDeleteErrorCode.IsEmpty()
                                 ? TEXT("SOURCE_EXTERNAL_DELETE_FAILED")
                                 : ExternalDeleteErrorCode);
    } else if (bDeletedWithSidecars) {
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             FString::Printf(TEXT("Level file deleted: %s"), *LongPackageName), Result);
    } else if (bCurrentWorldMatchesTarget || bPackageStillLoaded) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Level is still loaded and cannot be deleted safely: %s"), *LongPackageName),
                             Result, TEXT("LEVEL_LOADED"));
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Failed to delete level: %s"), *LongPackageName),
                             Result, TEXT("DELETE_FAILED"));
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
