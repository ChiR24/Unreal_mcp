// Copyright (c) 2024 EngineWire Contributors

#include "Foundation/Optimize/McpNelderMead.h"

namespace McpNelderMead
{
namespace
{
TArray<double> NelderMeadMix(const TArray<double>& From, const TArray<double>& Toward, double Amount)
{
    TArray<double> Out;
    for (int32 Index = 0; Index < From.Num(); ++Index)
    {
        Out.Add(From[Index] + Amount * (Toward[Index] - From[Index]));
    }
    return Out;
}
} // namespace

FResult Minimize(TFunctionRef<double(const TArray<double>&)> Cost, const TArray<double>& Start, const TArray<double>& Step,
                 double Tolerance, int32 MaxEvaluations)
{
    const int32 Dims = Start.Num();
    FResult Out;
    TArray<TArray<double>> Points = {Start};
    for (int32 Dim = 0; Dim < Dims; ++Dim)
    {
        TArray<double> Corner = Start;
        Corner[Dim] += Step.IsValidIndex(Dim) ? Step[Dim] : 1.0;
        Points.Add(Corner);
    }
    TArray<double> Values;
    for (const TArray<double>& Point : Points)
    {
        Values.Add(Cost(Point));
    }
    Out.Evaluations = Points.Num();
    TArray<int32> Order;
    for (int32 Index = 0; Index <= Dims; ++Index)
    {
        Order.Add(Index);
    }
    while (true)
    {
        Order.Sort([&Values](int32 L, int32 R) { return Values[L] != Values[R] ? Values[L] < Values[R] : L < R; });
        const int32 Best = Order[0];
        const int32 Worst = Order[Dims];
        if (Dims == 0 || Values[Worst] - Values[Best] < Tolerance || Out.Evaluations >= MaxEvaluations)
        {
            break;
        }
        TArray<double> Centroid;
        Centroid.Init(0.0, Dims);
        for (int32 Rank = 0; Rank < Dims; ++Rank)
        {
            for (int32 Dim = 0; Dim < Dims; ++Dim)
            {
                Centroid[Dim] += Points[Order[Rank]][Dim] / Dims;
            }
        }
        // Reflect the worst point through the centroid of the others, then expand, contract or shrink.
        const TArray<double> Reflected = NelderMeadMix(Centroid, Points[Worst], -1.0);
        const double ReflectedValue = Cost(Reflected);
        ++Out.Evaluations;
        if (ReflectedValue < Values[Best])
        {
            const TArray<double> Expanded = NelderMeadMix(Centroid, Points[Worst], -2.0);
            const double ExpandedValue = Cost(Expanded);
            ++Out.Evaluations;
            const bool bExpand = ExpandedValue < ReflectedValue;
            Points[Worst] = bExpand ? Expanded : Reflected;
            Values[Worst] = bExpand ? ExpandedValue : ReflectedValue;
            continue;
        }
        if (ReflectedValue < Values[Order[Dims - 1]])
        {
            Points[Worst] = Reflected;
            Values[Worst] = ReflectedValue;
            continue;
        }
        const bool bOutside = ReflectedValue < Values[Worst];
        const TArray<double> Contracted = NelderMeadMix(Centroid, bOutside ? Reflected : Points[Worst], 0.5);
        const double ContractedValue = Cost(Contracted);
        ++Out.Evaluations;
        if (ContractedValue < (bOutside ? ReflectedValue : Values[Worst]))
        {
            Points[Worst] = Contracted;
            Values[Worst] = ContractedValue;
            continue;
        }
        for (int32 Rank = 1; Rank <= Dims; ++Rank) // shrink toward the best
        {
            Points[Order[Rank]] = NelderMeadMix(Points[Best], Points[Order[Rank]], 0.5);
            Values[Order[Rank]] = Cost(Points[Order[Rank]]);
            ++Out.Evaluations;
        }
    }
    Out.Point = Points[Order[0]];
    Out.Value = Values[Order[0]];
    return Out;
}
} // namespace McpNelderMead
