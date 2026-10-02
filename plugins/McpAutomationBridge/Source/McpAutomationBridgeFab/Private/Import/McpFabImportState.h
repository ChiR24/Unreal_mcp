// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"
#include "McpFabDownloadProgress.h"
#include "McpFabProvider.h"

// The operation store's own data, shared by the two files that make it up (McpFabImportOperations.cpp owns
// the lifecycle, McpFabImportDescribe.cpp reads a record into the public status). Nothing outside
// Private/Import includes this: the rest of the module sees only McpFabImportOperations.h.
namespace McpFabImportOperations
{
enum class EState : uint8 { Queued, Resolving, Active, Done, Failed };

struct FOperation
{
	FString Id;
	FString ListingId;
	// Quality, combineMeshes and the core's RequestKey of the add that made it: a repeat add is folded in only on a match.
	FString OptionsKey;
	EState State = EState::Resolving;
	double StartedAt = 0.0;
	double FinishedAt = 0.0;
	int32 AssetsSoFar = 0;
	// The accept-time facts, replaced by the outcome once the import ends.
	FMcpFabAddResult Result;
	TArray<FString> FabErrors;
	// Starts a queued add when its turn comes.
	TFunction<void()> Launch;
	// Fab or Interchange was told to stop; the watcher ends the operation on its next tick.
	bool bCancelRequested = false;
};

// Every operation of this session, oldest first. Game thread only.
TArray<FOperation>& Operations();

FOperation* FindById(const FString& Id);

inline bool IsRunning(const FOperation& Op)
{
	return Op.State == EState::Resolving || Op.State == EState::Active;
}

inline bool IsOpen(const FOperation& Op)
{
	return Op.State == EState::Queued || IsRunning(Op);
}

// 1-based place among the queued operations, oldest first; 0 when the operation is not queued.
int32 PositionOf(const FOperation& Target);

// What can stop an operation right now, given the toast Fab shows for its download (McpFabImportCancel.cpp).
enum class ECancelRoute : uint8
{
	None,
	DropQueued,
	PressFabCancel,
	StopInterchange
};
ECancelRoute RouteFor(const FOperation& Op, const McpFabDownloadProgress::FNotification& Toast);

// The name Fab titles an operation's download notification with ("Downloading <name>"): the AssetName the
// add handed Fab, which is the version or file name, else the listing id.
FString ToastName(const FOperation& Op);

// Reads an operation into the public status shape. CacheLocation is Fab's download cache: for a source
// format it is the one view of a download in flight.
void Describe(const FOperation& Op, const FString& CacheLocation, FMcpFabImportStatus& Out);
} // namespace McpFabImportOperations
