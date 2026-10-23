#include "Foundation/Reflection/McpPropertyReflectionPrivate.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpFloatingPointImportTest,
    "EngineWire.Reflection.FloatingPointImport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpFloatingPointImportTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TUniquePtr<FFloatProperty> FloatProperty = MakeUnique<FFloatProperty>(
        GetTransientPackage(), TEXT("McpFloat"), RF_Transient);
    TUniquePtr<FDoubleProperty> DoubleProperty = MakeUnique<FDoubleProperty>(
        GetTransientPackage(), TEXT("McpDouble"), RF_Transient);
    float FloatValue = 42.0f;
    double DoubleValue = 42.0;
    FString Error;
    auto Apply = [&Error](void* Storage, FProperty* Property, const TSharedPtr<FJsonValue>& Value)
    {
        Error.Empty();
        return McpPropertyReflection::ApplyJsonValueToProperty(Storage, Property, Value, Error);
    };
    auto Reject = [&](const TSharedPtr<FJsonValue>& Value)
    {
        FloatValue = 42.0f;
        DoubleValue = 42.0;
        TestFalse(TEXT("Invalid float input is refused"), Apply(&FloatValue, FloatProperty.Get(), Value));
        TestFalse(TEXT("Float refusal explains the failure"), Error.IsEmpty());
        TestEqual(TEXT("Refused float leaves storage unchanged"), FloatValue, 42.0f);
        TestFalse(TEXT("Invalid double input is refused"), Apply(&DoubleValue, DoubleProperty.Get(), Value));
        TestFalse(TEXT("Double refusal explains the failure"), Error.IsEmpty());
        TestEqual(TEXT("Refused double leaves storage unchanged"), DoubleValue, 42.0);
    };
    for (const TCHAR* Invalid : {TEXT("garbage"), TEXT("12junk"), TEXT(""), TEXT(" "),
             TEXT("NaN"), TEXT("inf"), TEXT("-inf"), TEXT("1e999")})
    {
        Reject(MakeShared<FJsonValueString>(Invalid));
    }
    FString EmbeddedNull(TEXT("12"));
    EmbeddedNull.GetCharArray().Insert(TEXT('\0'), EmbeddedNull.Len());
    EmbeddedNull.Append(TEXT("34"));
    Reject(MakeShared<FJsonValueString>(EmbeddedNull));
    Reject(MakeShared<FJsonValueNumber>(std::numeric_limits<double>::infinity()));
    Reject(MakeShared<FJsonValueNumber>(std::numeric_limits<double>::quiet_NaN()));
    Reject(MakeShared<FJsonValueBoolean>(true));
    Reject(MakeShared<FJsonValueNull>());

    for (const TSharedPtr<FJsonValue>& Value : TArray<TSharedPtr<FJsonValue>>{
             MakeShared<FJsonValueNumber>(1e100), MakeShared<FJsonValueString>(TEXT("-1e100"))})
    {
        FloatValue = 42.0f;
        TestFalse(TEXT("Finite double outside float range is refused"), Apply(&FloatValue, FloatProperty.Get(), Value));
        TestFalse(TEXT("Float range refusal explains the failure"), Error.IsEmpty());
        TestEqual(TEXT("Float range refusal leaves storage unchanged"), FloatValue, 42.0f);
        TestTrue(TEXT("Finite value remains valid for double"), Apply(&DoubleValue, DoubleProperty.Get(), Value));
        TestEqual(TEXT("Double retains the full value"), DoubleValue, Value->Type == EJson::Number ? 1e100 : -1e100);
    }
    struct FValidCase { const TCHAR* Text; double Expected; };
    for (const FValidCase& Case : {FValidCase{TEXT("0"), 0.0}, FValidCase{TEXT("-12.5"), -12.5},
             FValidCase{TEXT(" 2.5e2 "), 250.0}, FValidCase{TEXT("+1.25"), 1.25}})
    {
        const auto Value = MakeShared<FJsonValueString>(Case.Text);
        TestTrue(TEXT("Valid float string remains accepted"), Apply(&FloatValue, FloatProperty.Get(), Value));
        TestEqual(TEXT("Float string stores its parsed value"), FloatValue, static_cast<float>(Case.Expected));
        TestTrue(TEXT("Valid double string remains accepted"), Apply(&DoubleValue, DoubleProperty.Get(), Value));
        TestEqual(TEXT("Double string stores its parsed value"), DoubleValue, Case.Expected);
    }
    const auto Number = MakeShared<FJsonValueNumber>(3.5);
    TestTrue(TEXT("Float JSON number remains accepted"), Apply(&FloatValue, FloatProperty.Get(), Number));
    TestEqual(TEXT("Float JSON number stores its value"), FloatValue, 3.5f);
    TestTrue(TEXT("Double JSON number remains accepted"), Apply(&DoubleValue, DoubleProperty.Get(), Number));
    TestEqual(TEXT("Double JSON number stores its value"), DoubleValue, 3.5);
    return true;
}
#endif
