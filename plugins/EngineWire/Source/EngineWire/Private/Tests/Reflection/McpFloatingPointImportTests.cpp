#include "Foundation/Reflection/McpPropertyReflectionPrivate.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include <limits>
#if PLATFORM_WINDOWS
#include "Misc/ScopeExit.h"
#include <clocale>
#endif

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

#if PLATFORM_WINDOWS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpFloatingPointImportLocaleTest,
    "EngineWire.Reflection.FloatingPointImportLocale",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpFloatingPointImportLocaleTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    // Windows can isolate CRT locale changes to this test's thread.
    const int PreviousMode = _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
    if (!TestTrue(TEXT("Thread-local numeric locale is available"), PreviousMode != -1)) return false;
    const FString PreviousLocale(ANSI_TO_TCHAR(std::setlocale(LC_NUMERIC, nullptr)));
    ON_SCOPE_EXIT
    {
        std::setlocale(LC_NUMERIC, TCHAR_TO_ANSI(*PreviousLocale));
        _configthreadlocale(PreviousMode);
    };
    if (!TestNotNull(TEXT("Decimal-comma locale is available"),
            std::setlocale(LC_NUMERIC, "German_Germany.1252"))) return false;
    TestEqual(TEXT("Test locale uses a decimal comma"), FString(ANSI_TO_TCHAR(std::localeconv()->decimal_point)), FString(TEXT(",")));

    TUniquePtr<FFloatProperty> FloatProperty = MakeUnique<FFloatProperty>(
        GetTransientPackage(), TEXT("McpLocaleFloat"), RF_Transient);
    TUniquePtr<FDoubleProperty> DoubleProperty = MakeUnique<FDoubleProperty>(
        GetTransientPackage(), TEXT("McpLocaleDouble"), RF_Transient);
    float FloatValue = 42.0f;
    double DoubleValue = 42.0;
    FString Error;
    auto Apply = [&Error](void* Storage, FProperty* Property, const TCHAR* Text)
    {
        Error.Empty();
        return McpPropertyReflection::ApplyJsonValueToProperty(Storage, Property,
            MakeShared<FJsonValueString>(Text), Error);
    };
    TestTrue(TEXT("Float accepts dot decimal in comma locale"), Apply(&FloatValue, FloatProperty.Get(), TEXT(" 2.5e2 ")));
    TestEqual(TEXT("Float parses dot decimal correctly"), FloatValue, 250.0f);
    TestTrue(TEXT("Double accepts signed dot decimal in comma locale"), Apply(&DoubleValue, DoubleProperty.Get(), TEXT("+1.25")));
    TestEqual(TEXT("Double parses dot decimal correctly"), DoubleValue, 1.25);
    FloatValue = 42.0f;
    DoubleValue = 42.0;
    TestFalse(TEXT("Float rejects comma decimal"), Apply(&FloatValue, FloatProperty.Get(), TEXT("2,5")));
    TestFalse(TEXT("Float refusal explains the failure"), Error.IsEmpty());
    TestEqual(TEXT("Refused float leaves storage unchanged"), FloatValue, 42.0f);
    TestFalse(TEXT("Double rejects comma decimal"), Apply(&DoubleValue, DoubleProperty.Get(), TEXT("2,5")));
    TestFalse(TEXT("Double refusal explains the failure"), Error.IsEmpty());
    TestEqual(TEXT("Refused double leaves storage unchanged"), DoubleValue, 42.0);
    return true;
}
#endif
#endif
