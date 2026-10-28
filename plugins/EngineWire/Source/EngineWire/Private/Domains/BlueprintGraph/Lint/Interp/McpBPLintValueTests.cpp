// Known answers for the lint's interval arithmetic (McpBPLintValue.h): sums, products and quotients, a divisor that can
// be 0, integer truncation, the bounds a ForLoop over 0..Length(A) and an Is Valid Index check give, comparisons decided
// or not, widening, and a join of two ranges that each leave out 0.

#include "Domains/BlueprintGraph/Lint/Interp/McpBPLintValue.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpLintValuesTest,
    "EngineWire.Lint.Values",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpLintValuesTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace McpBPLint;
    const auto Is = [this](const TCHAR* What, const FRange& Range, double Lo, double Hi)
    {
        TestTrue(What, !Range.bNone && Range.Lo == Lo && Range.Hi == Hi);
    };
    Is(TEXT("[1, 2] + [10, 20]"), Add(FRange::Between(1, 2), FRange::Between(10, 20)), 11, 22);
    Is(TEXT("[1, 2] - [10, 20]"), Sub(FRange::Between(1, 2), FRange::Between(10, 20)), -19, -8);
    Is(TEXT("[-2, 3] * [4, 5]"), Mul(FRange::Between(-2, 3), FRange::Between(4, 5)), -10, 15);
    Is(TEXT("[1, 5] / 2 in integers"), Div(FRange::Between(1, 5), FRange::Exactly(2), true), 0, 2);
    Is(TEXT("[-7, 7] % 3"), Rem(FRange::Between(-7, 7), FRange::Exactly(3)), -3, 3);
    TestTrue(TEXT("a quotient by a divisor that can be 0 is unknown"), Div(FRange::Exactly(1), FRange::Between(0, 5), false).Hi >= Unbounded);
    Is(TEXT("Clamp([-50, 200], 0, 100)"), MinOf(MaxOf(FRange::Between(-50, 200), FRange::Exactly(0)), FRange::Exactly(100)), 0, 100);

    // ForLoop 0..Length(A): the index reaches Length(A) + 0, one past the last item.
    const FName Array(TEXT("Items"));
    const FRange Length = FRange::LengthOf(Array, FRange::Between(0, Unbounded));
    const FRange Index = Refine(Refine(FRange::Unknown(), ECompare::GreaterEqual, FRange::Exactly(0), true), ECompare::LessEqual, Length, true);
    TestTrue(TEXT("ForLoop to Length reaches the length"), Index.Array == Array && Index.bLenHi && Index.LenHi == 0.0 && Index.Lo == 0.0);
    const FRange LastIndex = Sub(Length, FRange::Exactly(1));
    const FRange Inside = Refine(FRange::Between(0, Unbounded), ECompare::LessEqual, LastIndex, true);
    TestTrue(TEXT("ForLoop to Last Index stays inside"), Inside.bLenHi && Inside.LenHi == -1.0);
    const FRange Checked = Refine(FRange::Unknown(), ECompare::Less, Length, true);
    TestTrue(TEXT("i < Length(A) keeps i at most Length - 1"), Checked.bLenHi && Checked.LenHi == -1.0);

    const FRange Positive = Refine(FRange::Unknown(), ECompare::Greater, FRange::Exactly(0), true);
    TestTrue(TEXT("x > 0 in integers is at least 1"), Positive.Lo == 1.0 && !Positive.CanBeZero());
    const FRange Real = Refine(FRange::Unknown(), ECompare::Greater, FRange::Exactly(0), false);
    TestTrue(TEXT("x > 0 in floats rules out 0"), !Real.CanBeZero());
    const FRange NotZero = Refine(FRange::Between(-5, 5), ECompare::NotEqual, FRange::Exactly(0), true);
    TestTrue(TEXT("x != 0 rules out 0"), !NotZero.CanBeZero() && NotZero.Lo == -5.0);
    TestTrue(TEXT("a contradiction leaves no value"), Refine(FRange::Between(0, 3), ECompare::Greater, FRange::Exactly(5), true).bNone);

    TestTrue(TEXT("[0, 5] < [6, 10] always"), Compare(ECompare::Less, FRange::Between(0, 5), FRange::Between(6, 10)).IsExactly(1));
    TestTrue(TEXT("[0, 5] >= [6, 10] never"), Compare(ECompare::GreaterEqual, FRange::Between(0, 5), FRange::Between(6, 10)).IsExactly(0));
    const FRange Either = Compare(ECompare::Less, FRange::Between(0, 5), FRange::Between(3, 10));
    TestTrue(TEXT("[0, 5] < [3, 10] either way"), Either.Lo == 0.0 && Either.Hi == 1.0);
    TestTrue(TEXT("Length - 1 < Length always"), Compare(ECompare::Less, LastIndex, Length).IsExactly(1));

    // What a check comparing two variables says of their difference (A > B on floats: A - B is never 0).
    const FRange Apart = Refine(FRange::Unknown(), ECompare::Greater, FRange::Exactly(0), false);
    TestFalse(TEXT("A > B on floats rules 0 out of A - B"), Meet(FRange::Unknown(true), Apart).CanBeZero());
    TestTrue(TEXT("A >= B leaves 0 in A - B"), Meet(FRange::Unknown(true), Refine(FRange::Unknown(), ECompare::GreaterEqual, FRange::Exactly(0), false)).CanBeZero());
    TestFalse(TEXT("A != B rules 0 out of A - B"), Refine(FRange::Unknown(), ECompare::NotEqual, FRange::Exactly(0), false).CanBeZero());
    TestTrue(TEXT("two facts that cannot both hold leave no value"), Meet(FRange::Between(0, 1), FRange::Between(2, 3)).bNone);

    Is(TEXT("widening a growing bound"), Widen(FRange::Between(0, 1), FRange::Between(0, 2)), 0, Unbounded);
    Is(TEXT("widening a steady range"), Widen(FRange::Between(0, 1), FRange::Between(0, 1)), 0, 1);
    TestFalse(TEXT("[1, 5] joined with [-5, -1] still rules out 0"), Join(FRange::Between(1, 5), FRange::Between(-5, -1)).CanBeZero());
    TestTrue(TEXT("Unbounded saturates"), Add(FRange::Between(0, Unbounded), FRange::Exactly(1)).Hi == Unbounded);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
