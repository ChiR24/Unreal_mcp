// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpFabImportScope.h"

namespace McpFabImportScope
{
namespace
{
/** /Game/<folder> for a path under /Game, /Game for an asset at its root, empty for anything outside it. */
FString TopFolder(const FString& Path)
{
	FString Remainder = Path;
	if (!Remainder.RemoveFromStart(TEXT("/Game/")))
	{
		return FString();
	}
	FString Folder;
	return Remainder.Split(TEXT("/"), &Folder, nullptr) ? TEXT("/Game/") + Folder : FString(TEXT("/Game"));
}
} // namespace

TSet<FString> TopFolders(const TSet<FString>& Before)
{
	TSet<FString> Tops;
	for (const FString& Path : Before)
	{
		const FString Top = TopFolder(Path);
		if (!Top.IsEmpty())
		{
			Tops.Add(Top);
		}
	}
	return Tops;
}

bool Counts(const FString& Path, const TSet<FString>& OldTops)
{
	const FString Top = TopFolder(Path);
	return Top == TEXT("/Game/Fab") || (!Top.IsEmpty() && !OldTops.Contains(Top));
}

FString CommonRoot(const TArray<FString>& Paths)
{
	FString Root;
	for (const FString& Path : Paths)
	{
		const FString Candidate = TopFolder(Path);
		if (Candidate.IsEmpty() || Candidate == TEXT("/Game"))
		{
			continue;
		}
		if (Root.IsEmpty()) { Root = Candidate; }
		else if (Root != Candidate) { return TEXT("/Game"); }
	}
	return Root.IsEmpty() ? TEXT("/Game") : Root;
}

TArray<FString> PickSamples(TArray<FString> Meshes, TArray<FString> Others)
{
	Meshes.Sort();
	Others.Sort();
	TArray<FString> Samples;
	for (const TArray<FString>* Group : {&Meshes, &Others})
	{
		for (const FString& Path : *Group)
		{
			if (Samples.Num() < 10)
			{
				Samples.Add(Path);
			}
		}
	}
	return Samples;
}
} // namespace McpFabImportScope
