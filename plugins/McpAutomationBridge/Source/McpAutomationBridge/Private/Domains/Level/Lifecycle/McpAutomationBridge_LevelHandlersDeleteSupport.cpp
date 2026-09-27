#include "Domains/Level/Lifecycle/McpAutomationBridge_LevelHandlersDeletion.h"
#include "Domains/Level/Copy/McpAutomationBridge_LevelHandlersCopyOperations.h"
#include "Domains/Level/Lifecycle/McpAutomationBridge_LevelHandlersPathSafety.h"
#include "PackageTools.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "EditorLevelUtils.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "McpAutomationBridgeLog.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"

namespace McpLevelHandlers {
namespace {
void FlushDeleteGarbage() {
  FlushRenderingCommands();
  if (GEditor) {
    GEditor->ForceGarbageCollection(true);
  }
  CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
  FlushRenderingCommands();
}
} // namespace

int32 RemoveStreamingReferencesForLevelDelete(const FString& LongPackageName,
                                              const FString& ObjectPath,
                                              bool& bCurrentWorldMatchesTarget) {
  int32 RemovedStreamingRefs = 0;
  bCurrentWorldMatchesTarget = false;
  if (!GEditor) {
    return RemovedStreamingRefs;
  }

  UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
  if (!EditorWorld) {
    return RemovedStreamingRefs;
  }

  bCurrentWorldMatchesTarget = EditorWorld->GetOutermost() &&
                               EditorWorld->GetOutermost()->GetName() == LongPackageName;
  if (bCurrentWorldMatchesTarget) {
    return RemovedStreamingRefs;
  }

  TArray<ULevelStreaming*> StreamingLevels = EditorWorld->GetStreamingLevels();
  for (ULevelStreaming* StreamingLevel : StreamingLevels) {
    if (!StreamingLevel) {
      continue;
    }

    const FString StreamingPackage = StreamingLevel->GetWorldAssetPackageFName().ToString();
    if (StreamingPackage != LongPackageName && StreamingPackage != ObjectPath) {
      continue;
    }

    StreamingLevel->SetShouldBeLoaded(false);
    StreamingLevel->SetShouldBeVisible(false);
    if (ULevel* LoadedStreamingLevel = StreamingLevel->GetLoadedLevel()) {
      if (UEditorLevelUtils::RemoveLevelFromWorld(LoadedStreamingLevel)) {
        ++RemovedStreamingRefs;
      }
    } else {
      EditorWorld->RemoveStreamingLevel(StreamingLevel);
      ++RemovedStreamingRefs;
    }
  }

  if (RemovedStreamingRefs > 0) {
    FlushDeleteGarbage();
  }
  return RemovedStreamingRefs;
}

void TryUnloadLoadedLevelPackageForDelete(const FString& LongPackageName,
                                          bool bCurrentWorldMatchesTarget,
                                          UPackage*& LoadedPackage,
                                          bool& bPackageUnloadAttempted,
                                          bool& bPackageUnloadSucceeded) {
  bPackageUnloadAttempted = false;
  bPackageUnloadSucceeded = false;
  if (!LoadedPackage || bCurrentWorldMatchesTarget) {
    return;
  }

  FlushDeleteGarbage();
  LoadedPackage = FindPackage(nullptr, *LongPackageName);
  if (!LoadedPackage) {
    return;
  }

  TArray<UPackage*> PackagesToUnload;
  PackagesToUnload.Add(LoadedPackage);
  TWeakObjectPtr<UPackage> WeakLoadedPackage = LoadedPackage;
  FText UnloadError;
  bPackageUnloadAttempted = true;
  bPackageUnloadSucceeded = UPackageTools::UnloadPackages(PackagesToUnload, UnloadError, true);
  if (!UnloadError.IsEmpty()) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("delete_level: UnloadPackages reported for %s: %s"),
           *LongPackageName, *UnloadError.ToString());
  }
  FlushDeleteGarbage();
  LoadedPackage = FindPackage(nullptr, *LongPackageName);
  bPackageUnloadSucceeded = bPackageUnloadSucceeded &&
      !WeakLoadedPackage.IsValid() && LoadedPackage == nullptr;
}

void DeleteLevelFiles(const FString& PackagePath, bool bDelete, FLevelFileDeletion& Out) {
  IFileManager& FileManager = IFileManager::Get();
  TryGetAbsoluteMapFilename(PackagePath, Out.MapFilename);
  Out.bMapExisted = !Out.MapFilename.IsEmpty() && FileManager.FileExists(*Out.MapFilename);
  FString BuiltDataFilename;
  if (FPackageName::TryConvertLongPackageNameToFilename(PackagePath + TEXT("_BuiltData"), BuiltDataFilename,
                                                        FPackageName::GetAssetPackageExtension())) {
    BuiltDataFilename = FPaths::ConvertRelativePathToFull(BuiltDataFilename);
    FPaths::NormalizeFilename(BuiltDataFilename);
    Out.bBuiltDataExists = FileManager.FileExists(*BuiltDataFilename);
  }
  if (!bDelete) {
    return;
  }
  Out.bDeletedMap = Out.bMapExisted && FileManager.Delete(*Out.MapFilename, false, true, true);
  Out.bDeletedBuiltData = Out.bBuiltDataExists && FileManager.Delete(*BuiltDataFilename, false, true, true);
  if (Out.bMapExisted && !Out.bDeletedMap) {
    return;
  }
  Out.bSidecarDeleteAttempted = true;
  const TCHAR* Roots[] = {TEXT("__ExternalActors__"), TEXT("__ExternalObjects__")};
  bool* Exists[] = {&Out.bExternalActorsExists, &Out.bExternalObjectsExists};
  bool* Deleted[] = {&Out.bDeletedExternalActors, &Out.bDeletedExternalObjects};
  for (int32 Index = 0; Index < 2; ++Index) {
    FString Message, Code;
    if (!DeleteExternalPackageDirectory(PackagePath, Roots[Index], *Exists[Index], *Deleted[Index],
                                        Message, Code) &&
        Out.SidecarErrorMessage.IsEmpty()) {
      Out.SidecarErrorMessage = Message;
      Out.SidecarErrorCode = Code;
    }
  }
}
} // namespace McpLevelHandlers
