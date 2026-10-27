// Copyright (c) 2024 EngineWire Contributors

#include "Domains/AssetQuery/Retrieval/McpAssetRank.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpAssetLookTest, "EngineWire.AssetQuery.Look",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpAssetRankTest, "EngineWire.AssetQuery.Rank",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
// A 64 x 64 BGRA tile with one shape drawn in a colour (a square or a disc), on plain grey or on the editor's checker.
TArray<uint8> RetrievalTile(bool bDisc, FColor Colour, bool bChecker = false)
{
    TArray<uint8> Out;
    for (int32 Y = 0; Y < 64; ++Y)
    {
        for (int32 X = 0; X < 64; ++X)
        {
            const bool bIn = bDisc ? FMath::Square(X - 31.5) + FMath::Square(Y - 31.5) < 20.0 * 20.0 : X >= 14 && X < 50 && Y >= 14 && Y < 50;
            const FColor Square = ((X / 8) + (Y / 8)) % 2 ? FColor(21, 21, 21) : FColor(12, 12, 12);
            const FColor Pixel = bIn ? Colour : bChecker ? Square : FColor(90, 90, 90);
            Out.Append({Pixel.B, Pixel.G, Pixel.R, 255});
        }
    }
    return Out;
}

McpAssetRetrieval::FIndexed RetrievalEntry(const FString& Path, const FVector& Size, bool bDisc, FColor Colour)
{
    McpAssetRetrieval::FIndexed Entry;
    Entry.Class = TEXT("StaticMesh");
    Entry.Words = McpAssetRetrieval::WordsOf(Path.RightChop(6));
    Entry.Size = Size;
    Entry.bHasLook = McpAssetRetrieval::LookOf(RetrievalTile(bDisc, Colour), 64, 64, Entry.Look);
    return Entry;
}
} // namespace

// Known answers: a red square looks more like a red disc than like a green square, more like another red square than
// like either; the backdrop is left out (a tile of backdrop only has no look); a red tile's drawn part is all near red.
bool FMcpAssetLookTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    McpAssetRetrieval::FLook RedSquare, RedDisc, GreenSquare, Again, Empty;
    TestTrue(TEXT("a red square"), McpAssetRetrieval::LookOf(RetrievalTile(false, FColor(200, 30, 30)), 64, 64, RedSquare));
    TestTrue(TEXT("a red disc"), McpAssetRetrieval::LookOf(RetrievalTile(true, FColor(200, 30, 30)), 64, 64, RedDisc));
    TestTrue(TEXT("a green square"), McpAssetRetrieval::LookOf(RetrievalTile(false, FColor(30, 180, 40)), 64, 64, GreenSquare));
    TestTrue(TEXT("the same square again"), McpAssetRetrieval::LookOf(RetrievalTile(false, FColor(200, 30, 30)), 64, 64, Again));
    TestEqual(TEXT("the same look"), McpAssetRetrieval::Unlike(RedSquare, Again), 0.0, 1e-9);
    TestTrue(TEXT("colour weighs more than shape"), McpAssetRetrieval::Unlike(RedSquare, RedDisc) < McpAssetRetrieval::Unlike(RedSquare, GreenSquare));
    TestTrue(TEXT("shape still tells a square from a disc"), McpAssetRetrieval::Unlike(RedSquare, RedDisc) > 0.0);
    TestEqual(TEXT("the drawn part is the square's share of the tile"), RedSquare.Drawn, 36.f * 36.f / (64.f * 64.f), 1e-4f);
    TestFalse(TEXT("a tile of backdrop only"), McpAssetRetrieval::LookOf(RetrievalTile(false, FColor(90, 90, 90)), 64, 64, Empty));
    TestEqual(TEXT("all of it near red"), McpAssetRetrieval::ShareNear(RedSquare, FVector3f(RedSquare.Palette[0].X, RedSquare.Palette[0].Y, RedSquare.Palette[0].Z), 0.05), 1.0, 1e-4);
    // On the checker the editor draws behind meshes, the same square looks the same: both tones are backdrop.
    McpAssetRetrieval::FLook OnChecker;
    TestTrue(TEXT("a red square on the checker"), McpAssetRetrieval::LookOf(RetrievalTile(false, FColor(200, 30, 30), true), 64, 64, OnChecker));
    TestEqual(TEXT("the checker is not drawn"), OnChecker.Drawn, RedSquare.Drawn, 1e-4f);
    TestEqual(TEXT("and the square looks as it does on grey"), McpAssetRetrieval::Unlike(RedSquare, OnChecker), 0.0, 1e-6);
    // A 256 x 256 thumbnail is averaged down to 64 across: the same picture at four times the size looks the same.
    TArray<uint8> Large;
    const TArray<uint8> Small = RetrievalTile(false, FColor(200, 30, 30));
    for (int32 Index = 0; Index < 256 * 256; ++Index)
    {
        const int32 From = ((Index / 256) / 4 * 64 + (Index % 256) / 4) * 4;
        Large.Append({Small[From], Small[From + 1], Small[From + 2], Small[From + 3]});
    }
    McpAssetRetrieval::FLook Scaled;
    TestTrue(TEXT("a large thumbnail"), McpAssetRetrieval::LookOf(Large, 256, 256, Scaled));
    TestEqual(TEXT("looks as its small copy does"), McpAssetRetrieval::Unlike(RedSquare, Scaled), 0.0, 1e-6);
    return true;
}

