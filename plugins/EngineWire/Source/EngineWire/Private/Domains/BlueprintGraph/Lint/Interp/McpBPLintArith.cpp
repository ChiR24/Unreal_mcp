// Products, quotients and the library's rounding, clamping and absolute values over the lint's intervals.

#include "Domains/BlueprintGraph/Lint/Interp/McpBPLintValue.h"

#include <cmath>

namespace McpBPLint
{
namespace
{
double ReciprocalBound(double Value)
{
    return FMath::Abs(Value) >= Unbounded ? 0.0 : SaturateBound(1.0 / Value);
}

FRange ProductHull(double A, double B, double C, double D, bool bOpaque)
{
    FRange Range;
    Range.Lo = FMath::Min(FMath::Min(A, B), FMath::Min(C, D));
    Range.Hi = FMath::Max(FMath::Max(A, B), FMath::Max(C, D));
    Range.bOpaque = bOpaque;
    return Range;
}
} // namespace

FRange Mul(const FRange& A, const FRange& B)
{
    if (A.bNone || B.bNone)
    {
        return FRange::NoValue();
    }
    FRange Range = ProductHull(MulBound(A.Lo, B.Lo), MulBound(A.Lo, B.Hi), MulBound(A.Hi, B.Lo), MulBound(A.Hi, B.Hi), A.bOpaque || B.bOpaque);
    Range.bNonZero = A.ExcludesZero() && B.ExcludesZero();
    return Range;
}

FRange Div(const FRange& A, const FRange& B, bool bInteger)
{
    if (A.bNone || B.bNone)
    {
        return FRange::NoValue();
    }
    if (B.Lo <= 0.0 && B.Hi >= 0.0) // can be 0, or lies on both sides of it: no bound on the quotient
    {
        return FRange::Unknown(A.bOpaque || B.bOpaque);
    }
    const double Low = ReciprocalBound(B.Lo);
    const double High = ReciprocalBound(B.Hi);
    FRange Range = Mul(A, FRange::Between(FMath::Min(Low, High), FMath::Max(Low, High)));
    Range.bNonZero = false; // 1 / 3 in integers is 0
    if (bInteger)
    {
        Range.Lo = std::trunc(Range.Lo);
        Range.Hi = std::trunc(Range.Hi);
    }
    Range.bOpaque = A.bOpaque || B.bOpaque;
    return Range;
}

FRange Rem(const FRange& A, const FRange& B)
{
    if (A.bNone || B.bNone)
    {
        return FRange::NoValue();
    }
    const double Size = FMath::Max(FMath::Abs(B.Lo), FMath::Abs(B.Hi));
    FRange Range = FRange::Between(A.Lo >= 0.0 ? 0.0 : FMath::Max(A.Lo, -Size), A.Hi <= 0.0 ? 0.0 : FMath::Min(A.Hi, Size));
    Range.bOpaque = A.bOpaque || B.bOpaque;
    return Range;
}

FRange AbsOf(const FRange& A)
{
    if (A.bNone || A.Lo >= 0.0)
    {
        return A;
    }
    FRange Range = A.Hi <= 0.0 ? Negate(A) : FRange::Between(0.0, FMath::Max(-A.Lo, A.Hi));
    Range.bNonZero = A.bNonZero;
    Range.bOpaque = A.bOpaque;
    return Range;
}

FRange MinOf(const FRange& A, const FRange& B)
{
    if (A.bNone || B.bNone)
    {
        return A.bNone ? B : A;
    }
    FRange Range = FRange::Between(FMath::Min(A.Lo, B.Lo), FMath::Min(A.Hi, B.Hi));
    // The smaller of two is at most either: a length bound above either one bounds it.
    const FRange* Bounded = A.bLenHi ? &A : B.bLenHi ? &B : nullptr;
    if (Bounded)
    {
        Range.Array = Bounded->Array;
        Range.bLenHi = true;
        Range.LenHi = Bounded->LenHi;
    }
    Range.bNonZero = A.Hi < 0.0 || B.Hi < 0.0;
    Range.bOpaque = A.bOpaque || B.bOpaque;
    return Range;
}

FRange MaxOf(const FRange& A, const FRange& B)
{
    if (A.bNone || B.bNone)
    {
        return A.bNone ? B : A;
    }
    FRange Range = FRange::Between(FMath::Max(A.Lo, B.Lo), FMath::Max(A.Hi, B.Hi));
    const FRange* Bounded = A.bLenLo ? &A : B.bLenLo ? &B : nullptr;
    if (Bounded)
    {
        Range.Array = Bounded->Array;
        Range.bLenLo = true;
        Range.LenLo = Bounded->LenLo;
    }
    Range.bNonZero = A.Lo > 0.0 || B.Lo > 0.0;
    Range.bOpaque = A.bOpaque || B.bOpaque;
    return Range;
}

FRange RoundOf(const FRange& A, double (*Round)(double))
{
    if (A.bNone)
    {
        return A;
    }
    FRange Range = FRange::Between(Round(A.Lo), Round(A.Hi));
    Range.bOpaque = A.bOpaque;
    return Range;
}
} // namespace McpBPLint
