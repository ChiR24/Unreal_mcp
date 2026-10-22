#include "Misc/AutomationTest.h"
#include "Domains/PCG/Parameters/McpPCGParameters.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG_PARAMETERS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGParameterNumberTest, "EngineWire.PCG.ParameterNumbersRejectInvalidValues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGParameterNumberTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    FInstancedPropertyBag Source;
    Source.AddProperty(TEXT("Spacing"), EPropertyBagPropertyType::Double);
    Source.AddProperty(TEXT("Scale"), EPropertyBagPropertyType::Float);
    Source.AddContainerProperty(TEXT("Weights"), EPropertyBagContainerType::Array, EPropertyBagPropertyType::Double);
    Source.SetValueDouble(TEXT("Spacing"), 200.0);
    Source.SetValueFloat(TEXT("Scale"), 2.0f);
    const auto* Desc = Source.FindPropertyDescByName(TEXT("Spacing"));
    if (!TestNotNull(TEXT("Double parameter descriptor exists"), Desc)) return false;
    const FString Before = McpPropertyReflection::FingerprintReflectedValue(Desc->CachedProperty,
        const_cast<uint8*>(Source.GetValue().GetMemory()));
    FString Error;
    FInstancedPropertyBag Staged;
    for (const TCHAR* Invalid : {TEXT("invalid-double"), TEXT("12junk"), TEXT(""), TEXT("NaN"), TEXT("inf"), TEXT("1e999")})
    {
        TestFalse(TEXT("Malformed or nonfinite numeric string is refused"), StagePCGParameterValue(
            Source, TEXT("Spacing"), MakeShared<FJsonValueString>(Invalid), Staged, Error));
        TestFalse(TEXT("Refusal includes an explanation"), Error.IsEmpty());
    }
    FString EmbeddedNull(TEXT("12"));
    EmbeddedNull.GetCharArray().Insert(TEXT('\0'), EmbeddedNull.Len());
    EmbeddedNull.Append(TEXT("34"));
    TestFalse(TEXT("Embedded NUL is not accepted as a numeric prefix"), StagePCGParameterValue(
        Source, TEXT("Spacing"), MakeShared<FJsonValueString>(EmbeddedNull), Staged, Error));
    TestFalse(TEXT("Nonfinite JSON number is refused"), StagePCGParameterValue(Source, TEXT("Spacing"),
        MakeShared<FJsonValueNumber>(std::numeric_limits<double>::infinity()), Staged, Error));
    TestFalse(TEXT("Finite double outside float range is refused"), StagePCGParameterValue(
        Source, TEXT("Scale"), MakeShared<FJsonValueNumber>(1e100), Staged, Error));
    const TArray<TSharedPtr<FJsonValue>> Values = {MakeShared<FJsonValueNumber>(1.0), MakeShared<FJsonValueString>(TEXT("invalid-double"))};
    TestFalse(TEXT("Malformed array element is refused"), StagePCGParameterValue(Source, TEXT("Weights"),
        MakeShared<FJsonValueArray>(Values), Staged, Error));
    TestEqual(TEXT("Every failed staging operation preserves the source value"),
        McpPropertyReflection::FingerprintReflectedValue(Desc->CachedProperty,
            const_cast<uint8*>(Source.GetValue().GetMemory())), Before);
    TestTrue(TEXT("Valid scientific notation remains accepted"), StagePCGParameterValue(
        Source, TEXT("Spacing"), MakeShared<FJsonValueString>(TEXT(" 2.5e2 ")), Staged, Error));
    const FDoubleProperty* Property = CastField<FDoubleProperty>(Staged.FindPropertyDescByName(TEXT("Spacing"))->CachedProperty);
    if (!TestNotNull(TEXT("Staged double property exists"), Property)) return false;
    TestEqual(TEXT("Valid scientific notation is parsed exactly"), Property->GetPropertyValue_InContainer(Staged.GetValue().GetMemory()), 250.0);
    TestTrue(TEXT("Valid JSON number remains accepted"), StagePCGParameterValue(
        Source, TEXT("Scale"), MakeShared<FJsonValueNumber>(3.5), Staged, Error));
    return true;
}
#endif
