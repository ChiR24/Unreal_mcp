// Copyright (c) 2024 EngineWire Contributors

#include "Foundation/Solvers/McpBoxSat.h"

namespace McpBoxSat
{
double Reach(const FFootprint& Box, const FVector2D& Axis)
{
    const double Cos = FMath::Cos(Box.Yaw);
    const double Sin = FMath::Sin(Box.Yaw);
    return Box.Half.X * FMath::Abs(Cos * Axis.X + Sin * Axis.Y) + Box.Half.Y * FMath::Abs(Cos * Axis.Y - Sin * Axis.X);
}

double Separation(const FFootprint& A, const FFootprint& B, FVector2D* OutAxis)
{
    const FVector2D Between = B.Center - A.Center;
    double Largest = -TNumericLimits<double>::Max();
    for (const double Yaw : {A.Yaw, B.Yaw})
    {
        const double Cos = FMath::Cos(Yaw);
        const double Sin = FMath::Sin(Yaw);
        for (const FVector2D& Axis : {FVector2D(Cos, Sin), FVector2D(-Sin, Cos)})
        {
            const double Along = FVector2D::DotProduct(Between, Axis);
            const double Gap = FMath::Abs(Along) - Reach(A, Axis) - Reach(B, Axis);
            if (Gap > Largest)
            {
                Largest = Gap;
                if (OutAxis)
                {
                    *OutAxis = Along < 0.0 ? -Axis : Axis;
                }
            }
        }
    }
    return Largest;
}

double Outside(const FFootprint& Inner, const FFootprint& Outer)
{
    const double CosI = FMath::Cos(Inner.Yaw);
    const double SinI = FMath::Sin(Inner.Yaw);
    const double CosO = FMath::Cos(Outer.Yaw);
    const double SinO = FMath::Sin(Outer.Yaw);
    double Sum = 0.0;
    for (const double U : {-1.0, 1.0})
    {
        for (const double V : {-1.0, 1.0})
        {
            const FVector2D Local(U * Inner.Half.X, V * Inner.Half.Y);
            const FVector2D Off = Inner.Center + FVector2D(CosI * Local.X - SinI * Local.Y, SinI * Local.X + CosI * Local.Y) - Outer.Center;
            Sum += FMath::Max(0.0, FMath::Abs(CosO * Off.X + SinO * Off.Y) - Outer.Half.X) +
                   FMath::Max(0.0, FMath::Abs(CosO * Off.Y - SinO * Off.X) - Outer.Half.Y);
        }
    }
    return Sum;
}
} // namespace McpBoxSat
