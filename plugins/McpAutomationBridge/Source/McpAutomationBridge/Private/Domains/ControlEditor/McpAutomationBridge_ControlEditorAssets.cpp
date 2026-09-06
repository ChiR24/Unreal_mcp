#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"
#include "Foundation/McpCompensationReceipt.h"

bool UMcpAutomationBridgeSubsystem::HandleControlEditorOpenAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("assetPath required"), nullptr);
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SECURITY_VIOLATION"),
                              TEXT("Invalid assetPath"), nullptr);
    return true;
  }

  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  UAssetEditorSubsystem *AssetEditorSS =
      GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
  if (!AssetEditorSS) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SUBSYSTEM_MISSING"),
                              TEXT("AssetEditorSubsystem not available"), nullptr);
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ASSET_NOT_FOUND"),
                              TEXT("Asset not found"), nullptr);
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  if (!Asset) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("LOAD_FAILED"),
                              TEXT("Failed to load asset"), nullptr);
    return true;
  }

  if (FParse::Param(FCommandLine::Get(), TEXT("NullRHI"))) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetStringField(TEXT("assetClass"), Asset->GetClass()->GetName());
    Resp->SetBoolField(TEXT("loaded"), true);
    Resp->SetBoolField(TEXT("editorOpened"), false);
    Resp->SetBoolField(TEXT("headlessSafe"), true);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Asset loaded; editor window skipped under NullRHI"), Resp,
                           FString());
    return true;
  }

  const bool bOpened = AssetEditorSS->OpenEditorForAsset(Asset);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), bOpened);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);

  if (bOpened) {
    SendAutomationResponse(Socket, RequestId, true, TEXT("Asset opened"), Resp,
                           FString());
  } else {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("OPEN_FAILED"),
                              TEXT("Failed to open asset editor"), Resp);
  }
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorCloseAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("assetPath required"), nullptr);
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SECURITY_VIOLATION"),
                              TEXT("Invalid assetPath"), nullptr);
    return true;
  }

  UAssetEditorSubsystem* AssetEditorSS = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
  if (!AssetEditorSS) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SUBSYSTEM_MISSING"),
                              TEXT("AssetEditorSubsystem unavailable"), nullptr);
    return true;
  }

  UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  if (!Asset) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("LOAD_FAILED"),
                              TEXT("Failed to load asset"), nullptr);
    return true;
  }

  if (FParse::Param(FCommandLine::Get(), TEXT("NullRHI"))) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetBoolField(TEXT("loaded"), true);
    Resp->SetBoolField(TEXT("editorClosed"), false);
    Resp->SetBoolField(TEXT("headlessSafe"), true);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Asset verified; editor close skipped under NullRHI"), Resp,
                           FString());
    return true;
  }

  AssetEditorSS->CloseAllEditorsForAsset(Asset);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Asset editor closed"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSaveAll(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  TArray<UPackage*> DirtyWorldPackages;
  TArray<UPackage*> DirtyContentPackages;
  FEditorFileUtils::GetDirtyWorldPackages(DirtyWorldPackages);
  FEditorFileUtils::GetDirtyContentPackages(DirtyContentPackages);

  bool bSuccess = true;
  int32 SavedWorldCount = 0;
  int32 SavedContentCount = 0;
  int32 SkippedCount = 0;
  int32 TotalDirty = 0;
  TArray<FString> SkippedPackages;
  TArray<FString> FailedPackages;
  TSet<UPackage*> ProcessedPackages;

  // Each package lands independently and the ones that landed stay landed, so a
  // partial failure here has no rollback -- only a compensating next step.
  FMcpCompensationReceipt Receipt(TEXT("control_editor.save_all"));

  auto ShouldSkipPackage = [](UPackage* Package) -> bool {
    if (!Package || Package->HasAnyFlags(RF_Transient)) {
      return true;
    }
    const FString PackagePath = Package->GetPathName();
    return PackagePath.StartsWith(TEXT("/Temp/")) ||
           PackagePath.StartsWith(TEXT("/Transient/")) ||
           PackagePath.StartsWith(TEXT("/Engine/Transient"));
  };

  auto ProcessPackage = [&](UPackage* Package) {
    if (!Package || ProcessedPackages.Contains(Package)) {
      return;
    }
    ProcessedPackages.Add(Package);
    TotalDirty++;

    FString PackagePath = Package->GetPathName();
    if (ShouldSkipPackage(Package)) {
      SkippedCount++;
      SkippedPackages.Add(PackagePath);
      Receipt.NoteSkipped(FString::Printf(TEXT("save:%s"), *PackagePath),
                          TEXT("transient or temporary package"));
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("HandleControlEditorSaveAll: Skipping transient/temp package: %s"), *PackagePath);
      return;
    }

    const FString StepId = FString::Printf(TEXT("save:%s"), *PackagePath);

    UWorld* PackageWorld = UWorld::FindWorldInPackage(Package);
    if (PackageWorld) {
      if (PackageWorld->PersistentLevel && McpSafeLevelSave(PackageWorld->PersistentLevel, PackagePath)) {
        SavedWorldCount++;
        Receipt.NoteCompleted(StepId, TEXT("level package written to disk"));
      } else {
        bSuccess = false;
        FailedPackages.Add(PackagePath);
        Receipt.NoteNotCompleted(StepId, TEXT("level save failed; this package is unchanged on disk"));
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
               TEXT("HandleControlEditorSaveAll: Failed to save world package: %s"), *PackagePath);
      }
      return;
    }

    if (McpSafeAssetSave(Package)) {
      SavedContentCount++;
      Receipt.NoteCompleted(StepId, TEXT("content package written to disk"));
    } else {
      bSuccess = false;
      FailedPackages.Add(PackagePath);
      Receipt.NoteNotCompleted(StepId, TEXT("content save failed; this package is unchanged on disk"));
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("HandleControlEditorSaveAll: Failed to save content package: %s"), *PackagePath);
    }
  };

  for (UPackage* Package : DirtyWorldPackages) {
    ProcessPackage(Package);
  }

  for (UPackage* Package : DirtyContentPackages) {
    ProcessPackage(Package);
  }

  auto MakeStringArray = [](const TArray<FString>& Values) {
    TArray<TSharedPtr<FJsonValue>> Result;
    for (const FString& Value : Values) {
      Result.Add(MakeShared<FJsonValueString>(Value));
    }
    return Result;
  };

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), bSuccess);
  Resp->SetNumberField(TEXT("savedCount"), SavedWorldCount + SavedContentCount);
  Resp->SetNumberField(TEXT("savedWorldCount"), SavedWorldCount);
  Resp->SetNumberField(TEXT("savedContentCount"), SavedContentCount);
  Resp->SetNumberField(TEXT("skippedCount"), SkippedCount);
  Resp->SetNumberField(TEXT("failedCount"), FailedPackages.Num());
  Resp->SetNumberField(TEXT("totalDirty"), TotalDirty);
  Resp->SetArrayField(TEXT("skippedPackages"), MakeStringArray(SkippedPackages));
  Resp->SetArrayField(TEXT("failedPackages"), MakeStringArray(FailedPackages));

  if (FailedPackages.Num() > 0) {
    Receipt.AddCompensatingCapability(TEXT("control_editor.save_all"));
    Receipt.SetCallerAction(FString::Printf(
        TEXT("%d package(s) are already written to disk and stay written. Resolve the listed "
             "failures (check out or clear the read-only flag on failedPackages) and call "
             "control_editor.save_all again to finish the remaining %d."),
        SavedWorldCount + SavedContentCount, FailedPackages.Num()));
  } else {
    Receipt.SetCallerAction(
        TEXT("Nothing outstanding. The saved packages are durable and are not undoable."));
  }
  Receipt.DescribeInto(Resp);

  if (bSuccess || TotalDirty == 0) {
    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Saved %d world and %d content packages (skipped %d transient/temp)"), SavedWorldCount, SavedContentCount, SkippedCount),
                           Resp, FString());
  } else {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SAVE_FAILED"),
                              FString::Printf(TEXT("Failed to save all packages. Saved %d of %d dirty packages."),
                                               SavedWorldCount + SavedContentCount,
                                               TotalDirty - SkippedCount),
                              Resp);
  }
  return true;
#else
  return false;
#endif
}
