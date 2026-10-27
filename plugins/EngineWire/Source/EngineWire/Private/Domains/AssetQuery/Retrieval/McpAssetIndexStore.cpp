// Copyright (c) 2024 EngineWire Contributors
//
// The retrieval index and captions on disk (McpAssetIndex.h): two JSON files under the project's Saved/EngineWire. A file
// of another version, or one that does not parse, reads as empty: the index rebuilds itself, and a caption file is set
// aside as AssetCaptions.unread.json before captions start over.

#include "Domains/AssetQuery/Retrieval/McpAssetIndex.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace McpAssetRetrieval
{
namespace
{
// Each file has its own version. The index is a cache: raise IndexVersion whenever LookOf or WordsOf change, and an
// index of another version is read again. Captions are the agents' work: CaptionVersion moves only with their file's format.
constexpr int32 IndexVersion = 2;
constexpr int32 CaptionVersion = 1;

FString StorePath(const TCHAR* Name)
{
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("EngineWire"), Name);
}

TSharedPtr<FJsonObject> StoreRead(const TCHAR* Name, int32 Expected)
{
    FString Text;
    TSharedPtr<FJsonObject> Root;
    double Version = 0.0;
    if (!FFileHelper::LoadFileToString(Text, *StorePath(Name)) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) ||
        !Root.IsValid() || !Root->TryGetNumberField(TEXT("version"), Version) || static_cast<int32>(Version) != Expected)
    {
        return nullptr;
    }
    return Root;
}

bool StoreWrite(const TCHAR* Name, const TSharedRef<FJsonObject>& Root, int32 Version)
{
    Root->SetNumberField(TEXT("version"), Version);
    FString Text;
    FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Text));
    return FFileHelper::SaveStringToFile(Text, *StorePath(Name));
}

TArray<TSharedPtr<FJsonValue>> StoreNumbers(const TArray<float>& Values)
{
    TArray<TSharedPtr<FJsonValue>> Out;
    for (const float Value : Values)
    {
        Out.Add(MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Value * 1e4) / 1e4));
    }
    return Out;
}

TArray<float> StoreFloats(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
{
    TArray<float> Out;
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (Object->TryGetArrayField(Field, Values))
    {
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            Out.Add(static_cast<float>(Value->AsNumber()));
        }
    }
    return Out;
}

TArray<FString> StoreStrings(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
{
    TArray<FString> Out;
    Object->TryGetStringArrayField(Field, Out);
    return Out;
}

FIndex StoreLoad()
{
    FIndex Index;
    const TSharedPtr<FJsonObject> Assets = StoreRead(TEXT("AssetIndex.json"), IndexVersion);
    const TSharedPtr<FJsonObject>* Rows = nullptr;
    // FString(Key): 5.8 keys FJsonObject::Values by a type of its own, which only an FString constructed from it reads.
    for (const auto& Pair : Assets.IsValid() && Assets->TryGetObjectField(TEXT("assets"), Rows) ? (*Rows)->Values : decltype((*Rows)->Values)())
    {
        const TSharedPtr<FJsonObject> Row = Pair.Value->AsObject();
        FIndexed& Entry = Index.Assets.Add(FString(Pair.Key));
        Entry.Class = Row->GetStringField(TEXT("class"));
        LexFromString(Entry.Stamp, *Row->GetStringField(TEXT("stamp")));
        Entry.Words = StoreStrings(Row, TEXT("words"));
        const TArray<float> Size = StoreFloats(Row, TEXT("size"));
        Entry.Size = Size.Num() == 3 ? FVector(Size[0], Size[1], Size[2]) : FVector::ZeroVector;
        Entry.Triangles = static_cast<int32>(Row->GetNumberField(TEXT("triangles")));
        const TArray<float> Palette = StoreFloats(Row, TEXT("palette"));
        for (int32 At = 0; At + 3 < Palette.Num(); At += 4)
        {
            Entry.Look.Palette.Add(FVector4f(Palette[At], Palette[At + 1], Palette[At + 2], Palette[At + 3]));
        }
        Entry.Look.Edges = StoreFloats(Row, TEXT("edges"));
        Entry.Look.Hu = StoreFloats(Row, TEXT("hu"));
        Entry.Look.Drawn = static_cast<float>(Row->GetNumberField(TEXT("drawn")));
        Entry.bHasLook = Entry.Look.Palette.Num() > 0;
    }
    const TSharedPtr<FJsonObject> Captions = StoreRead(TEXT("AssetCaptions.json"), CaptionVersion);
    if (!Captions.IsValid() && IFileManager::Get().FileExists(*StorePath(TEXT("AssetCaptions.json"))))
    {
        // Unreadable (another version, a broken hand edit): set aside, so the next caption write does not overwrite it.
        IFileManager::Get().Move(*StorePath(TEXT("AssetCaptions.unread.json")), *StorePath(TEXT("AssetCaptions.json")));
    }
    const TSharedPtr<FJsonObject>* Texts = nullptr;
    for (const auto& Pair : Captions.IsValid() && Captions->TryGetObjectField(TEXT("captions"), Texts) ? (*Texts)->Values : decltype((*Texts)->Values)())
    {
        const TSharedPtr<FJsonObject> Row = Pair.Value->AsObject();
        Index.Captions.Add(FString(Pair.Key), {Row->GetStringField(TEXT("caption")), StoreStrings(Row, TEXT("tags"))});
    }
    return Index;
}
} // namespace

