// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "Domains/AssetWorkflow/Fab/McpAutomationBridge_FabPostImport.h"
#include "Domains/AssetWorkflow/Fab/McpAutomationBridge_FabRelocate.h"
#include "Safety/McpSafeOperations.h"

#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace McpFabPostImport
{
namespace
{
// Fab's importers leave what they create in memory. Five Megascans surface adds left 24 dirty texture
// and material-instance packages that existed only until the editor closed, and one import was lost
// outright when the editor had to be shut. A unreal-engine pack is copied from disk, never loaded, and
// so is not dirty: it is left alone rather than loaded just to be written back out.
void SaveImported(FMcpFabAddResult& Result, const TArray<FString>& ImportedPaths)
{
	TSet<FString> Seen;
	for (const FString& ObjectPath : ImportedPaths)
	{
		const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
		if (Seen.Contains(PackageName))
		{
			continue;
		}
		Seen.Add(PackageName);
		UPackage* Package = FindPackage(nullptr, *PackageName);
		if (Package == nullptr || !Package->IsDirty())
		{
			continue;
		}
		if (McpSafeOperations::McpSafeAssetSave(Package))
		{
			++Result.SavedCount;
		}
		else
		{
			Result.UnsavedPackages.Add(PackageName);
		}
	}
	Result.bSaveRan = true;
}
} // namespace

void Run(FMcpFabAddResult& Result, const TArray<FString>& ImportedPaths, const FString& DestinationFolder,
	const FString& AssetName)
{
	// Moved first, so what is saved is what is left at the new paths and not the redirectors behind it.
	TArray<FString> Paths = ImportedPaths;
	if (!DestinationFolder.IsEmpty() || !AssetName.IsEmpty())
	{
		McpFabRelocate::Apply(DestinationFolder, AssetName, Result, Paths);
	}
	SaveImported(Result, Paths);
}
} // namespace McpFabPostImport
