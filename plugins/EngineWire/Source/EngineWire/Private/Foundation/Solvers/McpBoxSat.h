// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"

// Boxes turned about Z, seen from above (Foundation/Solvers): how far apart or how deep into each other two of them are,
// by the separating-axis test over the four box axes. A solver needs a measure rather than a yes or no, so it has a
// slope to follow out of an overlap or toward a contact.
namespace McpBoxSat
{
struct FFootprint
{
    FVector2D Center = FVector2D::ZeroVector;
    FVector2D Half = FVector2D::ZeroVector; // half the sides along the box's own X and Y
    double Yaw = 0.0;                       // radians
};

/** Half the width of the box's shadow on a unit axis. */
double Reach(const FFootprint& Box, const FVector2D& Axis);

/** The largest gap along the four axes: above 0 the boxes are at least that far apart, below 0 they overlap and minus it
 *  is how far one must move to leave the other. OutAxis, when given, is that axis, pointing from A toward B. */
double Separation(const FFootprint& A, const FFootprint& B, FVector2D* OutAxis = nullptr);

/** How far one box must move to leave the other; 0 when they are apart. */
inline double Depth(const FFootprint& A, const FFootprint& B)
{
    return FMath::Max(0.0, -Separation(A, B));
}

/** How far Inner's corners reach outside Outer along Outer's axes, summed; 0 when Inner lies within it. */
double Outside(const FFootprint& Inner, const FFootprint& Outer);

/** Whether a ray from Origin along the unit Direction meets the box; OutDistance is where it first does (0 inside). */
bool RayHits(const FFootprint& Box, const FVector2D& Origin, const FVector2D& Direction, double& OutDistance);

/** Whether From's front (its own +X axis) looks at To: the ray along it meets To, or points within Grace radians of
 *  To's centre (a small thing far off). OutOff is how far it points from To's centre, in radians. */
bool Faces(const FFootprint& From, const FFootprint& To, double Grace, double& OutOff);
} // namespace McpBoxSat
