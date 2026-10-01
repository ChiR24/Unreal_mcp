// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"

/**
 * The folder and naming rules of McpFabRelocate, as plain string functions.
 *
 * Which folders belong to an import and which Fab shares between all of them, what a folder is called
 * once nothing is left in it, and how an asset's name follows the one the caller chose. Nothing here
 * touches an asset, so the rules can be read, and pinned, apart from the move that applies them.
 */
namespace McpFabRelocateRules
{
inline const TCHAR* FabFolderPrefix()
{
	return TEXT("/Game/Fab/");
}

/** The kinds of asset an import's name can be taken from: its mesh, or for a surface its material instance. */
enum class ERole : uint8 { Other, StaticMesh, SkeletalMesh, MaterialInstance };

/** The first folder under /Game/Fab/ that holds Folder, or empty when Folder is not under there. */
inline FString FabChild(const FString& Folder)
{
	if (!Folder.StartsWith(FabFolderPrefix()))
	{
		return FString();
	}
	const FString Rest = Folder.Mid(FCString::Strlen(FabFolderPrefix()));
	int32 Slash = INDEX_NONE;
	return Rest.FindChar(TEXT('/'), Slash) ? Rest.Left(Slash) : Rest;
}

/** Fab installs these once, for every Megascans import: its master materials, material functions and textures. */
inline bool IsSharedFolder(const FString& Folder)
{
	const FString Child = FabChild(Folder);
	return Child == TEXT("Materials") || Child == TEXT("MaterialFunctions") || Child == TEXT("Textures");
}

/** A folder only one import used: /Game/Fab/<name>, or /Game/Fab/Megascans/<type>/<name> and below. */
inline bool IsListingFolder(const FString& Folder)
{
	TArray<FString> Parts;
	Folder.ParseIntoArray(Parts, TEXT("/"), true);
	const bool bMegascans = Parts.Num() > 2 && Parts[1] == TEXT("Fab") && Parts[2] == TEXT("Megascans");
	return Folder.StartsWith(FabFolderPrefix()) && Parts.Num() > (bMegascans ? 4 : 2);
}

/** The deepest folder every one of Folders sits in, as a /Game path. Folders must not be empty. */
inline FString CommonFolder(const TArray<FString>& Folders)
{
	TArray<FString> Common;
	Folders[0].ParseIntoArray(Common, TEXT("/"), true);
	for (const FString& Folder : Folders)
	{
		TArray<FString> Parts;
		Folder.ParseIntoArray(Parts, TEXT("/"), true);
		int32 Same = 0;
		while (Same < Common.Num() && Same < Parts.Num() && Common[Same] == Parts[Same])
		{
			++Same;
		}
		Common.SetNum(Same);
	}
	return TEXT("/") + FString::Join(Common, TEXT("/"));
}

/** The anchor's name without its tier or type prefix: the token its materials and textures are named after. */
inline FString StemOf(const FString& AnchorName)
{
	const int32 Tier = AnchorName.Find(TEXT("_tier_"), ESearchCase::IgnoreCase);
	if (Tier != INDEX_NONE)
	{
		return AnchorName.Left(Tier);
	}
	const bool bPrefixed = AnchorName.StartsWith(TEXT("SM_")) || AnchorName.StartsWith(TEXT("SK_")) || AnchorName.StartsWith(TEXT("MI_"));
	return bPrefixed ? AnchorName.Mid(3) : AnchorName;
}

/** The prefix the anchor takes with its new name. */
inline const TCHAR* PrefixFor(ERole Role)
{
	return Role == ERole::SkeletalMesh ? TEXT("SK_") : Role == ERole::MaterialInstance ? TEXT("MI_") : TEXT("SM_");
}

/** MI_ubitfhtfa and T_ubitfhtfa_4K_ORD follow the anchor to MI_<name> and T_<name>_4K_ORD; any other name stays. */
inline FString FollowName(const FString& Name, const FString& Stem, const FString& AssetName)
{
	int32 Underscore = INDEX_NONE;
	if (Stem.IsEmpty() || !Name.FindChar(TEXT('_'), Underscore))
	{
		return Name;
	}
	const FString Rest = Name.Mid(Underscore + 1);
	if (Rest != Stem && !Rest.StartsWith(Stem + TEXT("_")))
	{
		return Name;
	}
	return Name.Left(Underscore + 1) + AssetName + Rest.Mid(Stem.Len());
}
} // namespace McpFabRelocateRules
