// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"

// A room's acoustics from rays cast from a listener: its volume and surface from where 256 Fibonacci rays land, its
// mean free path, its size, and its reverberation time twice over: Eyring's formula (valid only for a closed,
// well-proportioned room with even absorption) and a ray-traced energy decay (rays reflected, 30% of them scattered,
// energy times 1 - absorption per bounce) read as T30 off its Schroeder curve, which also holds in corridors, flat
// halls and rooms with openings.
namespace McpAcoustics
{
struct FHit
{
    double Distance = 0.0; // cm
    FVector Normal = FVector::ZeroVector; // facing back along the ray
};

/** Where a ray from From along Direction (unit) first meets a surface within MaxDistance cm; false for none. */
using FTrace = TFunctionRef<bool(const FVector& From, const FVector& Direction, double MaxDistance, FHit& Out)>;

struct FRoom
{
    double OpenShare = 0.0;      // rays that met nothing within the reach: open sky, a doorway, a window
    double VolumeM3 = 0.0;       // as seen from the listener
    double SurfaceM2 = 0.0;
    double MeanFreePathM = 0.0;  // 4 V / S
    double FirstReflectionMs = 0.0;
    double LengthM = 0.0;        // the walls' extent along the room's own floor axes (longer, shorter) and up
    double WidthM = 0.0;
    double HeightM = 0.0;
    double Proportion = 0.0;     // the largest of the three over the smallest
    FBox Bounds = FBox(ForceInit); // the first hits, world axes, cm
    TOptional<double> EyringSeconds; // closed and well proportioned only
    TOptional<double> DecaySeconds;  // T30 (or T20) of the ray-traced decay; unset when it fell less than 25 dB
};

/** Past this proportion a room is long or flat and Eyring's formula does not hold. */
constexpr double LongRoom = 3.0;

/** The room around Listener with surfaces of the given absorption (0.02-0.9), reached within MaxDistance cm. */
FRoom Estimate(FTrace Trace, const FVector& Listener, double Absorption, double MaxDistance);
} // namespace McpAcoustics
