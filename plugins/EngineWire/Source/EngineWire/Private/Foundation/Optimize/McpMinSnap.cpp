// Copyright (c) 2024 EngineWire Contributors

#include "Foundation/Optimize/McpMinSnap.h"

namespace McpMinSnap
{
namespace
{
constexpr int32 SnapOrder = 8;   // coefficients per leg: degree 7
constexpr int32 SnapKept = 4;    // position and three derivatives carry on across a point

// k! / (k - d)!: the factor the d-th derivative brings down onto t^k.
double Falling(int32 K, int32 D)
{
    double Out = 1.0;
    for (int32 Step = 0; Step < D; ++Step)
    {
        Out *= K - Step;
    }
    return Out;
}

// The inverse of a leg's map from coefficients to the derivatives at its two ends (rows: the four at t = 0, then the
// four at t = Seconds), and its snap cost in those end derivatives, Ainv^T Q Ainv.
bool LegCost(double Seconds, TArray<double>& OutInverse, TArray<double>& OutCost)
{
    TArray<double> Map;
    Map.Init(0.0, SnapOrder * SnapOrder);
    OutInverse.Init(0.0, SnapOrder * SnapOrder);
    for (int32 D = 0; D < SnapKept; ++D)
    {
        Map[D * SnapOrder + D] = Falling(D, D);
        for (int32 K = D; K < SnapOrder; ++K)
        {
            Map[(SnapKept + D) * SnapOrder + K] = Falling(K, D) * FMath::Pow(Seconds, static_cast<double>(K - D));
        }
        OutInverse[D * SnapOrder + D] = 1.0;
        OutInverse[(SnapKept + D) * SnapOrder + SnapKept + D] = 1.0;
    }
    if (!SolveDense(Map, OutInverse, SnapOrder, SnapOrder))
    {
        return false;
    }
    TArray<double> Snap; // Q: the integral of the squared fourth derivative over the leg
    Snap.Init(0.0, SnapOrder * SnapOrder);
    for (int32 J = 4; J < SnapOrder; ++J)
    {
        for (int32 K = 4; K < SnapOrder; ++K)
        {
            const int32 Power = J + K - 7;
            Snap[J * SnapOrder + K] = Falling(J, 4) * Falling(K, 4) * FMath::Pow(Seconds, static_cast<double>(Power)) / Power;
        }
    }
    OutCost.Init(0.0, SnapOrder * SnapOrder);
    for (int32 Row = 0; Row < SnapOrder; ++Row)
    {
        for (int32 Column = 0; Column < SnapOrder; ++Column)
        {
            double Sum = 0.0;
            for (int32 J = 0; J < SnapOrder; ++J)
            {
                for (int32 K = 0; K < SnapOrder; ++K)
                {
                    Sum += OutInverse[J * SnapOrder + Row] * Snap[J * SnapOrder + K] * OutInverse[K * SnapOrder + Column];
                }
            }
            OutCost[Row * SnapOrder + Column] = Sum;
        }
    }
    return true;
}

// Where a leg's end derivative (0-7: four at its start, four at its end) sits among every point's four.
int32 Shared(int32 Leg, int32 Local)
{
    return Local < SnapKept ? Leg * SnapKept + Local : (Leg + 1) * SnapKept + Local - SnapKept;
}
} // namespace

bool SolveDense(TArray<double>& A, TArray<double>& B, int32 N, int32 Columns)
{
    for (int32 Pivot = 0; Pivot < N; ++Pivot)
    {
        int32 Best = Pivot;
        for (int32 Row = Pivot + 1; Row < N; ++Row)
        {
            Best = FMath::Abs(A[Row * N + Pivot]) > FMath::Abs(A[Best * N + Pivot]) ? Row : Best;
        }
        if (FMath::Abs(A[Best * N + Pivot]) < 1e-300)
        {
            return false;
        }
        for (int32 Column = 0; Column < N; ++Column)
        {
            Swap(A[Pivot * N + Column], A[Best * N + Column]);
        }
        for (int32 Column = 0; Column < Columns; ++Column)
        {
            Swap(B[Pivot * Columns + Column], B[Best * Columns + Column]);
        }
        for (int32 Row = 0; Row < N; ++Row)
        {
            const double Factor = Row == Pivot ? 0.0 : A[Row * N + Pivot] / A[Pivot * N + Pivot];
            for (int32 Column = Pivot; Column < N && Factor != 0.0; ++Column)
            {
                A[Row * N + Column] -= Factor * A[Pivot * N + Column];
            }
            for (int32 Column = 0; Column < Columns && Factor != 0.0; ++Column)
            {
                B[Row * Columns + Column] -= Factor * B[Pivot * Columns + Column];
            }
        }
    }
    for (int32 Row = 0; Row < N; ++Row)
    {
        for (int32 Column = 0; Column < Columns; ++Column)
        {
            B[Row * Columns + Column] /= A[Row * N + Row];
        }
    }
    return true;
}

bool Solve(const TArray<FVector>& Points, const TArray<double>& Seconds, TArray<FLeg>& OutLegs)
{
    const int32 Legs = Points.Num() - 1;
    if (Legs < 1 || Seconds.Num() != Legs)
    {
        return false;
    }
    // Solved in time scaled to a mean leg of one second (the powers of t stay near 1); the scale is taken out after.
    double Scale = 0.0;
    for (const double Leg : Seconds)
    {
        if (!(Leg > 0.0))
        {
            return false;
        }
        Scale += Leg / Legs;
    }
    const int32 Count = (Legs + 1) * SnapKept;
    TArray<double> Total;
    Total.Init(0.0, Count * Count);
    TArray<TArray<double>> Inverses;
    for (int32 Leg = 0; Leg < Legs; ++Leg)
    {
        TArray<double> Cost;
        if (!LegCost(Seconds[Leg] / Scale, Inverses.AddDefaulted_GetRef(), Cost))
        {
            return false;
        }
        for (int32 Row = 0; Row < SnapOrder; ++Row)
        {
            for (int32 Column = 0; Column < SnapOrder; ++Column)
            {
                Total[Shared(Leg, Row) * Count + Shared(Leg, Column)] += Cost[Row * SnapOrder + Column];
            }
        }
    }
    // Fixed: every position, and rest (velocity, acceleration, jerk 0) at both ends; free: the rest at inner points.
    TArray<double> Known;
    Known.Init(0.0, Count * 3);
    TArray<int32> Free;
    for (int32 Point = 0; Point <= Legs; ++Point)
    {
        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            Known[Point * SnapKept * 3 + Axis] = Points[Point][Axis];
        }
        for (int32 D = 1; D < SnapKept && Point > 0 && Point < Legs; ++D)
        {
            Free.Add(Point * SnapKept + D);
        }
    }
    const int32 Unknown = Free.Num();
    TArray<double> System;
    System.Init(0.0, Unknown * Unknown);
    TArray<double> Right;
    Right.Init(0.0, Unknown * 3);
    for (int32 Row = 0; Row < Unknown; ++Row)
    {
        for (int32 Column = 0; Column < Unknown; ++Column)
        {
            System[Row * Unknown + Column] = Total[Free[Row] * Count + Free[Column]];
        }
        for (int32 Index = 0; Index < Count; ++Index)
        {
            for (int32 Axis = 0; Axis < 3 && !Free.Contains(Index); ++Axis)
            {
                Right[Row * 3 + Axis] -= Total[Free[Row] * Count + Index] * Known[Index * 3 + Axis];
            }
        }
    }
    if (Unknown > 0 && !SolveDense(System, Right, Unknown, 3))
    {
        return false;
    }
    for (int32 Row = 0; Row < Unknown; ++Row)
    {
        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            Known[Free[Row] * 3 + Axis] = Right[Row * 3 + Axis];
        }
    }
    OutLegs.Reset();
    for (int32 Leg = 0; Leg < Legs; ++Leg)
    {
        FLeg& Out = OutLegs.AddDefaulted_GetRef();
        Out.Seconds = Seconds[Leg];
        for (int32 K = 0; K < SnapOrder; ++K)
        {
            FVector Coefficient = FVector::ZeroVector;
            for (int32 Local = 0; Local < SnapOrder; ++Local)
            {
                const int32 Index = Shared(Leg, Local);
                Coefficient += Inverses[Leg][K * SnapOrder + Local] * FVector(Known[Index * 3], Known[Index * 3 + 1], Known[Index * 3 + 2]);
            }
            Out.C[K] = Coefficient / FMath::Pow(Scale, static_cast<double>(K)); // back to real seconds
        }
    }
    return true;
}

FVector Evaluate(const FLeg& Leg, double T, int32 Derivative)
{
    FVector Out = FVector::ZeroVector;
    for (int32 K = Derivative; K < SnapOrder; ++K)
    {
        Out += Leg.C[K] * (Falling(K, Derivative) * FMath::Pow(T, static_cast<double>(K - Derivative)));
    }
    return Out;
}
} // namespace McpMinSnap
