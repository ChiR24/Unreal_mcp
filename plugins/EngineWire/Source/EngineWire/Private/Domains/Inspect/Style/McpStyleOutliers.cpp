#include "Domains/Inspect/Style/McpStyleOutliers.h"

namespace McpStyleOutliers
{
double Median(TArray<double> Values)
{
    if (Values.Num() == 0)
    {
        return 0.0;
    }
    Values.Sort();
    const int32 Mid = Values.Num() / 2;
    return Values.Num() % 2 ? Values[Mid] : 0.5 * (Values[Mid - 1] + Values[Mid]);
}

TArray<FOutlier> Find(const TArray<FSample>& Samples, double Threshold, int32 MinGroup)
{
    TMap<FString, TArray<int32>> Groups;
    for (int32 Index = 0; Index < Samples.Num(); ++Index)
    {
        Groups.FindOrAdd(Samples[Index].Group).Add(Index);
    }
    TArray<FOutlier> Out;
    for (const TPair<FString, TArray<int32>>& Group : Groups)
    {
        if (Group.Value.Num() < MinGroup)
        {
            continue;
        }
        TArray<double> Values;
        for (const int32 Index : Group.Value) Values.Add(Samples[Index].Value);
        const double Mid = Median(Values);
        TArray<double> Deviations;
        for (const double Value : Values) Deviations.Add(FMath::Abs(Value - Mid));
        const double Spread = FMath::Max(1.4826 * Median(Deviations), 0.1 * FMath::Abs(Mid));
        for (const int32 Index : Group.Value)
        {
            const double Score = Spread > 0.0 ? FMath::Abs(Samples[Index].Value - Mid) / Spread : 0.0;
            if (Score > Threshold)
            {
                Out.Add({Samples[Index].Name, Group.Key, Samples[Index].Value, Mid, Score});
            }
        }
    }
    Out.Sort([](const FOutlier& A, const FOutlier& B) { return A.Score > B.Score; });
    return Out;
}
} // namespace McpStyleOutliers
