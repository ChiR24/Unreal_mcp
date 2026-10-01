// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "Domains/AssetWorkflow/Fab/McpAutomationBridge_FabRelocate.h"
#include "Domains/AssetWorkflow/Fab/McpAutomationBridge_FabRelocateRules.h"
#include "Domains/AssetWorkflow/Rename/McpAutomationBridge_AssetRenameGuard.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"
#include "Safety/McpSafeOperations.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstance.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectGlobals.h"

namespace McpFabRelocate
{
using namespace McpFabRelocateRules;

namespace
{
/** One asset the import created, with where it is and where it is going. */
struct FMove
{
	UObject* Object = nullptr;
	FString OldFolder;
	FString OldName;
	FString NewFolder;
	FString NewName;
	ERole Role = ERole::Other;

	FString OldPackage() const { return OldFolder / OldName; }
	FString NewPackage() const { return NewFolder / NewName; }
	bool IsMesh() const { return Role == ERole::StaticMesh || Role == ERole::SkeletalMesh; }
};

/** The assets of the import that sit in its own folders, loaded. Everything else is left alone. */
void Gather(const TArray<FString>& ImportedPaths, TArray<FMove>& OutMoves)
{
	for (const FString& ObjectPath : ImportedPaths)
	{
		const FString Package = FPackageName::ObjectPathToPackageName(ObjectPath);
		const FString Folder = FPackageName::GetLongPackagePath(Package);
		if (!Folder.StartsWith(FabFolderPrefix()) || IsSharedFolder(Folder))
		{
			continue;
		}
		UObject* Object = LoadObject<UObject>(nullptr, *ObjectPath);
		if (Object == nullptr)
		{
			continue;
		}
		FMove& Move = OutMoves.AddDefaulted_GetRef();
		Move.Object = Object;
		Move.OldFolder = Folder;
		Move.OldName = FPackageName::GetShortName(Package);
		Move.NewFolder = Move.OldFolder;
		Move.NewName = Move.OldName;
		Move.Role = Cast<UStaticMesh>(Object) != nullptr ? ERole::StaticMesh
			: Cast<USkeletalMesh>(Object) != nullptr ? ERole::SkeletalMesh
			: Cast<UMaterialInstance>(Object) != nullptr ? ERole::MaterialInstance : ERole::Other;
	}
}

/** The asset the name belongs to: the import's one mesh, or for a surface its one material instance. */
const FMove* FindAnchor(const TArray<FMove>& Moves, int32& OutMeshes, int32& OutInstances)
{
	OutMeshes = 0;
	OutInstances = 0;
	for (const FMove& Move : Moves)
	{
		OutMeshes += Move.IsMesh() ? 1 : 0;
		OutInstances += Move.Role == ERole::MaterialInstance ? 1 : 0;
	}
	for (const FMove& Move : Moves)
	{
		if ((OutMeshes == 1 && Move.IsMesh()) || (OutMeshes == 0 && OutInstances == 1 && Move.Role == ERole::MaterialInstance))
		{
			return &Move;
		}
	}
	return nullptr;
}

/** Says where each asset goes. False, with the reason in Note, when the plan cannot be carried out. */
bool Plan(const FString& Destination, const FString& AssetName, const FString& OldRoot, TArray<FMove>& Moves,
	FString& Note)
{
	int32 Meshes = 0;
	int32 Instances = 0;
	const FMove* Anchor = FindAnchor(Moves, Meshes, Instances);
	if (!AssetName.IsEmpty() && Anchor == nullptr)
	{
		Note += FString::Printf(
			TEXT("assetName was not applied: the import holds %d meshes and %d material instances, so no one asset is the one to name. "),
			Meshes, Instances);
	}
	const FString Stem = Anchor != nullptr ? StemOf(Anchor->OldName) : FString();
	for (FMove& Move : Moves)
	{
		if (!Destination.IsEmpty())
		{
			Move.NewFolder = Destination + Move.OldFolder.Mid(OldRoot.Len());
		}
		if (Anchor != nullptr && !AssetName.IsEmpty())
		{
			Move.NewName = &Move == Anchor ? FString(PrefixFor(Move.Role)) + AssetName : FollowName(Move.OldName, Stem, AssetName);
		}
	}

	// All or nothing: a target that is taken, or two assets that would share one, stops the whole move.
	TSet<FString> Leaving;
	TSet<FString> Arriving;
	for (const FMove& Move : Moves)
	{
		Leaving.Add(Move.OldPackage());
	}
	for (const FMove& Move : Moves)
	{
		const FString Target = Move.NewPackage();
		if (Target != Move.OldPackage() && ((!Leaving.Contains(Target) && McpAssetExists(Target)) || Arriving.Contains(Target)))
		{
			Note += FString::Printf(TEXT("Nothing was moved: %s already exists or would be taken twice. Pass another destinationPath or assetName. "), *Target);
			return false;
		}
		Arriving.Add(Target);
	}
	return true;
}

/** The folders the moved assets left empty, from the deepest up; the folders every import shares stay. */
void PruneEmptyFolders(const FString& Folder)
{
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	for (FString Current = Folder; IsListingFolder(Current); Current = FPaths::GetPath(Current))
	{
		TArray<FAssetData> Left;
		Registry.GetAssetsByPath(FName(*Current), Left, /*bRecursive=*/true);
		if (Left.Num() > 0 || !McpSafeOperations::McpSafeDeleteFolder(Current))
		{
			return;
		}
	}
}

FString ObjectPathOf(const FString& Folder, const FString& Name)
{
	return Folder / Name + TEXT(".") + Name;
}
} // namespace

bool IsValidAssetName(const FString& Name)
{
	if (Name.IsEmpty() || Name.Len() > 64 || FChar::IsDigit(Name[0]))
	{
		return false;
	}
	for (const TCHAR Ch : Name)
	{
		const bool bAllowed = (Ch >= TEXT('a') && Ch <= TEXT('z')) || (Ch >= TEXT('A') && Ch <= TEXT('Z')) ||
			(Ch >= TEXT('0') && Ch <= TEXT('9')) || Ch == TEXT('_');
		if (!bAllowed)
		{
			return false;
		}
	}
	return true;
}

void Apply(const FString& Destination, const FString& AssetName, FMcpFabAddResult& Result, TArray<FString>& ImportedPaths)
{
	Result.bRelocateRan = true;
	TArray<FMove> Moves;
	Gather(ImportedPaths, Moves);
	if (Moves.Num() == 0)
	{
		Result.RelocationNote = FString::Printf(
			TEXT("Nothing was moved: this import created nothing in a folder of its own under /Game/Fab; it landed under %s, which is left where Fab put it. asset.move relocates a folder."),
			*Result.RootPath);
		return;
	}

	TArray<FString> Folders;
	for (const FMove& Move : Moves)
	{
		Folders.Add(Move.OldFolder);
	}
	const FString OldRoot = CommonFolder(Folders);
	FString Note;
	if (!Plan(Destination, AssetName, OldRoot, Moves, Note))
	{
		Result.RelocationNote = Note.TrimEnd();
		return;
	}

	TArray<FAssetRenameData> Renames;
	for (const FMove& Move : Moves)
	{
		if (Move.NewPackage() != Move.OldPackage())
		{
			Renames.Emplace(Move.Object, Move.NewFolder, Move.NewName);
		}
	}
	if (Renames.Num() == 0)
	{
		Result.RelocationNote = (Note + TEXT("Nothing needed to move: every asset already has the folder and name asked for.")).TrimEnd();
		return;
	}

	// The same rename asset.move runs: settings that point at an asset follow it, and a failure puts them back.
	FString Failure;
	if (!McpAssetRename::RenameWithSettingsFollow(Renames, MakeShared<FJsonObject>(), Failure))
	{
		Result.RelocationNote = (Note + (Failure.IsEmpty() ? FString(TEXT("The rename failed, so nothing was moved.")) : Failure)).TrimEnd();
		return;
	}

	TMap<FString, FString> NowAt;
	for (const FMove& Move : Moves)
	{
		NowAt.Add(ObjectPathOf(Move.OldFolder, Move.OldName), ObjectPathOf(Move.NewFolder, Move.NewName));
	}
	for (TArray<FString>* Paths : {&ImportedPaths, &Result.SamplePaths})
	{
		for (FString& Path : *Paths)
		{
			if (const FString* Moved = NowAt.Find(Path))
			{
				Path = *Moved;
			}
		}
	}
	if (!Destination.IsEmpty())
	{
		Result.RootPath = Destination;
	}
	Result.MovedCount = Renames.Num();
	Result.RelocationNote = Note.TrimEnd();

	// What the move left behind: redirectors at the old paths, then the folders that held them.
	int32 Found = 0;
	int32 Fixed = 0;
	McpAssetRename::FixupRedirectorsIn(OldRoot, Found, Fixed);
	PruneEmptyFolders(OldRoot);
}
} // namespace McpFabRelocate
