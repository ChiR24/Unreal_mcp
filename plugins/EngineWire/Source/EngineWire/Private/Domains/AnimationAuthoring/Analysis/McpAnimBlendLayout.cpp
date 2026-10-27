// Copyright (c) 2024 EngineWire Contributors

#include "Domains/AnimationAuthoring/Analysis/McpAnimBlendLayout.h"

namespace McpAnimBlendLayout
{
namespace
{
constexpr double BlendLayoutIdleSpeed = 10.0; // cm/s
constexpr double BlendLayoutBackward = 170.0; // degrees from forward
constexpr double BlendLayoutStep = 50.0;      // speed axes end on a multiple of this

double BlendLayoutRound(double Value)
{
    return FMath::RoundToDouble(Value * 10.0) / 10.0;
}

double BlendLayoutTop(double Value)
{
    return FMath::Max(BlendLayoutStep, FMath::CeilToDouble(Value / BlendLayoutStep - 1e-9) * BlendLayoutStep);
}

// A clip's value on an axis; an idle stands still on every speed axis.
double BlendLayoutValue(EQuantity Quantity, const FVector2D& Velocity, const FClipPlace& Place)
{
    switch (Quantity)
    {
    case EQuantity::Speed:
        return Place.bIdle ? 0.0 : Place.Speed;
    case EQuantity::Direction:
        return Place.Direction;
    case EQuantity::ForwardSpeed:
        return Place.bIdle ? 0.0 : Velocity.X;
    default:
        return Place.bIdle ? 0.0 : Velocity.Y;
    }
}

bool BlendLayoutBackwardOf(const FClipPlace& Place)
{
    return !Place.bIdle && FMath::Abs(Place.Direction) >= BlendLayoutBackward;
}
} // namespace

bool ParseQuantity(const FString& Text, EQuantity& Out)
{
    static const TCHAR* const Names[] = {TEXT("speed"), TEXT("direction"), TEXT("forwardSpeed"), TEXT("rightSpeed")};
    for (int32 Index = 0; Index < int32(UE_ARRAY_COUNT(Names)); ++Index)
    {
        if (Text.Equals(Names[Index], ESearchCase::IgnoreCase))
        {
            Out = static_cast<EQuantity>(Index);
            return true;
        }
    }
    return false;
}

const TCHAR* QuantityName(EQuantity Quantity)
{
    static const TCHAR* const Names[] = {TEXT("Speed"), TEXT("Direction"), TEXT("Forward Speed"), TEXT("Right Speed")};
    return Names[static_cast<int32>(Quantity)];
}

void Layout(const TArray<FVector2D>& Velocities, const TArray<EQuantity>& Quantities, TArray<FBlendLayoutAxis>& OutAxes, TArray<FClipPlace>& OutClips)
{
    OutClips.Reset();
    OutClips.SetNum(Velocities.Num());
    for (int32 Clip = 0; Clip < Velocities.Num(); ++Clip)
    {
        FClipPlace& Place = OutClips[Clip];
        Place.Speed = Velocities[Clip].Size();
        Place.bIdle = Place.Speed < BlendLayoutIdleSpeed;
        Place.Direction = Place.bIdle ? 0.0 : FMath::RadiansToDegrees(FMath::Atan2(Velocities[Clip].Y, Velocities[Clip].X));
    }
    OutAxes.Reset();
    for (const EQuantity Quantity : Quantities)
    {
        FBlendLayoutAxis& Axis = OutAxes.AddDefaulted_GetRef();
        Axis.Quantity = Quantity;
        Axis.bWrap = Quantity == EQuantity::Direction;
        double Top = 0.0;
        for (int32 Clip = 0; Clip < Velocities.Num(); ++Clip)
        {
            Top = FMath::Max(Top, FMath::Abs(BlendLayoutValue(Quantity, Velocities[Clip], OutClips[Clip])));
        }
        Axis.Max = Axis.bWrap ? 180.0 : BlendLayoutTop(Top);
        Axis.Min = Quantity == EQuantity::Speed ? 0.0 : -Axis.Max;
    }

    // The directions the moving clips take (a backward one both edges): an idle takes every one of them.
    const int32 DirectionAxis = Quantities.IndexOfByKey(EQuantity::Direction);
    TArray<double> Directions;
    for (const FClipPlace& Place : OutClips)
    {
        if (BlendLayoutBackwardOf(Place))
        {
            Directions.AddUnique(-180.0);
            Directions.AddUnique(180.0);
        }
        else if (!Place.bIdle)
        {
            Directions.AddUnique(BlendLayoutRound(Place.Direction));
        }
    }
    if (Directions.Num() == 0)
    {
        Directions.Add(0.0);
    }

    // Positions in clip order: one a former clip holds is refused (the blend space holds one sample per point).
    TArray<FVector> Taken;
    for (int32 Clip = 0; Clip < OutClips.Num(); ++Clip)
    {
        FClipPlace& Place = OutClips[Clip];
        const TArray<double> Own = DirectionAxis == INDEX_NONE ? TArray<double>{0.0}
            : Place.bIdle ? Directions
            : BlendLayoutBackwardOf(Place) ? TArray<double>{-180.0, 180.0}
            : TArray<double>{BlendLayoutRound(Place.Direction)};
        for (const double Direction : Own)
        {
            FVector Position = FVector::ZeroVector;
            for (int32 Axis = 0; Axis < Quantities.Num(); ++Axis)
            {
                Position[Axis] = Axis == DirectionAxis ? Direction : BlendLayoutRound(BlendLayoutValue(Quantities[Axis], Velocities[Clip], Place));
            }
            if (Taken.ContainsByPredicate([&Position](const FVector& Other) { return Other.Equals(Position, 1e-3); }))
            {
                ++Place.Refused;
                continue;
            }
            Taken.Add(Position);
            Place.Positions.Add(Position);
        }
    }
}

void Cover(FBlendLayoutAxis& Axis, double Value)
{
    if (Axis.bWrap)
    {
        return;
    }
    if (Value > Axis.Max)
    {
        Axis.Max = BlendLayoutTop(Value);
    }
    if (Value < Axis.Min)
    {
        Axis.Min = -BlendLayoutTop(-Value);
    }
}
} // namespace McpAnimBlendLayout
