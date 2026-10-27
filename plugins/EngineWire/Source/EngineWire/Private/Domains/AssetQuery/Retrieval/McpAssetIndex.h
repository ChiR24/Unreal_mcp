// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "Domains/AssetQuery/Retrieval/McpAssetLook.h"

// The project's asset index for retrieval (Saved/EngineWire/AssetIndex.json): for every asset under the folders asked
// about, its class, the words of its name and folders, its registry size and triangle count, and the look of the
// thumbnail stored in its package (read without loading the asset). Captions written by caption_assets live beside it
// (Saved/EngineWire/AssetCaptions.json). Both are plain files: nothing is written into the assets themselves.
namespace McpAssetRetrieval
{
struct FCaption
{
    FString Text;
    TArray<FString> Tags;
};

struct FIndexed
{
    FString Class;
    int64 Stamp = 0;             // the package file's time and size: a different stamp means look again
    TArray<FString> Words;       // lower case, from the asset's name and folders
    FVector Size = FVector::ZeroVector; // the registry's ApproxSize (meshes), zero when it has none
    int32 Triangles = 0;
    bool bHasLook = false;
    FLook Look;
};

struct FIndex
{
    TMap<FString, FIndexed> Assets;   // by package path
    TMap<FString, FCaption> Captions; // by package path
    bool bDirty = false;              // entries changed since SaveIndex
};

/** The index and captions, read from disk on first use and kept for the editor session. */
FIndex& TheIndex();
void SaveIndex(FIndex& Index);
/** Writes the captions given; false when the file could not be written. */
bool SaveCaptions(const TMap<FString, FCaption>& Captions);

/** The registry's assets under Folder (Class, when given, by class name), redirectors left out, in package path order. */
TArray<FAssetData> AssetsUnder(const FString& Folder, const FString& Class);

/** Brings the assets under Folder up to date until Deadline (FPlatformTime); OutPaths gets those with an entry, in path
 *  order; returns how many were left unread for want of time (another call goes on where this one stopped). */
int32 Refresh(FIndex& Index, const FString& Folder, const FString& Class, double Deadline, TArray<FString>& OutPaths);

/** Brings one asset's entry up to date; false when it has no package on disk. */
bool Update(FIndex& Index, const FAssetData& Data);

/** The thumbnail the editor has for an asset (cached in memory, else stored in its package), as BGRA; false for none. */
bool ThumbnailOf(const FAssetData& Data, TArray<uint8>& OutBgra, int32& OutWidth, int32& OutHeight);

/** The asset's class by its own name (StaticMesh, not /Script/Engine.StaticMesh). */
FString ClassOf(const FAssetData& Data);

/** The words of a name or a caption: lower case, camel case and underscores, digits and punctuation split. */
TArray<FString> WordsOf(const FString& Text);
} // namespace McpAssetRetrieval
