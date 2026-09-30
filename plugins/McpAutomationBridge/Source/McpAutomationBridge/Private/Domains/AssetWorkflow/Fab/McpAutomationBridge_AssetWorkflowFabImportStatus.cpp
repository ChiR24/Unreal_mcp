// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Domains/AssetWorkflow/Fab/McpAutomationBridge_FabImportJson.h"
#include "McpFabProvider.h"

#include "Dom/JsonObject.h"

namespace
{
/** An operation id or a listing id: the characters both use, so nothing else is ever looked up. */
bool IsPlainKey(const FString &Key) {
  if (Key.IsEmpty() || Key.Len() > 64) {
    return false;
  }
  for (const TCHAR Ch : Key) {
    if (!FChar::IsAlnum(Ch) && Ch != TEXT('-') && Ch != TEXT('_')) {
      return false;
    }
  }
  return true;
}

FString NextStep(const FMcpFabImportStatus &Status) {
  if (Status.Phase == TEXT("done")) {
    return TEXT("The import finished: importedRoot and sampleAssetPaths say where it landed, the meshes first. asset.move relocates a folder and fixes redirectors.");
  }
  if (Status.Phase == TEXT("failed")) {
    return TEXT("The import did not finish; failureCode and failure say why, and what landed before it stopped is under importedRoot. Fab's own log lines are listed under fabErrors, and system_control read_log with filter LogFab shows the rest. Add the listing again to retry.");
  }
  return TEXT("Still running: poll again in 20 to 30 seconds. While Fab imports, the editor is held and every call, this read included, answers EDITOR_BLOCKED; that is the import working, not a failure.");
}
} // namespace

/**
 * Reports one Fab import the add started, by operation id or by listing id.
 *
 * The add answers when Fab accepts the download; the download and the import
 * that follows run on in the background. This reads what the adapter has
 * learned since -- the phase, how much has downloaded when the cache shows it,
 * and, once the asset registry settles, where the content landed -- from the
 * operation store, so it never touches Fab's page and needs no sign-in.
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
  if (!IsPlainKey(Key)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("Pass 'operationId' (from the add's reply) or 'listingId' (its newest import is reported); each is [A-Za-z0-9_-], 64 characters at most."),
        nullptr, TEXT("INVALID_ARGUMENT"));
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
  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("operationId"), Status.OperationId);
  Data->SetStringField(TEXT("listingId"), Status.ListingId);
  Data->SetStringField(TEXT("phase"), Status.Phase);
  Data->SetBoolField(TEXT("finished"), bFinished);
  Data->SetNumberField(TEXT("elapsedSeconds"), FMath::RoundToInt(Status.ElapsedSeconds));
  McpFabImportJson::SetAddFacts(Data, Result);
  // Only what is known: a download nobody can observe has no byte count, and zero would be a claim.
  if (Status.DownloadedBytes >= 0) {
    Data->SetNumberField(TEXT("downloadedBytes"), static_cast<double>(Status.DownloadedBytes));
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
  Data->SetObjectField(
      TEXT("task"),
      McpFabImportJson::MakeTask(Status.OperationId, Status.Phase == TEXT("done") ? TEXT("completed")
                                                     : Status.Phase == TEXT("failed") ? TEXT("failed")
                                                                                      : TEXT("running")));
  Data->SetStringField(TEXT("note"), NextStep(Status));

  SendAutomationResponse(
      Socket, RequestId, true,
      FString::Printf(TEXT("Fab import %s of %s is %s after %d s."), *Status.OperationId, *Status.ListingId,
                      *Status.Phase, FMath::RoundToInt(Status.ElapsedSeconds)),
      Data);
  return true;
}
