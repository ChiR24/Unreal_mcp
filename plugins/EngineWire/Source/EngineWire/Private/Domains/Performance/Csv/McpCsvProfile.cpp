// McpCsvProfile.cpp — see header.
#include "Domains/Performance/Csv/McpCsvProfile.h"

#include "Dom/JsonValue.h"

namespace
{
// The frame-time column: FrameTime in every 5.x writer; else the first column whose name says so.
int32 FrameColumn(const TArray<FString>& Header)
{
    const int32 Exact = Header.IndexOfByKey(TEXT("FrameTime"));
    return Exact != INDEX_NONE ? Exact : Header.IndexOfByPredicate([](const FString& Name) { return Name.Contains(TEXT("FrameTime")); });
}

double Percentile(TArray<double> Values, double Share)
{
    if (Values.Num() == 0) return 0.0;
    Values.Sort();
    return Values[FMath::Clamp(FMath::FloorToInt(Share * (Values.Num() - 1) + 0.5), 0, Values.Num() - 1)];
}

double Round(double Value) { return FMath::RoundToDouble(Value * 100.0) / 100.0; }

// A column in milliseconds: the Exclusive/ scopes and the thread and GPU times. The writer's other columns are counts and
// megabytes (draw calls, GPUMem/LocalUsedMB), which outranked every timing by their size.
bool IsTiming(const FString& Name)
{
    return Name.StartsWith(TEXT("Exclusive/")) || (Name.EndsWith(TEXT("Time")) && !Name.Contains(TEXT("FrameTime")));
}

TSharedPtr<FJsonObject> StatRow(const FString& Name, double Mean, double Peak)
{
    const TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("stat"), Name);
    Row->SetNumberField(TEXT("meanMs"), Round(Mean));
    Row->SetNumberField(TEXT("peakMs"), Round(Peak));
    return Row;
}
} // namespace

