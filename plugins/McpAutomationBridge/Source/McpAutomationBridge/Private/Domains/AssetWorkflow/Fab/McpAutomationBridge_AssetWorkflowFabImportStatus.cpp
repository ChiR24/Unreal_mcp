// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Domains/AssetWorkflow/Fab/McpAutomationBridge_FabImportJson.h"
#include "McpFabProvider.h"

#include "Dom/JsonObject.h"

namespace
{
FString StepFor(const FMcpFabImportStatus &Status) {
  if (Status.Phase == TEXT("cancelling")) {
    return TEXT("Cancel was requested and Fab has been told to stop; the next read says failed with CANCELLED, listing anything that had already landed.");
  }
  if (Status.Result.ErrorCode == TEXT("CANCELLED")) {
    return TEXT("The import was cancelled. Anything that had already landed is listed under importedRoot and was left unsaved and unmoved: asset.delete removes it. Add the listing again to retry.");
  }
  if (Status.Phase == TEXT("done")) {
    return TEXT("The import finished: importedRoot and sampleAssetPaths say where it landed, the meshes first. asset.move relocates a folder and fixes redirectors.");
  }
  if (Status.Phase == TEXT("failed")) {
    return TEXT("The import did not finish; failureCode and failure say why, and what landed before it stopped is under importedRoot. Fab's own log lines are listed under fabErrors, and system_control read_log with filter LogFab shows the rest. Add the listing again to retry.");
  }
  if (Status.Phase == TEXT("queued")) {
    return TEXT("Queued behind the import that is running: it starts by itself, in order, when that one ends. The queue lists what is ahead. Poll again in 20 to 30 seconds.");
  }
  return TEXT("Still running: poll again in 20 to 30 seconds. While Fab imports, the editor is held and every call, this read included, answers EDITOR_BLOCKED; that is the import working, not a failure.");
}

/** What a caller must not miss: packages the import left in memory only, which an editor restart or crash loses. */
FString UnsavedWarning(const FMcpFabImportStatus &Status) {
  const int32 Count = Status.Result.UnsavedPackages.Num();
  return Count > 0 ? FString::Printf(TEXT("%d package(s) are NOT SAVED (unsavedPackages): they exist only in memory until control_editor save_all writes them. "
                                          "The import is saved again 15 and 60 seconds after it ends, so read this once more before saving by hand."), Count)
                   : FString();
}

FString NextStep(const FMcpFabImportStatus &Status) {
  const FString Warning = UnsavedWarning(Status);
  return Warning.IsEmpty() ? StepFor(Status) : Warning + TEXT(" ") + StepFor(Status);
}

/** One line of the queue: enough to tell the imports apart and see how far each has come. */
TSharedPtr<FJsonValue> QueueRow(const FMcpFabImportStatus &Entry) {
  TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
  Row->SetStringField(TEXT("operationId"), Entry.OperationId);
  Row->SetStringField(TEXT("listingId"), Entry.ListingId);
  if (!Entry.Result.Title.IsEmpty()) {
    Row->SetStringField(TEXT("title"), Entry.Result.Title);
  }
  Row->SetStringField(TEXT("phase"), Entry.Phase);
  if (Entry.QueuePosition > 0) {
    Row->SetNumberField(TEXT("queuePosition"), Entry.QueuePosition);
  }
  Row->SetNumberField(TEXT("elapsedSeconds"), FMath::RoundToInt(Entry.ElapsedSeconds));
  if (Entry.DownloadedBytes >= 0) {
    Row->SetNumberField(TEXT("downloadedBytes"), static_cast<double>(Entry.DownloadedBytes));
  }
  if (Entry.DownloadPercent >= 0.0f) {
    Row->SetNumberField(TEXT("downloadPercent"), FMath::RoundToInt(Entry.DownloadPercent));
  }
  return MakeShared<FJsonValueObject>(Row);
}
} // namespace

/**
 * Reports one Fab import the add started, by operation id or by listing id, and the queue it is in.
 *
 * The add answers when Fab accepts the download; the download and the import
 * that follows run on in the background, one at a time, with later adds queued
 * behind them. This reads what the adapter has learned since -- the phase, how
 * much has downloaded when the cache shows it, and, once the asset registry
 * settles, where the content landed -- from the operation store, so it never
 * touches Fab's page and needs no sign-in.
 */
