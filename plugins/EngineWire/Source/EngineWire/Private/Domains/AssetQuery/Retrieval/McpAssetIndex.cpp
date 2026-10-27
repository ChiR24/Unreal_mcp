// Copyright (c) 2024 EngineWire Contributors

#include "Domains/AssetQuery/Retrieval/McpAssetIndex.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/ObjectThumbnail.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"

namespace McpAssetRetrieval
{
namespace
{
// ApproxSize reads "XxYxZ" in cm (static and skeletal meshes keep it in the registry).
FVector IndexSizeOf(const FAssetData& Data)
{
    FString Text;
    TArray<FString> Parts;
    if (!Data.GetTagValue(FName(TEXT("ApproxSize")), Text) || Text.ParseIntoArray(Parts, TEXT("x")) != 3)
    {
        return FVector::ZeroVector;
    }
    return FVector(FCString::Atod(*Parts[0]), FCString::Atod(*Parts[1]), FCString::Atod(*Parts[2]));
}

int64 IndexStamp(const FString& File)
{
    return IFileManager::Get().GetTimeStamp(*File).GetTicks() ^ IFileManager::Get().FileSize(*File);
}

void IndexOne(FIndex& Index, const FAssetData& Data, int64 Stamp)
{
    const FString Path = Data.PackageName.ToString();
    FIndexed Fresh;
    Fresh.Class = ClassOf(Data);
    Fresh.Stamp = Stamp;
    Fresh.Words = WordsOf(Path.StartsWith(TEXT("/Game/")) ? Path.RightChop(6) : Path);
    Fresh.Size = IndexSizeOf(Data);
    FString Triangles;
    Fresh.Triangles = Data.GetTagValue(FName(TEXT("Triangles")), Triangles) ? FCString::Atoi(*Triangles) : 0;
    TArray<uint8> Bgra;
    int32 Width = 0, Height = 0;
    // ponytail: read once per file stamp, so a thumbnail made after the first read counts only once the package is saved;
    // re-read entries without a look whenever a call has time to spare if that ever matters.
    Fresh.bHasLook = ThumbnailOf(Data, Bgra, Width, Height) && LookOf(Bgra, Width, Height, Fresh.Look);
    Index.Assets.Add(Path, MoveTemp(Fresh));
    Index.bDirty = true;
}
} // namespace

FString ClassOf(const FAssetData& Data)
{
    const FString Path = MCP_ASSET_DATA_GET_CLASS_PATH(Data);
    int32 Dot = INDEX_NONE;
    return Path.FindLastChar(TEXT('.'), Dot) ? Path.Mid(Dot + 1) : Path;
}

TArray<FString> WordsOf(const FString& Text)
{
    TArray<FString> Out;
    FString Word;
    const auto Close = [&Out, &Word]()
    {
        if (Word.Len() > 1)
        {
            Out.Add(Word.ToLower());
        }
        Word.Reset();
    };
    for (int32 Index = 0; Index < Text.Len(); ++Index)
    {
        const TCHAR Char = Text[Index];
        const TCHAR Before = Index > 0 ? Text[Index - 1] : TEXT(' ');
        const bool bBreak = !FChar::IsAlnum(Char) || (FChar::IsUpper(Char) && FChar::IsLower(Before)) ||
                            (FChar::IsDigit(Char) != FChar::IsDigit(Before) && FChar::IsAlnum(Before));
        if (bBreak)
        {
            Close();
        }
        if (FChar::IsAlnum(Char))
        {
            Word.AppendChar(Char);
        }
    }
    Close();
    return Out;
}

TArray<FAssetData> AssetsUnder(const FString& Folder, const FString& Class)
{
    TArray<FAssetData> Assets;
    FAssetRegistryModule::GetRegistry().GetAssetsByPath(FName(*Folder), Assets, true);
    Assets.RemoveAll([&Class](const FAssetData& Data) { return Data.IsRedirector() || (!Class.IsEmpty() && !ClassOf(Data).Equals(Class, ESearchCase::IgnoreCase)); });
    Assets.Sort([](const FAssetData& L, const FAssetData& R) { return L.PackageName.ToString().Compare(R.PackageName.ToString(), ESearchCase::CaseSensitive) < 0; });
    return Assets;
}

int32 Refresh(FIndex& Index, const FString& Folder, const FString& Class, double Deadline, TArray<FString>& OutPaths)
{
    int32 Pending = 0;
    const TArray<FAssetData> Assets = AssetsUnder(Folder, Class);
    if (Class.IsEmpty()) // entries of assets gone from the folder are dropped (with a class asked, others are not seen)
    {
        TSet<FString> Present;
        for (const FAssetData& Data : Assets)
        {
            Present.Add(Data.PackageName.ToString());
        }
        const FString Prefix = Folder.EndsWith(TEXT("/")) ? Folder : Folder + TEXT("/");
        const int32 Before = Index.Assets.Num();
        for (auto It = Index.Assets.CreateIterator(); It; ++It)
        {
            if (It.Key().StartsWith(Prefix) && !Present.Contains(It.Key()))
            {
                It.RemoveCurrent();
            }
        }
        Index.bDirty |= Index.Assets.Num() != Before;
    }
    for (const FAssetData& Data : Assets)
    {
        const FString Path = Data.PackageName.ToString();
        FString File;
        if (!FPackageName::DoesPackageExist(Path, &File))
        {
            continue;
        }
        const int64 Stamp = IndexStamp(File);
        const FIndexed* Known = Index.Assets.Find(Path);
        if (!Known || Known->Stamp != Stamp)
        {
            if (FPlatformTime::Seconds() > Deadline)
            {
                ++Pending;
                if (!Known)
                {
                    continue; // nothing known yet: left for the next call
                }
            }
            else
            {
                IndexOne(Index, Data, Stamp);
            }
        }
        OutPaths.Add(Path);
    }
    return Pending;
}

bool Update(FIndex& Index, const FAssetData& Data)
{
    FString File;
    if (!FPackageName::DoesPackageExist(Data.PackageName.ToString(), &File))
    {
        return false;
    }
    const int64 Stamp = IndexStamp(File);
    const FIndexed* Known = Index.Assets.Find(Data.PackageName.ToString());
    if (!Known || Known->Stamp != Stamp)
    {
        IndexOne(Index, Data, Stamp);
    }
    return true;
}

bool ThumbnailOf(const FAssetData& Data, TArray<uint8>& OutBgra, int32& OutWidth, int32& OutHeight)
{
    // As the Content Browser looks (AssetHasCustomThumbnail): the loaded package's thumbnail first (one made this session,
    // or a package not saved yet, which the disk read refuses), then the one stored in the package file.
    const FString FullName = Data.GetFullName();
    const FObjectThumbnail* Thumbnail = ThumbnailTools::FindCachedThumbnail(FullName);
    FThumbnailMap Thumbnails;
    if (!Thumbnail || Thumbnail->IsEmpty())
    {
        ThumbnailTools::ConditionallyLoadThumbnailsForObjects({FName(*FullName)}, Thumbnails);
        Thumbnail = Thumbnails.Find(FName(*FullName));
    }
    if (!Thumbnail || Thumbnail->IsEmpty() || Thumbnail->GetImageWidth() <= 0 || Thumbnail->GetImageHeight() <= 0)
    {
        return false;
    }
    OutBgra = Thumbnail->GetUncompressedImageData();
    OutWidth = Thumbnail->GetImageWidth();
    OutHeight = Thumbnail->GetImageHeight();
    return OutBgra.Num() >= OutWidth * OutHeight * 4;
}
} // namespace McpAssetRetrieval
