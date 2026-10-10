// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"

/**
 * Which new assets belong to the Fab import being watched, and how they are summed up.
 *
 * The registry announces every asset created while an import runs, those other calls make in the meantime
 * included. A unreal-engine pack downloads for minutes before any of its files land, and an asset created
 * then settled the import as done, with that unrelated asset reported as what it imported. A pack lands in a
 * /Game folder of its own and a source format under /Game/Fab, so an asset in any other top-level folder the
 * project already had when the add began is not the import's.
 */
namespace McpFabImportScope
{
/** The top-level /Game folders the baseline already held (/Game for an asset at the root). */
TSet<FString> TopFolders(const TSet<FString>& Before);

/** True when a new asset can be the import's: under /Game/Fab, or in a top-level folder the baseline lacked. */
bool Counts(const FString& Path, const TSet<FString>& OldTops);

/** Longest shared /Game/<folder> prefix of everything that appeared. */
FString CommonRoot(const TArray<FString>& Paths);

/** Up to ten paths, the static and skeletal meshes first: they are what a caller came for. */
TArray<FString> PickSamples(TArray<FString> Meshes, TArray<FString> Others);
} // namespace McpFabImportScope
