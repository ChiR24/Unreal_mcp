// Comparisons over the lint's intervals: whether a test is decided, and what it tells of a value on the path it holds.

#include "Domains/BlueprintGraph/Lint/Interp/McpBPLintValue.h"

namespace McpBPLint
{
namespace
{
// Whether every value of A lies below every value of B (or at most at it), by the plain bounds or by a shared length.
bool AllBelow(const FRange& A, const FRange& B, bool bStrict)
{
    if (bStrict ? A.Hi < B.Lo : A.Hi <= B.Lo)
    {
        return true;
    }
    return !A.Array.IsNone() && A.Array == B.Array && A.bLenHi && B.bLenLo && (bStrict ? A.LenHi < B.LenLo : A.LenHi <= B.LenLo);
}
} // namespace

FRange Compare(ECompare Op, const FRange& A, const FRange& B)
{
    if (A.bNone || B.bNone)
    {
        return FRange::NoValue();
    }
    const bool bSame = A.Lo == A.Hi && B.Lo == B.Hi && A.Lo == B.Lo;
    const bool bApart = AllBelow(A, B, true) || AllBelow(B, A, true) || (A.IsExactly(0.0) && B.ExcludesZero()) ||
        (B.IsExactly(0.0) && A.ExcludesZero());
    bool bAlways = false;
    bool bNever = false;
    switch (Op)
    {
    case ECompare::Less: bAlways = AllBelow(A, B, true); bNever = AllBelow(B, A, false); break;
    case ECompare::LessEqual: bAlways = AllBelow(A, B, false); bNever = AllBelow(B, A, true); break;
    case ECompare::Greater: bAlways = AllBelow(B, A, true); bNever = AllBelow(A, B, false); break;
    case ECompare::GreaterEqual: bAlways = AllBelow(B, A, false); bNever = AllBelow(A, B, true); break;
    case ECompare::Equal: bAlways = bSame; bNever = bApart; break;
    case ECompare::NotEqual: bAlways = bApart; bNever = bSame; break;
    }
    FRange Range = bAlways ? FRange::Exactly(1.0) : bNever ? FRange::Exactly(0.0) : FRange::Between(0.0, 1.0);
    Range.bOpaque = !bAlways && !bNever && (A.bOpaque || B.bOpaque);
    return Range;
}

ECompare Inverse(ECompare Op)
{
    switch (Op)
    {
    case ECompare::Less: return ECompare::GreaterEqual;
    case ECompare::LessEqual: return ECompare::Greater;
    case ECompare::Greater: return ECompare::LessEqual;
    case ECompare::GreaterEqual: return ECompare::Less;
    case ECompare::Equal: return ECompare::NotEqual;
    case ECompare::NotEqual: return ECompare::Equal;
    }
    return Op;
}

ECompare Mirror(ECompare Op)
{
    switch (Op)
    {
    case ECompare::Less: return ECompare::Greater;
    case ECompare::LessEqual: return ECompare::GreaterEqual;
    case ECompare::Greater: return ECompare::Less;
    case ECompare::GreaterEqual: return ECompare::LessEqual;
    default: return Op;
    }
}

FRange Refine(const FRange& A, ECompare Op, const FRange& B, bool bInteger)
{
    if (A.bNone || B.bNone)
    {
        return FRange::NoValue();
    }
    FRange Range = A;
    const double Step = bInteger ? 1.0 : 0.0;
    // A <= B + Offset: B's upper bounds, its length bound included, bound A.
    const auto Below = [&Range, &B](double Offset)
    {
        Range.Hi = FMath::Min(Range.Hi, AddBound(B.Hi, Offset));
        if (B.bLenHi)
        {
            Range.bLenLo = Range.bLenLo && Range.Array == B.Array;
            Range.Array = B.Array;
            Range.bLenHi = true;
            Range.LenHi = B.LenHi + Offset;
        }
    };
    const auto Above = [&Range, &B](double Offset)
    {
        Range.Lo = FMath::Max(Range.Lo, AddBound(B.Lo, Offset));
        if (B.bLenLo)
        {
            Range.bLenHi = Range.bLenHi && Range.Array == B.Array;
            Range.Array = B.Array;
            Range.bLenLo = true;
            Range.LenLo = B.LenLo + Offset;
        }
    };
    switch (Op)
    {
    case ECompare::Less: Below(-Step); Range.bNonZero |= B.Hi <= 0.0; break;
    case ECompare::LessEqual: Below(0.0); Range.bNonZero |= B.Hi < 0.0; break;
    case ECompare::Greater: Above(Step); Range.bNonZero |= B.Lo >= 0.0; break;
    case ECompare::GreaterEqual: Above(0.0); Range.bNonZero |= B.Lo > 0.0; break;
    case ECompare::Equal: Below(0.0); Above(0.0); Range.bNonZero |= B.ExcludesZero(); break;
    case ECompare::NotEqual:
        if (B.Lo == B.Hi)
        {
            Range.bNonZero |= B.Lo == 0.0;
            Range.Lo += bInteger && Range.Lo == B.Lo ? 1.0 : 0.0;
            Range.Hi -= bInteger && Range.Hi == B.Lo ? 1.0 : 0.0;
        }
        break;
    }
    Range.bNone = Range.Lo > Range.Hi;
    return Range;
}
} // namespace McpBPLint
