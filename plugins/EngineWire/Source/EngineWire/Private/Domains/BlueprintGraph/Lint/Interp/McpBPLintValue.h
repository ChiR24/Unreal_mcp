#pragma once

#include "CoreMinimal.h"

// What the lint's abstract interpreter (McpBPLintInterp.cpp) knows of a number where a node runs: an interval, and
// optionally bounds against the length of one array variable (Length(A) + c), so a ForLoop over 0..Length(A) is seen to
// reach one past A's end and a check i < Length(A) keeps i inside it. Booleans are numbers in [0, 1].
namespace McpBPLint
{
// A bound this far out means none. Finite on purpose: MSVC's /fp:fast (the default before build settings V7) may assume
// no infinities, so arithmetic saturates here instead.
constexpr double Unbounded = 1.0e300;

struct FRange
{
    double Lo = -Unbounded;
    double Hi = Unbounded;
    /** With Array set: Length(Array) + LenLo <= value (bLenLo) and value <= Length(Array) + LenHi (bLenHi). */
    FName Array;
    double LenLo = 0.0;
    double LenHi = 0.0;
    bool bLenLo = false;
    bool bLenHi = false;
    /** 0 is ruled out though Lo <= 0 <= Hi (a != 0 check). */
    bool bNonZero = false;
    /** Rests on what the lint cannot see (a public member's value, an unknown function's result): no finding on it alone. */
    bool bOpaque = false;
    /** No value: no path reaches here with the condition that made it. */
    bool bNone = false;

    static FRange Unknown(bool bInOpaque = false);
    static FRange Exactly(double Value);
    static FRange Between(double InLo, double InHi);
    static FRange NoValue();
    /** Length(InArray) itself: Known (at least 0), and the bounds Length + 0. */
    static FRange LengthOf(FName InArray, const FRange& Known);
    bool IsExactly(double Value) const;
    bool ExcludesZero() const;
    bool CanBeZero() const;
};

enum class ECompare : uint8
{
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Equal,
    NotEqual,
};

/** Bound arithmetic that saturates at Unbounded (McpBPLintValue.cpp). */
double SaturateBound(double Value);
double AddBound(double A, double B);
double MulBound(double A, double B);

bool SameRange(const FRange& A, const FRange& B);
FRange Join(const FRange& A, const FRange& B);
/** Old joined with New, a bound that moved going straight to Unbounded, so a loop's values settle. */
FRange Widen(const FRange& Old, const FRange& New);
/** A without its length bounds. */
FRange NumbersOnly(const FRange& A);
/** The values both A and B allow (two facts about one value). */
FRange Meet(const FRange& A, const FRange& B);

FRange Add(const FRange& A, const FRange& B);
FRange Negate(const FRange& A);
FRange Sub(const FRange& A, const FRange& B);
/** McpBPLintArith.cpp */
FRange Mul(const FRange& A, const FRange& B);
/** A / B; unknown where B can be 0. Integer division rounds toward zero. */
FRange Div(const FRange& A, const FRange& B, bool bInteger);
/** A % B: smaller than |B|, with A's sign. */
FRange Rem(const FRange& A, const FRange& B);
FRange AbsOf(const FRange& A);
FRange MinOf(const FRange& A, const FRange& B);
FRange MaxOf(const FRange& A, const FRange& B);
FRange RoundOf(const FRange& A, double (*Round)(double));

/** McpBPLintCompare.cpp: whether A Op B holds, as a Boolean: [1, 1] always, [0, 0] never, [0, 1] either. */
FRange Compare(ECompare Op, const FRange& A, const FRange& B);
/** The opposite test (a Branch's False exit). */
ECompare Inverse(ECompare Op);
/** The same test with its sides swapped (A < B is B > A). */
ECompare Mirror(ECompare Op);
/** What A can be on a path where A Op B holds. */
FRange Refine(const FRange& A, ECompare Op, const FRange& B, bool bInteger);
} // namespace McpBPLint
