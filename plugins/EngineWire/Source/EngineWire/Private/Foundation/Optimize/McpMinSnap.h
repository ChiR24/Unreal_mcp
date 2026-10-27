// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"

// Minimum snap (Foundation/Optimize): the smoothest path through given points, a polynomial of degree 7 per leg whose
// position and first three derivatives run on unbroken from one leg to the next, at rest at both ends, with the least
// integrated squared snap (the fourth derivative). Solved as Richter, Bry and Roy's unconstrained quadratic program
// (ISRR 2013) in closed form: the free derivatives at the inner points are -R_PP^-1 R_FP^T times the fixed ones.
namespace McpMinSnap
{
/** One leg: eight coefficients per axis (position = sum of C[k] t^k, t the seconds into the leg) and its duration. */
struct FLeg
{
    FVector C[8];
    double Seconds = 1.0;
};

/** The legs through Points (two or more) taking Seconds[i] from point i to point i + 1; false when a duration is not
 *  positive or the system is singular. */
bool Solve(const TArray<FVector>& Points, const TArray<double>& Seconds, TArray<FLeg>& OutLegs);

/** The Derivative-th derivative (0 position, 1 velocity, 2 acceleration) of Leg at T seconds into it. */
FVector Evaluate(const FLeg& Leg, double T, int32 Derivative = 0);

/** Solves A x = B in place for an N x N row-major A and B with Columns columns (Gaussian elimination, partial
 *  pivoting); false when A is singular. */
bool SolveDense(TArray<double>& A, TArray<double>& B, int32 N, int32 Columns);
} // namespace McpMinSnap