bool UMcpAutomationBridgeSubsystem::HandleGetFabImportStatus(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  IMcpFabProvider *Provider = GetMcpFabProvider();
  if (Provider == nullptr || !Provider->IsFabAvailable()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("Fab support is not loaded in this editor, so there is no import to report."),
        nullptr, TEXT("NOT_SUPPORTED"));
    return true;
  }

  FString Key;
  Payload->TryGetStringField(TEXT("operationId"), Key);
  if (Key.IsEmpty()) {
    Payload->TryGetStringField(TEXT("listingId"), Key);
  }
  if (!Key.IsEmpty() && !McpFabImportJson::IsPlainKey(Key)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("'operationId' (from the add's reply) and 'listingId' (its newest import is reported) are each [A-Za-z0-9_-], 64 characters at most."),
        nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  TArray<FMcpFabImportStatus> Queue;
  Provider->GetImportQueue(Queue);
  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  TArray<TSharedPtr<FJsonValue>> Rows;
  for (const FMcpFabImportStatus &Entry : Queue) {
    Rows.Add(QueueRow(Entry));
  }
  if (Rows.Num() > 0) {
    Data->SetArrayField(TEXT("queue"), Rows);
  }
  Data->SetNumberField(TEXT("queueLength"), Rows.Num());

  // No id: the caller wants the whole picture, which is the queue.
  if (Key.IsEmpty()) {
    Data->SetStringField(
        TEXT("note"),
        Rows.Num() > 0 ? TEXT("The import that is running comes first, then the adds waiting behind it in the order they start. Pass operationId to read one in full.")
                       : TEXT("Nothing is running or queued. Pass operationId or listingId to read a finished import."));
    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("%d Fab import(s) running or queued."), Rows.Num()), Data);
    return true;
  }

  FMcpFabImportStatus Status;
  if (!Provider->GetImportStatus(Key, Status)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("No Fab import matches '%s'. The editor keeps the last 16 imports of this session and forgets them on restart; add the listing to start one."), *Key),
        nullptr, TEXT("NOT_FOUND"));
    return true;
  }

  const FMcpFabAddResult &Result = Status.Result;
  const bool bFinished = Status.Phase == TEXT("done") || Status.Phase == TEXT("failed");
  Data->SetStringField(TEXT("operationId"), Status.OperationId);
  Data->SetStringField(TEXT("listingId"), Status.ListingId);
  Data->SetStringField(TEXT("phase"), Status.Phase);
  Data->SetBoolField(TEXT("finished"), bFinished);
  Data->SetNumberField(TEXT("elapsedSeconds"), FMath::RoundToInt(Status.ElapsedSeconds));
  if (Status.QueuePosition > 0) {
    Data->SetNumberField(TEXT("queuePosition"), Status.QueuePosition);
  }
  McpFabImportJson::SetAddFacts(Data, Result);
  // Present only when the add asked for separate meshes and Fab merges them for this listing.
  if (Result.MeshesSeparated.IsSet()) {
    Data->SetBoolField(TEXT("combineMeshesApplied"), Result.MeshesSeparated.GetValue());
  }
  // Only what is known: a download nobody can observe has no byte count, and zero would be a claim.
  if (Status.DownloadedBytes >= 0) {
    Data->SetNumberField(TEXT("downloadedBytes"), static_cast<double>(Status.DownloadedBytes));
  }
  // The percent Fab's own download notification shows; absent when there is no notification to read.
  if (Status.DownloadPercent >= 0.0f) {
    Data->SetNumberField(TEXT("downloadPercent"), FMath::RoundToInt(Status.DownloadPercent));
  }
  if (!bFinished) {
    Data->SetBoolField(TEXT("cancellable"), Status.bCancellable);
  }
  if (Status.AssetsSoFar > 0) {
    Data->SetNumberField(TEXT("assetsSoFar"), Status.AssetsSoFar);
  }
  if (bFinished && Result.AssetCount > 0) {
    Data->SetNumberField(TEXT("assetCount"), Result.AssetCount);
    Data->SetStringField(TEXT("importedRoot"), Result.RootPath);
    TArray<TSharedPtr<FJsonValue>> Samples;
    for (const FString &Path : Result.SamplePaths) {
      Samples.Add(MakeShared<FJsonValueString>(Path));
    }
    Data->SetArrayField(TEXT("sampleAssetPaths"), Samples);
  }
  if (Result.bRelocateRan) {
    Data->SetBoolField(TEXT("relocated"), Result.MovedCount > 0);
    Data->SetNumberField(TEXT("movedCount"), Result.MovedCount);
    if (!Result.RelocationNote.IsEmpty()) {
      Data->SetStringField(TEXT("relocationNote"), Result.RelocationNote);
    }
  }
  if (Result.bSaveRan) {
    Data->SetBoolField(TEXT("saved"), Result.UnsavedPackages.Num() == 0);
    Data->SetNumberField(TEXT("savedCount"), Result.SavedCount);
    if (Result.UnsavedPackages.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> Unsaved;
      for (const FString &Name : Result.UnsavedPackages) {
        Unsaved.Add(MakeShared<FJsonValueString>(Name));
      }
      Data->SetArrayField(TEXT("unsavedPackages"), Unsaved);
    }
  }
  if (Status.Phase == TEXT("failed")) {
    Data->SetStringField(TEXT("failureCode"), Result.ErrorCode);
    Data->SetStringField(TEXT("failure"), Result.Error);
  }
  if (Status.FabErrors.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> Lines;
    for (const FString &Line : Status.FabErrors) {
      Lines.Add(MakeShared<FJsonValueString>(Line));
    }
    Data->SetArrayField(TEXT("fabErrors"), Lines);
  }
  const TCHAR *TaskState = Status.Phase == TEXT("done")     ? TEXT("completed")
                           : Status.Phase == TEXT("failed") ? TEXT("failed")
                           : Status.Phase == TEXT("queued") ? TEXT("queued")
                                                            : TEXT("running");
  Data->SetObjectField(TEXT("task"), McpFabImportJson::MakeTask(Status.OperationId, TaskState));
  Data->SetStringField(TEXT("note"), NextStep(Status));

  // The line a caller reads first carries it too: a package that is not saved is lost by a restart or a crash.
  const int32 Unsaved = Result.UnsavedPackages.Num();
  SendAutomationResponse(
      Socket, RequestId, true,
      FString::Printf(TEXT("Fab import %s of %s is %s after %d s.%s"), *Status.OperationId, *Status.ListingId,
                      *Status.Phase, FMath::RoundToInt(Status.ElapsedSeconds),
                      Unsaved > 0 ? *FString::Printf(TEXT(" %d package(s) are NOT SAVED."), Unsaved) : TEXT("")),
      Data);
  return true;
}
