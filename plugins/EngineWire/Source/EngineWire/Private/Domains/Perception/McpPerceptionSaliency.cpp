// Copyright (c) 2024 EngineWire Contributors
//
// Boolean Map Saliency (Zhang and Sclaroff, TPAMI 2016; written from the paper): each OKLab channel is thresholded at
// evenly spaced levels into boolean maps and their complements; in each, the regions that do not touch the frame's
// border are "surrounded" and draw the eye (figure against ground); each such attention map is divided by its L2 norm, so
// a small surrounded region weighs more than a large one, and their mean, blurred, is the saliency. It was the
// strongest non-learned model in the MIT benchmark comparison (Bylinskii et al., ECCV 2016).

#include "Domains/Perception/McpPerception.h"

#include "Foundation/BridgeHelpers/Responses/McpResponseVerify.h"

namespace McpPerception
{
namespace
{
constexpr int32 Levels = 12; // thresholds per channel
constexpr int32 TopActors = 5;

// The cells of a boolean map that no path of set cells joins to the border.
void AddSurrounded(const TArray<bool>& Map, int32 Width, int32 Height, TArray<double>& Sum)
{
    TArray<bool> Reached;
    Reached.Init(false, Map.Num());
    TArray<int32> Open;
    const auto Push = [&](int32 X, int32 Y)
    {
        const int32 Index = Y * Width + X;
        if (Map[Index] && !Reached[Index])
        {
            Reached[Index] = true;
            Open.Add(Index);
        }
    };
    for (int32 X = 0; X < Width; ++X)
    {
        Push(X, 0);
        Push(X, Height - 1);
    }
    for (int32 Y = 0; Y < Height; ++Y)
    {
        Push(0, Y);
        Push(Width - 1, Y);
    }
    while (Open.Num() > 0)
    {
        const int32 Index = Open.Pop(); // Pop(bool) is deprecated from 5.4: the default argument is the same everywhere
        const int32 X = Index % Width;
        const int32 Y = Index / Width;
        if (X > 0) Push(X - 1, Y);
        if (X + 1 < Width) Push(X + 1, Y);
        if (Y > 0) Push(X, Y - 1);
        if (Y + 1 < Height) Push(X, Y + 1);
    }
    int32 Count = 0;
    for (int32 Index = 0; Index < Map.Num(); ++Index)
    {
        Count += Map[Index] && !Reached[Index] ? 1 : 0;
    }
    const double Norm = Count > 0 ? 1.0 / FMath::Sqrt(double(Count)) : 0.0;
    for (int32 Index = 0; Index < Map.Num() && Count > 0; ++Index)
    {
        Sum[Index] += Map[Index] && !Reached[Index] ? Norm : 0.0;
    }
}

// A box blur run twice, rows then columns, which is close to a Gaussian of about the radius. Outside the frame counts as
// zero, so a cell at the edge reads lower, as the eye favours the middle of a picture.
void Blur(TArray<double>& Values, int32 Width, int32 Height, int32 Radius)
{
    TArray<double> Line;
    for (int32 Pass = 0; Pass < 4; ++Pass)
    {
        const bool bRows = Pass % 2 == 0;
        const int32 Lines = bRows ? Height : Width;
        const int32 Length = bRows ? Width : Height;
        for (int32 L = 0; L < Lines; ++L)
        {
            Line.SetNumUninitialized(Length);
            for (int32 I = 0; I < Length; ++I)
            {
                double Total = 0.0;
                for (int32 J = FMath::Max(0, I - Radius); J <= FMath::Min(Length - 1, I + Radius); ++J)
                {
                    Total += Values[bRows ? L * Width + J : J * Width + L];
                }
                Line[I] = Total / (2 * Radius + 1);
            }
            for (int32 I = 0; I < Length; ++I)
            {
                Values[bRows ? L * Width + I : I * Width + L] = Line[I];
            }
        }
    }
}
} // namespace

TSharedPtr<FJsonObject> Saliency(const FFrame& Frame, const AActor* Subject)
{
    const int32 Count = Frame.Width * Frame.Height;
    TArray<double> Sum;
    Sum.Init(0.0, Count);
    TArray<bool> Map;
    Map.SetNumUninitialized(Count);
    for (int32 Channel = 0; Channel < 3; ++Channel)
    {
        float Low = TNumericLimits<float>::Max(), High = -TNumericLimits<float>::Max();
        for (const FVector3f& Lab : Frame.Lab)
        {
            Low = FMath::Min(Low, Lab[Channel]);
            High = FMath::Max(High, Lab[Channel]);
        }
        for (int32 Level = 1; Level <= Levels && High - Low > 1e-3f; ++Level)
        {
            const float Threshold = Low + (High - Low) * Level / (Levels + 1);
            for (const bool bAbove : {true, false})
            {
                for (int32 Index = 0; Index < Count; ++Index)
                {
                    Map[Index] = (Frame.Lab[Index][Channel] > Threshold) == bAbove;
                }
                AddSurrounded(Map, Frame.Width, Frame.Height, Sum);
            }
        }
    }
    Blur(Sum, Frame.Width, Frame.Height, FMath::Max(1, Frame.Width / 40));
    // Each actor by its density, its mean saliency per cell against the frame's mean. Not by its share of the whole (a
    // large wall holds much of the saliency without being what the eye goes to), nor by its peak (the blur spreads a
    // small subject's saliency, and the frame's peak sat on the wall beside it).
    double Total = 0.0;
    int32 Peak = 0;
    TMap<const AActor*, TPair<int32, double>> ByActor; // cells, sum
    for (int32 Index = 0; Index < Count; ++Index)
    {
        Total += Sum[Index];
        Peak = Sum[Index] > Sum[Peak] ? Index : Peak;
        TPair<int32, double>& Entry = ByActor.FindOrAdd(Frame.Actor[Index]);
        ++Entry.Key;
        Entry.Value += Sum[Index];
    }
    const double FrameMean = Total / Count;
    const auto Density = [FrameMean](const TPair<int32, double>& Measured)
    { return FrameMean > 0.0 && Measured.Key > 0 ? Measured.Value / Measured.Key / FrameMean : 0.0; };
    TArray<TPair<const AActor*, TPair<int32, double>>> Ranked;
    for (const TPair<const AActor*, TPair<int32, double>>& Entry : ByActor)
    {
        if (Entry.Key)
        {
            Ranked.Emplace(Entry.Key, Entry.Value);
        }
    }
    Ranked.Sort([&Density](const auto& A, const auto& B) { return Density(A.Value) > Density(B.Value); });
    const auto Share = [Total](double Value) { return Total > 0.0 ? FMath::RoundToDouble(1000.0 * Value / Total) / 10.0 : 0.0; };
    const auto Row = [&](const AActor* Actor, const TPair<int32, double>& Measured)
    {
        TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
        Out->SetStringField(TEXT("actorName"), McpActorRef(Actor));
        Out->SetNumberField(TEXT("density"), FMath::RoundToDouble(100.0 * Density(Measured)) / 100.0);
        Out->SetNumberField(TEXT("share"), Share(Measured.Value));
        return Out;
    };
    TArray<TSharedPtr<FJsonValue>> Top;
    for (int32 Index = 0; Index < Ranked.Num() && Index < TopActors; ++Index)
    {
        Top.Add(MakeShared<FJsonValueObject>(Row(Ranked[Index].Key, Ranked[Index].Value)));
    }
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetArrayField(TEXT("top"), Top);
    Out->SetNumberField(TEXT("background"), Share(ByActor.FindRef(nullptr).Value));
    TSharedPtr<FJsonObject> Focus = MakeShared<FJsonObject>();
    Focus->SetNumberField(TEXT("x"), FMath::RoundToDouble(100.0 * ((Peak % Frame.Width) + 0.5) / Frame.Width) / 100.0);
    Focus->SetNumberField(TEXT("y"), FMath::RoundToDouble(100.0 * ((Peak / Frame.Width) + 0.5) / Frame.Height) / 100.0);
    Out->SetObjectField(TEXT("focus"), Focus); // no actor: the peak can sit beside the actor that makes it
    if (Subject)
    {
        const int32 Rank = Ranked.IndexOfByPredicate([Subject](const auto& Entry) { return Entry.Key == Subject; });
        const TSharedPtr<FJsonObject> Mine = Row(Subject, ByActor.FindRef(Subject));
        Mine->SetNumberField(TEXT("rank"), Rank == INDEX_NONE ? 0 : Rank + 1); // 0: it draws no cell of the frame
        Out->SetObjectField(TEXT("subject"), Mine);
    }
    return Out;
}
} // namespace McpPerception
