// Copyright (c) 2024 EngineWire Contributors
//
// find_assets_like's ranking (McpAssetRank.h): each criterion asked ranks the candidates it can judge, and the rankings
// are fused by Reciprocal Rank Fusion (Cormack, Clarke and Buettcher 2009: score = sum of 1 / (60 + place)), which needs
// no tuning between scores of different kinds. Words are ranked by BM25 (Robertson and Zaragoza 2009).

#include "Domains/AssetQuery/Retrieval/McpAssetRank.h"

namespace McpAssetRetrieval
{
namespace
{
constexpr double RankFusion = 60.0;
constexpr double RankK1 = 1.2;
constexpr double RankB = 0.75;
constexpr double RankColourGap = 0.12; // OKLab distance that still counts as the colour asked
constexpr double RankColourShare = 0.05;

// An asset's words, each weighted by where it came from: caption 2, tags 1.5, name, folders and class 1.
TMap<FString, double> RankFieldWords(const FString& Path, const FIndexed& Entry, const FIndex& Index)
{
    TMap<FString, double> Out;
    for (const FString& Word : Entry.Words)
    {
        Out.FindOrAdd(Word) += 1.0;
    }
    for (const FString& Word : WordsOf(Entry.Class))
    {
        Out.FindOrAdd(Word) += 1.0;
    }
    if (const FCaption* Caption = Index.Captions.Find(Path))
    {
        for (const FString& Word : WordsOf(Caption->Text))
        {
            Out.FindOrAdd(Word) += 2.0;
        }
        for (const FString& Tag : Caption->Tags)
        {
            for (const FString& Word : WordsOf(Tag))
            {
                Out.FindOrAdd(Word) += 1.5;
            }
        }
    }
    return Out;
}

// Each ranked candidate with its place (from 1), best first.
using FPlaces = TArray<TPair<int32, int32>>;

// Candidates ordered by Score, higher first; only those Score keeps (set) are ranked. Equal scores share a place (the
// first of them): a criterion that cannot tell candidates apart (four meshes of one size) must not order them by path.
FPlaces RankOrder(int32 Count, TFunctionRef<TOptional<double>(int32)> Score)
{
    TArray<TPair<double, int32>> Scored;
    for (int32 At = 0; At < Count; ++At)
    {
        const TOptional<double> Value = Score(At);
        if (Value.IsSet())
        {
            Scored.Add(TPair<double, int32>(Value.GetValue(), At));
        }
    }
    Scored.Sort([](const TPair<double, int32>& L, const TPair<double, int32>& R) { return L.Key != R.Key ? L.Key > R.Key : L.Value < R.Value; });
    FPlaces Out;
    for (int32 Place = 0; Place < Scored.Num(); ++Place)
    {
        const bool bTied = Place > 0 && FMath::IsNearlyEqual(Scored[Place].Key, Scored[Place - 1].Key, 1e-9 * FMath::Max(1.0, FMath::Abs(Scored[Place].Key)));
        Out.Add(TPair<int32, int32>(Scored[Place].Value, bTied ? Out.Last().Value : Place + 1));
    }
    return Out;
}

// BM25 of each candidate's weighted words for Words; candidates holding none of them are not ranked.
FPlaces RankWords(const TArray<TMap<FString, double>>& Fields, const TArray<FString>& Asked)
{
    TArray<FString> Words; // each word once: a repeated one counted twice in Holding gave it a negative idf
    for (const FString& Word : Asked)
    {
        Words.AddUnique(Word);
    }
    double Average = 0.0;
    TMap<FString, int32> Holding;
    for (const TMap<FString, double>& Field : Fields)
    {
        for (const TPair<FString, double>& Pair : Field)
        {
            Average += Pair.Value / Fields.Num();
        }
        for (const FString& Word : Words)
        {
            Holding.FindOrAdd(Word) += Field.Contains(Word) ? 1 : 0;
        }
    }
    return RankOrder(Fields.Num(), [&](int32 At) -> TOptional<double>
    {
        double Length = 0.0, Score = 0.0;
        for (const TPair<FString, double>& Pair : Fields[At])
        {
            Length += Pair.Value;
        }
        for (const FString& Word : Words)
        {
            const double* Count = Fields[At].Find(Word);
            const double Idf = FMath::Loge(1.0 + (Fields.Num() - Holding[Word] + 0.5) / (Holding[Word] + 0.5));
            Score += Count ? Idf * *Count * (RankK1 + 1.0) / (*Count + RankK1 * (1.0 - RankB + RankB * Length / FMath::Max(Average, 1.0))) : 0.0;
        }
        return Score > 0.0 ? TOptional<double>(Score) : TOptional<double>();
    });
}
} // namespace

TArray<FRanked> Rank(const FIndex& Index, const TArray<FString>& Candidates, const FAsk& Ask)
{
    TArray<FString> Paths = Candidates;
    Paths.Remove(Ask.Like); // never like itself (nor placed first in an order, pushing the rest down)
    TArray<TMap<FString, double>> Fields;
    for (const FString& Path : Paths)
    {
        Fields.Add(RankFieldWords(Path, Index.Assets[Path], Index));
    }
    const auto Of = [&Index, &Paths](int32 At) -> const FIndexed& { return Index.Assets[Paths[At]]; };
    TArray<TPair<FString, FPlaces>> Orders;
    if (Ask.Words.Num() > 0)
    {
        Orders.Emplace(TEXT("words"), RankWords(Fields, Ask.Words));
    }
    const FIndexed* Like = Ask.Like.IsEmpty() ? nullptr : Index.Assets.Find(Ask.Like);
    if (Like) // its own words (folders and name, caption and tags): the same pack, the same kind of thing
    {
        TArray<FString> LikeWords;
        RankFieldWords(Ask.Like, *Like, Index).GetKeys(LikeWords);
        // A number names a variant, not a kind: SM_Car_06 shares 06 with its own wheels, not with the other cars.
        LikeWords.RemoveAll([](const FString& Word) { return Word.IsNumeric(); });
        Orders.Emplace(TEXT("name"), RankWords(Fields, LikeWords));
    }
    if (Like && Like->bHasLook)
    {
        Orders.Emplace(TEXT("look"), RankOrder(Paths.Num(), [&](int32 At) -> TOptional<double>
        {
            return Of(At).bHasLook ? TOptional<double>(-Unlike(Like->Look, Of(At).Look)) : TOptional<double>();
        }));
    }
    if (Like && !Like->Size.IsZero())
    {
        Orders.Emplace(TEXT("size"), RankOrder(Paths.Num(), [&](int32 At) -> TOptional<double>
        {
            return Of(At).Size.IsZero() ? TOptional<double>()
                : TOptional<double>(-FMath::Abs(FMath::Loge(FMath::Max(Of(At).Size.GetMax(), 1.0) / FMath::Max(Like->Size.GetMax(), 1.0))));
        }));
    }
    if (Ask.Colour.IsSet())
    {
        Orders.Emplace(TEXT("colour"), RankOrder(Paths.Num(), [&](int32 At) -> TOptional<double>
        {
            const double Share = Of(At).bHasLook ? ShareNear(Of(At).Look, Ask.Colour.GetValue(), RankColourGap) : 0.0;
            return Share > RankColourShare ? TOptional<double>(Share) : TOptional<double>();
        }));
    }
    if (Ask.SizeCm > 0.0)
    {
        Orders.Emplace(TEXT("sizeCm"), RankOrder(Paths.Num(), [&](int32 At) -> TOptional<double>
        {
            return Of(At).Size.IsZero() ? TOptional<double>() : TOptional<double>(-FMath::Abs(FMath::Loge(FMath::Max(Of(At).Size.GetMax(), 1.0) / Ask.SizeCm)));
        }));
    }
    TMap<int32, FRanked> Fused;
    for (const TPair<FString, FPlaces>& Order : Orders)
    {
        for (const TPair<int32, int32>& Placed : Order.Value)
        {
            const int32 At = Placed.Key, Place = Placed.Value;
            FRanked& Entry = Fused.FindOrAdd(At);
            Entry.Path = Paths[At];
            Entry.Score += 1.0 / (RankFusion + Place);
            Entry.Why.Add(FString::Printf(TEXT("%s %d"), *Order.Key, Place));
        }
    }
    TArray<FRanked> Out;
    Fused.GenerateValueArray(Out);
    Out.Sort([](const FRanked& L, const FRanked& R) { return L.Score != R.Score ? L.Score > R.Score : L.Path.Compare(R.Path, ESearchCase::CaseSensitive) < 0; });
    return Out;
}
} // namespace McpAssetRetrieval
