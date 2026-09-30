// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"
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
	EState State = EState::Resolving;
	double StartedAt = 0.0;
	double FinishedAt = 0.0;
	int32 AssetsSoFar = 0;
	// The accept-time facts, replaced by the outcome once the import ends.
	FMcpFabAddResult Result;
	TArray<FString> FabErrors;
	// Starts a queued add when its turn comes.
	TFunction<void()> Launch;
};

// Every operation of this session, oldest first. Game thread only.
TArray<FOperation>& Operations();

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

// Reads an operation into the public status shape. CacheLocation is Fab's download cache: for a source
// format it is the one view of a download in flight.
void Describe(const FOperation& Op, const FString& CacheLocation, FMcpFabImportStatus& Out);
} // namespace McpFabImportOperations