FIndex& TheIndex()
{
    static FIndex Index = StoreLoad();
    return Index;
}

void SaveIndex(FIndex& Index)
{
    const TSharedRef<FJsonObject> Rows = MakeShared<FJsonObject>();
    for (const TPair<FString, FIndexed>& Pair : Index.Assets)
    {
        const FIndexed& Entry = Pair.Value;
        const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("class"), Entry.Class);
        Row->SetStringField(TEXT("stamp"), LexToString(Entry.Stamp));
        TArray<TSharedPtr<FJsonValue>> Words;
        for (const FString& Word : Entry.Words)
        {
            Words.Add(MakeShared<FJsonValueString>(Word));
        }
        Row->SetArrayField(TEXT("words"), Words);
        Row->SetArrayField(TEXT("size"), StoreNumbers({static_cast<float>(Entry.Size.X), static_cast<float>(Entry.Size.Y), static_cast<float>(Entry.Size.Z)}));
        Row->SetNumberField(TEXT("triangles"), Entry.Triangles);
        TArray<float> Palette;
        for (const FVector4f& Colour : Entry.Look.Palette)
        {
            Palette.Append({Colour.X, Colour.Y, Colour.Z, Colour.W});
        }
        Row->SetArrayField(TEXT("palette"), StoreNumbers(Palette));
        Row->SetArrayField(TEXT("edges"), StoreNumbers(Entry.Look.Edges));
        Row->SetArrayField(TEXT("hu"), StoreNumbers(Entry.Look.Hu));
        Row->SetNumberField(TEXT("drawn"), Entry.Look.Drawn);
        Rows->SetObjectField(Pair.Key, Row);
    }
    const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetObjectField(TEXT("assets"), Rows);
    Index.bDirty = !StoreWrite(TEXT("AssetIndex.json"), Root, IndexVersion); // a cache: a failed write is tried again next call
}

bool SaveCaptions(const TMap<FString, FCaption>& Captions)
{
    const TSharedRef<FJsonObject> Rows = MakeShared<FJsonObject>();
    for (const TPair<FString, FCaption>& Pair : Captions)
    {
        const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("caption"), Pair.Value.Text);
        TArray<TSharedPtr<FJsonValue>> Tags;
        for (const FString& Tag : Pair.Value.Tags)
        {
            Tags.Add(MakeShared<FJsonValueString>(Tag));
        }
        Row->SetArrayField(TEXT("tags"), Tags);
        Rows->SetObjectField(Pair.Key, Row);
    }
    const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetObjectField(TEXT("captions"), Rows);
    return StoreWrite(TEXT("AssetCaptions.json"), Root, CaptionVersion);
}
} // namespace McpAssetRetrieval
