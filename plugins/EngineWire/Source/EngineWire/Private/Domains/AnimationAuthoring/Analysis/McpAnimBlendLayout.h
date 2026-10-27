// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"

// Where locomotion clips go in a blend space, from each clip's ground velocity in the character's frame (X forward, Y
// right, cm/s): the axes (speed from 0, direction -180..180 wrapping, signed speeds symmetric) and every position a
// clip takes. A clip within 10 degrees of backward takes both direction edges (wrapping wraps the input, it mirrors no
// sample), an idle (under 10 cm/s) takes every direction a moving clip uses, and a position another clip already holds
// is refused for the later clip.
namespace McpAnimBlendLayout
{
enum class EQuantity : uint8
{
    Speed,
    Direction,
    ForwardSpeed,
    RightSpeed
};

bool ParseQuantity(const FString& Text, EQuantity& Out);
const TCHAR* QuantityName(EQuantity Quantity);

struct FBlendLayoutAxis
{
    EQuantity Quantity = EQuantity::Speed;
    double Min = 0.0;
    double Max = 0.0;
    bool bWrap = false;
};

struct FClipPlace
{
    double Speed = 0.0;
    double Direction = 0.0; // degrees, 0 forward, +90 right
    bool bIdle = false;
    TArray<FVector> Positions; // x, y (y 0 on one axis)
    int32 Refused = 0;         // positions dropped: another clip holds them
};

/** Axes and positions for clips of the given velocities on the given quantities (one or two). */
void Layout(const TArray<FVector2D>& Velocities, const TArray<EQuantity>& Quantities, TArray<FBlendLayoutAxis>& OutAxes, TArray<FClipPlace>& OutClips);

/** Widens Axis to hold Value (a kept sample), never past -180..180 on a wrapping axis. */
void Cover(FBlendLayoutAxis& Axis, double Value);
} // namespace McpAnimBlendLayout
