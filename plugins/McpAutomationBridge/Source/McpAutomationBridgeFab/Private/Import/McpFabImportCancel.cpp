// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpFabImportOperations.h"
#include "McpFabImportState.h"
#include "McpFabInterchange.h"

// Stopping an import. Fab gives three handles and no more: a queued add is only a closure in this store,
// a unreal-engine pack's download notification carries a Cancel button, and Interchange has a cancel for
// the translation it runs. A source-format download (fbx, gltf, obj, usdz, Megascans) has no Cancel:
// Fab builds its notification with the button disabled, and the request is private to its workflow.
namespace McpFabImportOperations
{
FString ToastName(const FOperation& Op)
{
	return Op.Result.VersionName.IsEmpty() ? Op.ListingId : Op.Result.VersionName;
}

bool IsDownloadShowing(const FString& OperationId)
{
	const FOperation* Op = FindById(OperationId);
	return Op != nullptr && McpFabDownloadProgress::Read(ToastName(*Op)).bFound;
}

ECancelRoute RouteFor(const FOperation& Op, const McpFabDownloadProgress::FNotification& Toast)
{
	if (Op.State == EState::Queued)
	{
		return ECancelRoute::DropQueued;
	}
	if (Op.State != EState::Active)
	{
		return ECancelRoute::None;
	}
	// Once assets land the download is over and Fab's Cancel has nothing to stop.
	if (Op.AssetsSoFar == 0 && Toast.bCancellable)
	{
		return ECancelRoute::PressFabCancel;
	}
	return McpFabInterchange::IsActive() ? ECancelRoute::StopInterchange : ECancelRoute::None;
}

bool RequestCancel(const FString& OperationId, FString& OutMessage, FString& OutErrorCode)
{
	FOperation* Op = FindById(OperationId);
	if (Op == nullptr)
	{
		OutErrorCode = TEXT("NOT_FOUND");
		OutMessage = FString::Printf(
			TEXT("No Fab import has operation id '%s'. The editor keeps the last 16 imports of this session."), *OperationId);
		return false;
	}
	if (!IsOpen(*Op))
	{
		OutErrorCode = TEXT("ALREADY_FINISHED");
		OutMessage = FString::Printf(TEXT("Operation %s already %s, so there is nothing left to cancel."),
			*OperationId, Op->State == EState::Done ? TEXT("finished") : TEXT("failed"));
		return false;
	}
	if (Op->bCancelRequested)
	{
		OutMessage = TEXT("A cancel was already requested; the import ends as failed with CANCELLED within a second.");
		return true;
	}
	if (Op->State == EState::Resolving)
	{
		OutErrorCode = TEXT("NOT_CANCELLABLE");
		OutMessage = TEXT("Fab is still being asked for the download, which takes seconds and cannot be interrupted. Cancel again once the phase is downloading.");
		return false;
	}

	const FString Name = ToastName(*Op);
	const McpFabDownloadProgress::FNotification Toast = McpFabDownloadProgress::Read(Name);
	switch (RouteFor(*Op, Toast))
	{
	case ECancelRoute::DropQueued:
	{
		FMcpFabAddResult Outcome = Op->Result;
		Outcome.ErrorCode = TEXT("CANCELLED");
		Outcome.Error = TEXT("Cancelled while queued: it was never put to Fab, and nothing was downloaded.");
		Finish(OperationId, Outcome);
		OutMessage = TEXT("The queued add was dropped before it started; nothing was downloaded.");
		return true;
	}
	case ECancelRoute::PressFabCancel:
		if (McpFabDownloadProgress::PressCancel(Name))
		{
			Op->bCancelRequested = true;
			OutMessage = TEXT("Pressed Cancel on Fab's download notification: the pack download stops and the import ends as failed with CANCELLED within a second.");
			return true;
		}
		break;
	case ECancelRoute::StopInterchange:
		if (McpFabInterchange::CancelTasks())
		{
			Op->bCancelRequested = true;
			OutMessage = TEXT("Asked Interchange to cancel its import tasks: a translation stops, and the import ends as failed with CANCELLED with whatever had already landed listed. A mesh build already holding the editor finishes first.");
			return true;
		}
		break;
	case ECancelRoute::None:
		break;
	}
	OutErrorCode = TEXT("NOT_CANCELLABLE");
	OutMessage = Toast.bFound
		? TEXT("This download has no Cancel: Fab offers one only for a unreal-engine pack, and the download of a source format (fbx, gltf, obj, usdz, Megascans) cannot be stopped from outside Fab. It finishes, then imports; asset.delete removes what lands.")
		: TEXT("Fab is past the download and Interchange reports no import task to stop, so there is nothing to cancel; the import ends by itself.");
	return false;
}
} // namespace McpFabImportOperations
