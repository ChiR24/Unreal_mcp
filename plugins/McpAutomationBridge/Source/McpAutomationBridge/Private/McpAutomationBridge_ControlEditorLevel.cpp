#include "McpAutomationBridge_ControlEditorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleControlEditorOpenLevel(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString LevelPath;
  // Accept multiple parameter names for flexibility
  // levelPath is the primary, path and assetPath are aliases
  Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
  if (LevelPath.IsEmpty()) {
    Payload->TryGetStringField(TEXT("path"), LevelPath);
  }
  if (LevelPath.IsEmpty()) {
    Payload->TryGetStringField(TEXT("assetPath"), LevelPath);
  }
  if (LevelPath.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("levelPath, path, or assetPath required"), nullptr);
    return true;
  }

  // Normalize the level path
  if (!LevelPath.StartsWith(TEXT("/"))) {
    LevelPath = FString::Printf(TEXT("/Game/%s"), *LevelPath);
  }

  LevelPath = SanitizeProjectRelativePath(LevelPath);
  if (LevelPath.IsEmpty() ||
      !(LevelPath.StartsWith(TEXT("/Game/")) || LevelPath.StartsWith(TEXT("/Engine/")))) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SECURITY_VIOLATION"),
                              TEXT("Invalid levelPath"), nullptr);
    return true;
  }

  // Remove map suffix if present
  if (LevelPath.EndsWith(TEXT(".umap"))) {
    LevelPath.LeftChopInline(5);
  }

  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  // Use FEditorFileUtils to load the map
  FString MapPath = LevelPath + TEXT(".umap");

  // CRITICAL FIX: Unreal stores levels in TWO possible path patterns:
  // 1. Folder-based (standard): /Game/Path/LevelName/LevelName.umap
  // 2. Flat (legacy): /Game/Path/LevelName.umap
  // We must check BOTH paths before returning FILE_NOT_FOUND.

  // Build both possible paths
  FString FlatMapPath = LevelPath + TEXT(".umap");
  // Check if path is /Engine/ or /Game/ and extract accordingly
  int32 PrefixLen = 6; // Default: "/Game/" is 6 chars
  FString ContentDir = FPaths::ProjectContentDir();
  if (LevelPath.StartsWith(TEXT("/Engine/"))) {
    PrefixLen = 8; // "/Engine/" is 8 chars
    ContentDir = FPaths::EngineContentDir();
  }
  FString FullFlatMapPath = ContentDir + FlatMapPath.Mid(PrefixLen);
  FullFlatMapPath = FPaths::ConvertRelativePathToFull(FullFlatMapPath);

  // Folder-based path: /Game/Path/LevelName -> /Game/Path/LevelName/LevelName.umap
  FString LevelName = FPaths::GetBaseFilename(LevelPath);
  FString FolderMapPath = LevelPath + TEXT("/") + LevelName + TEXT(".umap");
  FString FullFolderMapPath = ContentDir + FolderMapPath.Mid(PrefixLen);
  FullFolderMapPath = FPaths::ConvertRelativePathToFull(FullFolderMapPath);

  // Check which path exists
  FString MapPathToLoad;
  FString FullMapPath;

  // Prefer folder-based path (Unreal's standard) if it exists
  if (FPaths::FileExists(FullFolderMapPath)) {
    MapPathToLoad = FolderMapPath;
    FullMapPath = FullFolderMapPath;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
           TEXT("OpenLevel: Found level at folder-based path: %s"), *FullFolderMapPath);
  } else if (FPaths::FileExists(FullFlatMapPath)) {
    // Fallback to flat path (legacy format)
    MapPathToLoad = FlatMapPath;
    FullMapPath = FullFlatMapPath;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
           TEXT("OpenLevel: Found level at flat path: %s"), *FullFlatMapPath);
  } else {
    // Neither path exists - return detailed error
    TSharedPtr<FJsonObject> ErrorDetails = McpHandlerUtils::CreateResultObject();
    ErrorDetails->SetStringField(TEXT("levelPath"), LevelPath);
    ErrorDetails->SetStringField(TEXT("checkedFolderBased"), FullFolderMapPath);
    ErrorDetails->SetStringField(TEXT("checkedFlat"), FullFlatMapPath);
    ErrorDetails->SetStringField(TEXT("hint"), TEXT("Unreal levels are typically stored as /Game/Path/LevelName/LevelName.umap"));
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("FILE_NOT_FOUND"),
                              FString::Printf(TEXT("Level file not found. Checked:\n  Folder: %s\n  Flat: %s"),
                                            *FullFolderMapPath, *FullFlatMapPath),
                              ErrorDetails);
    return true;
  }

  if (FApp::IsUnattended() || IsRunningCommandlet() || FParse::Param(FCommandLine::Get(), TEXT("nullrhi"))) {
    TArray<UPackage*> DirtyWorldPackages;
    TArray<UPackage*> DirtyContentPackages;
    FEditorFileUtils::GetDirtyWorldPackages(DirtyWorldPackages);
    FEditorFileUtils::GetDirtyContentPackages(DirtyContentPackages);
    auto IsBlockingDirtyPackage = [](UPackage* Package) -> bool {
      if (!Package || Package->HasAnyFlags(RF_Transient)) {
        return false;
      }
      const FString PackagePath = Package->GetPathName();
      return !PackagePath.StartsWith(TEXT("/Temp/")) &&
             !PackagePath.StartsWith(TEXT("/Transient/")) &&
             !PackagePath.StartsWith(TEXT("/Engine/Transient"));
    };
    int32 BlockingWorldPackages = 0;
    int32 BlockingContentPackages = 0;
    for (UPackage* Package : DirtyWorldPackages) {
      if (IsBlockingDirtyPackage(Package)) {
        BlockingWorldPackages++;
      }
    }
    for (UPackage* Package : DirtyContentPackages) {
      if (IsBlockingDirtyPackage(Package)) {
        BlockingContentPackages++;
      }
    }
    if (BlockingWorldPackages + BlockingContentPackages > 0) {
      TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
      Details->SetNumberField(TEXT("dirtyWorldPackages"), BlockingWorldPackages);
      Details->SetNumberField(TEXT("dirtyContentPackages"), BlockingContentPackages);
      Details->SetStringField(TEXT("levelPath"), LevelPath);
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("DIRTY_PACKAGES"),
                                TEXT("Cannot open a level in unattended/headless mode while packages are dirty. Save or discard changes first."),
                                Details);
      return true;
    }
  }

  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
  FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateLambda([WeakThis, Socket, RequestId, LevelPath,
                                     MapPathToLoad](float) {
        if (!WeakThis.IsValid()) {
          return false;
        }

        bool bOpened = McpSafeLoadMap(MapPathToLoad);

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("success"), bOpened);
        Resp->SetStringField(TEXT("levelPath"), LevelPath);
        Resp->SetStringField(TEXT("loadedPath"), MapPathToLoad);

        if (bOpened) {
          UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
                 TEXT("OpenLevel: Successfully opened level: %s"), *MapPathToLoad);
          WeakThis->SendAutomationResponse(Socket, RequestId, true,
                                           TEXT("Level opened"), Resp, FString());
        } else {
          SendStandardErrorResponse(WeakThis.Get(), Socket, RequestId,
                                    TEXT("OPEN_FAILED"),
                                    TEXT("Failed to open level"), Resp);
        }

        return false;
      }),
      0.0f);
  return true;
#else
  return false;
#endif
}