namespace McpCsvProfile
{
TSharedPtr<FJsonObject> Summarize(const FString& Csv, int32 Top, double HitchMs, FString& OutError)
{
    TArray<FString> Lines;
    Csv.ParseIntoArrayLines(Lines, true);
    if (Lines.Num() < 2)
    {
        OutError = TEXT("the CSV has no header row and frame below it.");
        return nullptr;
    }
    TArray<FString> Header;
    Lines[0].ParseIntoArray(Header, TEXT(","), false);
    for (FString& Name : Header) Name.TrimStartAndEndInline();
    TArray<TArray<double>> Columns;
    Columns.SetNum(Header.Num());
    for (int32 Line = 1; Line < Lines.Num(); ++Line)
    {
        TArray<FString> Cells;
        Lines[Line].ParseIntoArray(Cells, TEXT(","), false);
        // The writer's first column is EVENTS, empty on most frames and text on a frame something happened ("Cmd:
        // csvprofile stop"); after the frames it writes the header again, then metadata rows ([HasHeaderRowAtEnd]).
        if (Cells.Num() == 0 || Cells[0].TrimStartAndEnd() == Header[0] || Cells[0].TrimStart().StartsWith(TEXT("["))) break;
        for (int32 Col = 0; Col < Header.Num(); ++Col)
        {
            Columns[Col].Add(Cells.IsValidIndex(Col) && Cells[Col].TrimStartAndEnd().IsNumeric() ? FCString::Atod(*Cells[Col].TrimStartAndEnd()) : 0.0);
        }
    }
    const int32 Frames = Columns.Num() > 0 ? Columns[0].Num() : 0;
    const int32 FrameCol = FrameColumn(Header);
    if (Frames == 0 || FrameCol == INDEX_NONE)
    {
        OutError = Frames == 0 ? TEXT("the CSV holds no frame rows.") : TEXT("the CSV has no FrameTime column.");
        return nullptr;
    }
    Top = FMath::Clamp(Top, 1, 20);
    const TArray<double>& FrameTimes = Columns[FrameCol];
    double Sum = 0.0;
    for (const double Ms : FrameTimes) Sum += Ms;

    const TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetNumberField(TEXT("frames"), Frames);
    const TSharedPtr<FJsonObject> Frame = MakeShared<FJsonObject>();
    Frame->SetNumberField(TEXT("meanMs"), Round(Sum / Frames));
    Frame->SetNumberField(TEXT("p50Ms"), Round(Percentile(FrameTimes, 0.5)));
    Frame->SetNumberField(TEXT("p95Ms"), Round(Percentile(FrameTimes, 0.95)));
    Frame->SetNumberField(TEXT("maxMs"), Round(FMath::Max(FrameTimes)));
    Frame->SetNumberField(TEXT("meanFps"), Round(Sum > 0.0 ? 1000.0 * Frames / Sum : 0.0));
    Out->SetObjectField(TEXT("frameTime"), Frame);

    // Every timing column by its mean and peak.
    TArray<TPair<int32, double>> ByMean;
    TArray<TPair<int32, double>> ByPeak;
    for (int32 Col = 0; Col < Header.Num(); ++Col)
    {
        if (!IsTiming(Header[Col])) continue;
        double ColSum = 0.0;
        for (const double Value : Columns[Col]) ColSum += Value;
        ByMean.Emplace(Col, ColSum / Frames);
        ByPeak.Emplace(Col, FMath::Max(Columns[Col]));
    }
    ByMean.Sort([](const TPair<int32, double>& A, const TPair<int32, double>& B) { return A.Value > B.Value; });
    ByPeak.Sort([](const TPair<int32, double>& A, const TPair<int32, double>& B) { return A.Value > B.Value; });
    TArray<TSharedPtr<FJsonValue>> TopMean;
    TArray<TSharedPtr<FJsonValue>> TopPeak;
    for (int32 Index = 0; Index < FMath::Min(Top, ByMean.Num()); ++Index)
    {
        const int32 Col = ByMean[Index].Key;
        TopMean.Add(MakeShared<FJsonValueObject>(StatRow(Header[Col], ByMean[Index].Value, FMath::Max(Columns[Col]))));
        const int32 PeakCol = ByPeak[Index].Key;
        double PeakSum = 0.0;
        for (const double Value : Columns[PeakCol]) PeakSum += Value;
        TopPeak.Add(MakeShared<FJsonValueObject>(StatRow(Header[PeakCol], PeakSum / Frames, ByPeak[Index].Value)));
    }
    Out->SetArrayField(TEXT("topByMean"), TopMean);
    Out->SetArrayField(TEXT("topByPeak"), TopPeak);

    // The worst frames over HitchMs, each with its largest other columns.
    TArray<int32> Hitches;
    for (int32 Row = 0; Row < Frames; ++Row)
    {
        if (FrameTimes[Row] > HitchMs) Hitches.Add(Row);
    }
    Out->SetNumberField(TEXT("hitchCount"), Hitches.Num());
    Hitches.Sort([&FrameTimes](int32 A, int32 B) { return FrameTimes[A] > FrameTimes[B]; });
    TArray<TSharedPtr<FJsonValue>> Worst;
    for (int32 Index = 0; Index < FMath::Min(Top, Hitches.Num()); ++Index)
    {
        const int32 Row = Hitches[Index];
        TArray<TPair<int32, double>> Cells;
        for (int32 Col = 0; Col < Header.Num(); ++Col)
        {
            if (IsTiming(Header[Col])) Cells.Emplace(Col, Columns[Col][Row]);
        }
        Cells.Sort([](const TPair<int32, double>& A, const TPair<int32, double>& B) { return A.Value > B.Value; });
        TArray<TSharedPtr<FJsonValue>> Stats;
        for (int32 Cell = 0; Cell < FMath::Min(5, Cells.Num()); ++Cell)
        {
            Stats.Add(MakeShared<FJsonValueObject>(StatRow(Header[Cells[Cell].Key], Cells[Cell].Value, Cells[Cell].Value)));
        }
        const TSharedPtr<FJsonObject> Hitch = MakeShared<FJsonObject>();
        Hitch->SetNumberField(TEXT("frame"), Row);
        Hitch->SetNumberField(TEXT("frameMs"), Round(FrameTimes[Row]));
        Hitch->SetArrayField(TEXT("largest"), Stats);
        Worst.Add(MakeShared<FJsonValueObject>(Hitch));
    }
    Out->SetArrayField(TEXT("worstFrames"), Worst);
    return Out;
}
} // namespace McpCsvProfile
