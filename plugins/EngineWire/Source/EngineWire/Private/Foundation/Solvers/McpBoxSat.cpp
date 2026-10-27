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

bool RayHits(const FFootprint& Box, const FVector2D& Origin, const FVector2D& Direction, double& OutDistance)
{
    // Slabs in the box's own frame: the ray is inside both between its entry and exit.
    const double Cos = FMath::Cos(Box.Yaw);
    const double Sin = FMath::Sin(Box.Yaw);
    const FVector2D Off = Origin - Box.Center;
    const double From[2] = {Cos * Off.X + Sin * Off.Y, Cos * Off.Y - Sin * Off.X};
    const double Along[2] = {Cos * Direction.X + Sin * Direction.Y, Cos * Direction.Y - Sin * Direction.X};
    const double Half[2] = {Box.Half.X, Box.Half.Y};
    double Enter = -TNumericLimits<double>::Max();
    double Exit = TNumericLimits<double>::Max();
    for (int32 Axis = 0; Axis < 2; ++Axis)
    {
        if (FMath::Abs(Along[Axis]) < 1e-9)
        {
            if (FMath::Abs(From[Axis]) > Half[Axis])
            {
                return false;
            }
            continue;
        }
        const double Near = (-Half[Axis] - From[Axis]) / Along[Axis];
        const double Far = (Half[Axis] - From[Axis]) / Along[Axis];
        Enter = FMath::Max(Enter, FMath::Min(Near, Far));
        Exit = FMath::Min(Exit, FMath::Max(Near, Far));
    }
    OutDistance = FMath::Max(Enter, 0.0);
    return Enter <= Exit && Exit >= 0.0;
}

bool Faces(const FFootprint& From, const FFootprint& To, double Grace, double& OutOff)
{
    const FVector2D Front(FMath::Cos(From.Yaw), FMath::Sin(From.Yaw));
    const FVector2D Toward = (To.Center - From.Center).GetSafeNormal(); // Normalize returns nothing in 5.0, bool in 5.8
    OutOff = Toward.IsNearlyZero() ? 0.0 : FMath::Acos(FMath::Clamp(FVector2D::DotProduct(Front, Toward), -1.0, 1.0));
    double Distance = 0.0;
    return OutOff <= Grace || RayHits(To, From.Center, Front, Distance);
}
} // namespace McpBoxSat
