// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpFabAddReply.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpFabAddReply, Log, All);

namespace McpFabAddOperation
{
namespace
{
/** Says which step Fab refused at when it would give no download address, in Fab's own words. */
FString DescribeNoDownload(const TSharedPtr<FJsonObject>& Root, const FString& FileName)
{
	FString Step;
	FString Detail;
	double Status = 0.0;
	if (Root.IsValid())
	{
		Root->TryGetStringField(TEXT("failedStep"), Step);
		Root->TryGetStringField(TEXT("stepDetail"), Detail);
		Root->TryGetNumberField(TEXT("stepStatus"), Status);
	}
	const FString Words = Detail.IsEmpty() ? FString(TEXT("no reason given")) : Detail;
	if (Step == TEXT("license"))
	{
		return FString::Printf(
			TEXT("Fab gave no download for %s: the license step failed (%s), so it cannot be added to a library."), *FileName, *Words);
	}
	if (Step == TEXT("claim"))
	{
		return FString::Printf(
			TEXT("Fab gave no download for %s: the claim step failed (HTTP %.0f: %s). The account could not add this listing to its library, and Fab serves a download only for a listing it holds."),
			*FileName, Status, *Words);
	}
	if (Step == TEXT("download-info"))
	{
		return FString::Printf(
			TEXT("Fab gave no download for %s: the download-info step failed (HTTP %.0f: %s). The listing was claimed, but Fab answered no usable address for this file."),
			*FileName, Status, *Words);
	}
	return FString::Printf(TEXT("Fab gave no download for %s and did not say which step refused."), *FileName);
}
} // namespace

FMcpFabAddResult ParseAddReply(bool bSuccess, const FString& Payload)
{
	FMcpFabAddResult Result;
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Payload);
	const bool bJson = FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid();
	if (bJson) { Root->TryGetStringField(TEXT("error"), Result.ErrorCode); }
	if (bSuccess && bJson)
	{
		Root->TryGetBoolField(TEXT("accepted"), Result.bAccepted);
		Root->TryGetBoolField(TEXT("engineExactMatch"), Result.bEngineExactMatch);
		Root->TryGetStringField(TEXT("versionName"), Result.VersionName);
		Root->TryGetStringField(TEXT("formatCode"), Result.FormatCode);
		Root->TryGetStringField(TEXT("quality"), Result.Quality);
		Root->TryGetBoolField(TEXT("combinesMeshes"), Result.bMergesMeshes);
		Root->TryGetStringField(TEXT("title"), Result.Title);
		double Bytes = 0.0;
		if (Root->TryGetNumberField(TEXT("downloadBytes"), Bytes) && Bytes >= 0.0)
		{
			Result.DownloadBytes = static_cast<int64>(Bytes);
		}
		// A pack's version name is not a file; only a source format downloads one by that name.
		if (Result.FormatCode != TEXT("unreal-engine")) { Result.FileName = Result.VersionName; }
	}
	if (Result.bAccepted)
	{
		return Result;
	}
	// Safe to log in full: the page reports codes, statuses and key shapes, never values --
	// shape() emits key names and types, and the download url stays a local in the script.
	// Without this the self-diagnosing fields the script already computes never reach
	// anywhere a human can read them.
	UE_LOG(LogMcpFabAddReply, Warning, TEXT("Fab refused the add; page reported: %s"), *Payload);
	if (Result.ErrorCode.IsEmpty()) { Result.ErrorCode = TEXT("FAB_REJECTED"); }

	// The page already reports the formats it saw; naming them turns
	// NO_IMPORTABLE_FORMAT from "it failed somewhere" into "this
	// listing ships these formats and Fab imports none of them",
	// which is the difference between a dead end and a next step.
	TArray<FString> Formats;
	const TArray<TSharedPtr<FJsonValue>>* FormatRows = nullptr;
	if (bJson && Root->TryGetArrayField(TEXT("formatCodes"), FormatRows) && FormatRows != nullptr)
	{
		for (const TSharedPtr<FJsonValue>& Row : *FormatRows)
		{
			FString Code;
			if (Row.IsValid() && Row->TryGetString(Code) && !Code.IsEmpty())
			{
				Formats.Add(Code);
			}
		}
	}
	// The message follows the code. Claiming "none importable" for a
	// NO_VERSION failure described the wrong step and sent the reader
	// looking at the format list, which was fine.
	const FString FormatList = Formats.Num() > 0
		? FString::Printf(TEXT(" It advertises: %s."), *FString::Join(Formats, TEXT(", ")))
		: FString();
	FString FileName;
	if (bJson) { Root->TryGetStringField(TEXT("versionName"), FileName); }
	if (Result.ErrorCode == TEXT("NO_IMPORTABLE_FORMAT"))
	{
		Result.Error = FString::Printf(
			TEXT("Fab did not accept the listing.%s Fab imports unreal-engine, gltf, glb and fbx; this listing ships none of them."),
			*FormatList);
	}
	else if (Result.ErrorCode == TEXT("LARGE_SCENE_FILE"))
	{
		Result.Error = FString::Printf(
			TEXT("This listing ships one %s file of %.0f MB. Fab's importer merges every mesh in a file into a single static mesh, so a scene that size becomes one unplaceable mesh that can need gigabytes to build and holds the editor for many minutes. Nothing was downloaded. Pass combineMeshes=false to import each mesh separately, or combineMeshes=true to accept the single merged mesh."),
			*Result.FormatCode, static_cast<double>(Result.DownloadBytes) / 1.0e6);
	}
	else if (Result.ErrorCode == TEXT("NO_VERSION"))
	{
		Result.Error = FString::Printf(
			TEXT("The file lookup step found no downloadable file in the format it selected.%s See versionShape in the log for what the asset-formats response actually contained."),
			*FormatList);
	}
	else if (Result.ErrorCode == TEXT("NO_DOWNLOAD_URL"))
	{
		Result.Error = DescribeNoDownload(Root, FileName.IsEmpty() ? FString(TEXT("this listing")) : FileName) + FormatList;
	}
	else
	{
		Result.Error = FString::Printf(
			TEXT("Fab did not accept the listing (%s).%s"),
			*Result.ErrorCode, *FormatList);
	}
	// The dispatcher words its own failures (page not ready, page busy, timed out).
	FString PageMessage;
	if (bJson && Root->TryGetStringField(TEXT("message"), PageMessage) && !PageMessage.IsEmpty())
	{
		Result.Error = PageMessage;
	}
	return Result;
}

FString DescribeQueueHead(const FMcpFabImportStatus& Head, int32 QueueLimit)
{
	const FString Name = Head.Result.Title.IsEmpty()
		? Head.ListingId
		: FString::Printf(TEXT("%s (%s)"), *Head.Result.Title, *Head.ListingId);
	FString Progress;
	if (Head.DownloadedBytes >= 0)
	{
		Progress = Head.Result.DownloadBytes > 0
			? FString::Printf(TEXT(", %.0f of %.0f MB downloaded"), Head.DownloadedBytes / 1.0e6, Head.Result.DownloadBytes / 1.0e6)
			: FString::Printf(TEXT(", %.0f MB downloaded"), Head.DownloadedBytes / 1.0e6);
	}
	return FString::Printf(
		TEXT("Fab's add queue is full (%d waiting). It is working on %s (operation %s, %s%s, %.0f s in). Poll asset.query_marketplace with lookup=fab_import_status: it lists the queue, and an add is accepted again once fewer than %d wait."),
		QueueLimit, *Name, *Head.OperationId, *Head.Phase, *Progress, Head.ElapsedSeconds, QueueLimit);
}
} // namespace McpFabAddOperation
