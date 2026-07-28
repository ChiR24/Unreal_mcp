#include "Foundation/McpLiveStateRevisionTracker.h"

#include "Foundation/McpLiveStateRevisions.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Editor.h"
#include "Modules/ModuleManager.h"
#include "Selection.h"
#include "UObject/Package.h"

namespace
{
struct FMcpLiveStateDelegateHandles
{
	bool bStarted = false;
	FDelegateHandle Selection;
	FDelegateHandle MapChange;
	FDelegateHandle NewCurrentLevel;
	FDelegateHandle AssetAdded;
	FDelegateHandle AssetRemoved;
	FDelegateHandle PackageDirty;
};

FMcpLiveStateDelegateHandles& Handles()
{
	static FMcpLiveStateDelegateHandles State;
	return State;
}

void Advance(EMcpStateKind Kind)
{
	FMcpLiveStateRevisions::Get().Advance(Kind);
}
}
#endif

void McpStartLiveStateTracking()
{
#if WITH_EDITOR
	FMcpLiveStateDelegateHandles& State = Handles();
	if (State.bStarted)
	{
		return;
	}
	State.bStarted = true;

	State.Selection = USelection::SelectionChangedEvent.AddLambda(
		[](UObject*) { Advance(EMcpStateKind::Selection); });
	State.MapChange = FEditorDelegates::MapChange.AddLambda(
		[](uint32) { Advance(EMcpStateKind::Level); });
	State.NewCurrentLevel = FEditorDelegates::NewCurrentLevel.AddLambda(
		[]() { Advance(EMcpStateKind::Level); });
	State.PackageDirty = UPackage::PackageMarkedDirtyEvent.AddLambda(
		[](UPackage*, bool) { Advance(EMcpStateKind::Package); });

	IAssetRegistry& Registry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	State.AssetAdded = Registry.OnAssetAdded().AddLambda(
		[](const FAssetData&) { Advance(EMcpStateKind::AssetRegistry); });
	State.AssetRemoved = Registry.OnAssetRemoved().AddLambda(
		[](const FAssetData&) { Advance(EMcpStateKind::AssetRegistry); });
#endif
}

void McpStopLiveStateTracking()
{
#if WITH_EDITOR
	FMcpLiveStateDelegateHandles& State = Handles();
	if (!State.bStarted)
	{
		return;
	}
	State.bStarted = false;

	USelection::SelectionChangedEvent.Remove(State.Selection);
	FEditorDelegates::MapChange.Remove(State.MapChange);
	FEditorDelegates::NewCurrentLevel.Remove(State.NewCurrentLevel);
	UPackage::PackageMarkedDirtyEvent.Remove(State.PackageDirty);

	// Only touch the asset registry if its module is still loaded: during editor
	// shutdown it can already be gone, and LoadModuleChecked would resurrect or
	// assert instead of quietly unbinding.
	if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")))
	{
		IAssetRegistry& Registry =
			FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Registry.OnAssetAdded().Remove(State.AssetAdded);
		Registry.OnAssetRemoved().Remove(State.AssetRemoved);
	}

	State = FMcpLiveStateDelegateHandles();
#endif
}
