// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"

// Nelder-Mead (Foundation/Optimize): the downhill simplex, for minimizing a cost of a few variables that has no
// gradient (a camera's framing judged by traces). The standard coefficients (reflect 1, expand 2, contract 0.5,
// shrink 0.5); deterministic, so the same start gives the same answer.
namespace McpNelderMead
{
struct FResult
{
    TArray<double> Point;
    double Value = 0.0;
    int32 Evaluations = 0;
};

/** The lowest point of Cost found from Start, the simplex's first steps Step (one per variable), stopping once its
 *  values differ by less than Tolerance or after MaxEvaluations. */
FResult Minimize(TFunctionRef<double(const TArray<double>&)> Cost, const TArray<double>& Start, const TArray<double>& Step,
                 double Tolerance, int32 MaxEvaluations);
} // namespace McpNelderMead
