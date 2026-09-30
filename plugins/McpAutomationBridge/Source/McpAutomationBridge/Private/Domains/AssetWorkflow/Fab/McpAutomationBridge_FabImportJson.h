// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"
#include "McpFabProvider.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// Reply pieces the Fab add and its status read share.
namespace McpFabImportJson
{
/** The receipt's task: which operation this is and whether it is still going (running, completed, failed). */
inline TSharedPtr<FJsonObject> MakeTask(const FString& OperationId, const TCHAR* State)
{
	TSharedPtr<FJsonObject> Task = MakeShared<FJsonObject>();
	Task->SetStringField(TEXT("taskId"), OperationId);
	Task->SetStringField(TEXT("state"), State);
	return Task;
}

/** The executable gateway call that reads one operation's status, offered wherever a caller should poll. */
inline TSharedPtr<FJsonObject> MakeStatusNextCall(const FString& OperationId)
{
	TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("lookup"), TEXT("fab_import_status"));
	Params->SetStringField(TEXT("operationId"), OperationId);
	TSharedPtr<FJsonObject> Next = MakeShared<FJsonObject>();
	Next->SetStringField(TEXT("operation"), TEXT("execute"));
	Next->SetStringField(TEXT("tool"), TEXT("manage_asset"));
	Next->SetStringField(TEXT("action"), TEXT("query_marketplace"));
	Next->SetObjectField(TEXT("params"), Params);
	return Next;
}

/** What the page decided for an add: format, quality tier, file, size, engine version and whether meshes merge. */
inline void SetAddFacts(const TSharedPtr<FJsonObject>& Data, const FMcpFabAddResult& Result)
{
	Data->SetBoolField(TEXT("combinesMeshes"), Result.bMergesMeshes);
	if (!Result.FormatCode.IsEmpty()) { Data->SetStringField(TEXT("formatCode"), Result.FormatCode); }
	if (!Result.Quality.IsEmpty()) { Data->SetStringField(TEXT("quality"), Result.Quality); }
	if (!Result.FileName.IsEmpty()) { Data->SetStringField(TEXT("fileName"), Result.FileName); }
	// Unknown is not zero: a pack publishes no size, so the field is left out rather than reported as 0.
	if (Result.DownloadBytes >= 0) { Data->SetNumberField(TEXT("downloadBytes"), static_cast<double>(Result.DownloadBytes)); }
	if (!Result.VersionName.IsEmpty()) { Data->SetStringField(TEXT("versionName"), Result.VersionName); }
	Data->SetBoolField(TEXT("engineExactMatch"), Result.bEngineExactMatch);
}
} // namespace McpFabImportJson
