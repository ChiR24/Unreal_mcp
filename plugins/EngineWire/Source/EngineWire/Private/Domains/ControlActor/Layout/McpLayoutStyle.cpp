#include "Domains/ControlActor/Layout/McpLayoutStyle.h"

#include "Dom/JsonValue.h"

namespace McpLayoutStyle
{
namespace Detail
{
FString KeyOf(const FString& Anchor, const FString& Other, const FString& Kind)
{
    return Anchor + TEXT("|") + Other + TEXT("|") + Kind;
}

template <int32 N>
int32 Mode(const int32 (&Bins)[N])
{
    int32 Best = 0;
    for (int32 Index = 1; Index < N; ++Index)
    {
        Best = Bins[Index] > Bins[Best] ? Index : Best;
    }
    return Best;
}

template <int32 N>
TArray<TSharedPtr<FJsonValue>> BinsToJson(const int32 (&Bins)[N])
{
    TArray<TSharedPtr<FJsonValue>> Out;
    for (const int32 Each : Bins) Out.Add(MakeShared<FJsonValueNumber>(Each));
    return Out;
}

template <int32 N>
void BinsFromJson(const TSharedPtr<FJsonObject>& Json, const TCHAR* Field, int32 (&Bins)[N])
{
    const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
    for (int32 Index = 0; Json->TryGetArrayField(Field, List) && List && Index < FMath::Min(N, List->Num()); ++Index)
    {
        Bins[Index] = static_cast<int32>((*List)[Index]->AsNumber());
    }
}
} // namespace Detail

int32 DistanceBin(double Distance)
{
    return FMath::Clamp(static_cast<int32>(Distance / DistanceBinCm), 0, DistanceBins - 1);
}

int32 AngleBin(double Degrees)
{
    const double Turned = FMath::Fmod(FMath::Fmod(Degrees, 360.0) + 360.0, 360.0);
    return FMath::RoundToInt(Turned / 45.0) % AngleBins;
}

void Add(FLevelStats& Stats, const FObservation& Seen)
{
    FPairStats& Pair = Stats.Pairs.FindOrAdd(Detail::KeyOf(Seen.Anchor, Seen.Other, Seen.Kind));
    ++Pair.Count;
    ++Pair.Distance[DistanceBin(Seen.Distance)];
    ++Pair.Bearing[AngleBin(Seen.Bearing)];
    ++Pair.Yaw[AngleBin(Seen.Yaw)];
}

TArray<FSuggestion> Suggest(const TArray<FLevelStats>& Levels, const FString& AnchorMesh, int32 Count)
{
    int32 Anchors = 0;
    TMap<FString, FPairStats> Merged; // "other|kind"
    for (const FLevelStats& Level : Levels)
    {
        Anchors += Level.Copies.FindRef(AnchorMesh);
        for (const TPair<FString, FPairStats>& Pair : Level.Pairs)
        {
            TArray<FString> Parts;
            if (Pair.Key.ParseIntoArray(Parts, TEXT("|")) != 3 || Parts[0] != AnchorMesh || Parts[1] == AnchorMesh)
            {
                continue;
            }
            FPairStats& Into = Merged.FindOrAdd(Parts[1] + TEXT("|") + Parts[2]);
            Into.Count += Pair.Value.Count;
            for (int32 Index = 0; Index < DistanceBins; ++Index) Into.Distance[Index] += Pair.Value.Distance[Index];
            for (int32 Index = 0; Index < AngleBins; ++Index) Into.Bearing[Index] += Pair.Value.Bearing[Index];
            for (int32 Index = 0; Index < AngleBins; ++Index) Into.Yaw[Index] += Pair.Value.Yaw[Index];
        }
    }
    TMap<FString, FSuggestion> Best; // per mesh, its most common relation
    for (const TPair<FString, FPairStats>& Pair : Merged)
    {
        FString Mesh, Kind;
        Pair.Key.Split(TEXT("|"), &Mesh, &Kind);
        const double PerAnchor = static_cast<double>(Pair.Value.Count) / FMath::Max(1, Anchors);
        const FSuggestion* Held = Best.Find(Mesh);
        if (PerAnchor >= 0.1 && (!Held || PerAnchor > Held->PerAnchor))
        {
            Best.Add(Mesh, {Mesh, Kind, PerAnchor, (Detail::Mode(Pair.Value.Distance) + 0.5) * DistanceBinCm,
                            Detail::Mode(Pair.Value.Bearing) * 45.0, Detail::Mode(Pair.Value.Yaw) * 45.0});
        }
    }
    TArray<FSuggestion> Out;
    Best.GenerateValueArray(Out);
    Out.Sort([](const FSuggestion& A, const FSuggestion& B) { return A.PerAnchor != B.PerAnchor ? A.PerAnchor > B.PerAnchor : A.Mesh < B.Mesh; });
    if (Out.Num() > Count) Out.SetNum(Count);
    return Out;
}

TSharedPtr<FJsonObject> ToJson(const FLevelStats& Stats)
{
    TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Copies = MakeShared<FJsonObject>();
    for (const TPair<FString, int32>& Each : Stats.Copies) Copies->SetNumberField(Each.Key, Each.Value);
    TArray<TSharedPtr<FJsonValue>> Pairs;
    for (const TPair<FString, FPairStats>& Each : Stats.Pairs)
    {
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("key"), Each.Key);
        Row->SetNumberField(TEXT("count"), Each.Value.Count);
        Row->SetArrayField(TEXT("distance"), Detail::BinsToJson(Each.Value.Distance));
        Row->SetArrayField(TEXT("bearing"), Detail::BinsToJson(Each.Value.Bearing));
        Row->SetArrayField(TEXT("yaw"), Detail::BinsToJson(Each.Value.Yaw));
        Pairs.Add(MakeShared<FJsonValueObject>(Row));
    }
    Json->SetObjectField(TEXT("copies"), Copies);
    Json->SetArrayField(TEXT("pairs"), Pairs);
    return Json;
}

FLevelStats FromJson(const TSharedPtr<FJsonObject>& Json)
{
    FLevelStats Stats;
    const TSharedPtr<FJsonObject>* Copies = nullptr;
    if (Json.IsValid() && Json->TryGetObjectField(TEXT("copies"), Copies) && Copies)
    {
        for (const auto& Each : (*Copies)->Values) // the key type differs between engines (FString, then not on 5.8)
        {
            Stats.Copies.Add(FString(Each.Key.Len(), *Each.Key), static_cast<int32>(Each.Value->AsNumber()));
        }
    }
    const TArray<TSharedPtr<FJsonValue>>* Pairs = nullptr;
    for (int32 Index = 0; Json.IsValid() && Json->TryGetArrayField(TEXT("pairs"), Pairs) && Pairs && Index < Pairs->Num(); ++Index)
    {
        const TSharedPtr<FJsonObject> Row = (*Pairs)[Index]->AsObject();
        FString Key;
        if (!Row.IsValid() || !Row->TryGetStringField(TEXT("key"), Key))
        {
            continue;
        }
        FPairStats& Pair = Stats.Pairs.Add(Key);
        Pair.Count = static_cast<int32>(Row->GetNumberField(TEXT("count")));
        Detail::BinsFromJson(Row, TEXT("distance"), Pair.Distance);
        Detail::BinsFromJson(Row, TEXT("bearing"), Pair.Bearing);
        Detail::BinsFromJson(Row, TEXT("yaw"), Pair.Yaw);
    }
    return Stats;
}
} // namespace McpLayoutStyle