// Known answers on four assets: a word finds the asset whose caption holds it; like a rock, the other rock of its folder
// comes first and the example itself never; a colour ranks only what holds it; equal scores share a place.
bool FMcpAssetRankTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    McpAssetRetrieval::FIndex Index;
    const TArray<FString> Paths = {TEXT("/Game/Rocks/SM_Rock_01"), TEXT("/Game/Rocks/SM_Rock_02"), TEXT("/Game/Props/SM_Crate"), TEXT("/Game/Props/SM_Drum")};
    Index.Assets.Add(Paths[0], RetrievalEntry(Paths[0], FVector(100, 100, 80), false, FColor(120, 110, 100)));
    Index.Assets.Add(Paths[1], RetrievalEntry(Paths[1], FVector(110, 90, 85), true, FColor(125, 112, 98)));
    Index.Assets.Add(Paths[2], RetrievalEntry(Paths[2], FVector(100, 100, 100), false, FColor(30, 180, 40)));
    Index.Assets.Add(Paths[3], RetrievalEntry(Paths[3], FVector(60, 60, 120), true, FColor(200, 30, 30)));
    Index.Captions.Add(Paths[3], {TEXT("rusty red oil barrel"), {TEXT("metal")}});

    McpAssetRetrieval::FAsk Words;
    Words.Words = McpAssetRetrieval::WordsOf(TEXT("barrel"));
    const TArray<McpAssetRetrieval::FRanked> ByWords = McpAssetRetrieval::Rank(Index, Paths, Words);
    TestEqual(TEXT("one asset holds the word"), ByWords.Num(), 1);
    TestEqual(TEXT("the captioned drum"), ByWords.Num() > 0 ? ByWords[0].Path : FString(), Paths[3]);

    McpAssetRetrieval::FAsk Like;
    Like.Like = Paths[0];
    const TArray<McpAssetRetrieval::FRanked> ByLike = McpAssetRetrieval::Rank(Index, Paths, Like);
    TestEqual(TEXT("the other rock first"), ByLike.Num() > 0 ? ByLike[0].Path : FString(), Paths[1]);
    TestFalse(TEXT("never the example itself"), ByLike.ContainsByPredicate([&](const McpAssetRetrieval::FRanked& Row) { return Row.Path == Paths[0]; }));
    TestTrue(TEXT("it says why"), ByLike.Num() > 0 && ByLike[0].Why.Contains(TEXT("look 1")) && ByLike[0].Why.Contains(TEXT("name 1")));

    McpAssetRetrieval::FAsk Green;
    const FVector4f Crate = Index.Assets[Paths[2]].Look.Palette[0];
    Green.Colour = FVector3f(Crate.X, Crate.Y, Crate.Z);
    const TArray<McpAssetRetrieval::FRanked> ByColour = McpAssetRetrieval::Rank(Index, Paths, Green);
    TestEqual(TEXT("only the green crate"), ByColour.Num(), 1);
    TestEqual(TEXT("is the crate"), ByColour.Num() > 0 ? ByColour[0].Path : FString(), Paths[2]);

    // Equal scores share a place: two assets 100 cm across both come first, and the next is third.
    McpAssetRetrieval::FAsk Size;
    Size.SizeCm = 100.0;
    const TArray<McpAssetRetrieval::FRanked> BySize = McpAssetRetrieval::Rank(Index, Paths, Size);
    TestTrue(TEXT("two of one size share the first place"), BySize.Num() == 4 && BySize[0].Why[0] == TEXT("sizeCm 1") && BySize[1].Why[0] == TEXT("sizeCm 1"));
    TestEqual(TEXT("the next is third"), BySize.Num() == 4 ? BySize[2].Why[0] : FString(), FString(TEXT("sizeCm 3")));
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
