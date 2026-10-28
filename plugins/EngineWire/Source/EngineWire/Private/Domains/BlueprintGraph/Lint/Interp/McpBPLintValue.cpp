// Interval arithmetic for the lint's abstract interpreter (see the header). Every result holds every value the operation
// can give for values in its inputs, so a check that finds nothing wrong on a range holds for each value in it.

#include "Domains/BlueprintGraph/Lint/Interp/McpBPLintValue.h"

namespace McpBPLint
{
double SaturateBound(double Value)
{
    return Value > Unbounded ? Unbounded : Value < -Unbounded ? -Unbounded : Value;
}

// A sum of bounds on the same side: a missing bound stays missing.
double AddBound(double A, double B)
{
    if (A >= Unbounded || B >= Unbounded)
    {
        return Unbounded;
    }
    return A <= -Unbounded || B <= -Unbounded ? -Unbounded : SaturateBound(A + B);
}

double MulBound(double A, double B)
{
    if (A == 0.0 || B == 0.0)
    {
        return 0.0;
    }
    if (FMath::Abs(A) >= Unbounded || FMath::Abs(B) >= Unbounded)
    {
        return (A > 0.0) == (B > 0.0) ? Unbounded : -Unbounded;
    }
    return SaturateBound(A * B);
}

FRange FRange::Unknown(bool bInOpaque)
{
    FRange Range;
    Range.bOpaque = bInOpaque;
    return Range;
}

FRange FRange::Exactly(double Value)
{
    return Between(Value, Value);
}

FRange FRange::Between(double InLo, double InHi)
{
    FRange Range;
    Range.Lo = SaturateBound(InLo);
    Range.Hi = SaturateBound(InHi);
    Range.bNone = Range.Lo > Range.Hi;
    return Range;
}

FRange FRange::NoValue()
{
    FRange Range;
    Range.bNone = true;
    return Range;
}

FRange FRange::LengthOf(FName InArray, const FRange& Known)
{
    FRange Range = Known;
    Range.Lo = FMath::Max(Range.Lo, 0.0);
    Range.Array = InArray;
    Range.LenLo = Range.LenHi = 0.0;
    Range.bLenLo = Range.bLenHi = !InArray.IsNone();
    return Range;
}

bool FRange::IsExactly(double Value) const
{
    return !bNone && Lo == Value && Hi == Value;
}

bool FRange::ExcludesZero() const
{
    return bNone || bNonZero || Lo > 0.0 || Hi < 0.0;
}

bool FRange::CanBeZero() const
{
    return !ExcludesZero();
}

bool SameRange(const FRange& A, const FRange& B)
{
    return A.bNone == B.bNone && A.Lo == B.Lo && A.Hi == B.Hi && A.Array == B.Array && A.bLenLo == B.bLenLo &&
        A.bLenHi == B.bLenHi && (!A.bLenLo || A.LenLo == B.LenLo) && (!A.bLenHi || A.LenHi == B.LenHi) &&
        A.bNonZero == B.bNonZero && A.bOpaque == B.bOpaque;
}

FRange Join(const FRange& A, const FRange& B)
{
    if (A.bNone || B.bNone)
    {
        return A.bNone ? B : A;
    }
    FRange Range;
    Range.Lo = FMath::Min(A.Lo, B.Lo);
    Range.Hi = FMath::Max(A.Hi, B.Hi);
    if (!A.Array.IsNone() && A.Array == B.Array)
    {
        Range.Array = A.Array;
        Range.bLenLo = A.bLenLo && B.bLenLo;
        Range.bLenHi = A.bLenHi && B.bLenHi;
        Range.LenLo = FMath::Min(A.LenLo, B.LenLo);
        Range.LenHi = FMath::Max(A.LenHi, B.LenHi);
    }
    Range.bNonZero = A.ExcludesZero() && B.ExcludesZero();
    Range.bOpaque = A.bOpaque || B.bOpaque;
    return Range;
}

FRange Widen(const FRange& Old, const FRange& New)
{
    FRange Range = Join(Old, New);
    if (Old.bNone || New.bNone)
    {
        return Range;
    }
    Range.Lo = New.Lo < Old.Lo ? -Unbounded : Range.Lo;
    Range.Hi = New.Hi > Old.Hi ? Unbounded : Range.Hi;
    Range.bLenLo = Range.bLenLo && New.LenLo >= Old.LenLo;
    Range.bLenHi = Range.bLenHi && New.LenHi <= Old.LenHi;
    return Range;
}

FRange NumbersOnly(const FRange& A)
{
    FRange Range = A;
    Range.Array = NAME_None;
    Range.bLenLo = Range.bLenHi = false;
    return Range;
}

FRange Meet(const FRange& A, const FRange& B)
{
    if (A.bNone || B.bNone)
    {
        return FRange::NoValue();
    }
    FRange Range = A;
    Range.Lo = FMath::Max(A.Lo, B.Lo);
    Range.Hi = FMath::Min(A.Hi, B.Hi);
    Range.bNonZero = A.bNonZero || B.bNonZero;
    Range.bOpaque = A.bOpaque || B.bOpaque; // what is left unknown still rests on what the lint cannot see
    Range.bNone = Range.Lo > Range.Hi || (Range.Lo == 0.0 && Range.Hi == 0.0 && Range.bNonZero);
    return Range;
}

FRange Add(const FRange& A, const FRange& B)
{
    if (A.bNone || B.bNone)
    {
        return FRange::NoValue();
    }
    FRange Range;
    Range.Lo = AddBound(A.Lo, B.Lo);
    Range.Hi = AddBound(A.Hi, B.Hi);
    Range.bOpaque = A.bOpaque || B.bOpaque;
    // A bound against a length survives a number added to it.
    const FRange* Length = !A.Array.IsNone() ? &A : !B.Array.IsNone() ? &B : nullptr;
    const FRange* Number = Length == &A ? &B : &A;
    if (Length && Number->Array.IsNone())
    {
        Range.Array = Length->Array;
        Range.bLenLo = Length->bLenLo && Number->Lo > -Unbounded;
        Range.bLenHi = Length->bLenHi && Number->Hi < Unbounded;
        Range.LenLo = Length->LenLo + Number->Lo;
        Range.LenHi = Length->LenHi + Number->Hi;
    }
    return Range;
}

FRange Negate(const FRange& A)
{
    FRange Range = A.bNone ? FRange::NoValue() : FRange::Between(-A.Hi, -A.Lo);
    Range.bNonZero = A.bNonZero;
    Range.bOpaque = A.bOpaque;
    return Range;
}

FRange Sub(const FRange& A, const FRange& B)
{
    return Add(A, Negate(B));
}
} // namespace McpBPLint
