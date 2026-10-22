#include "Misc/AutomationTest.h"
#include "Domains/PCG/Parameters/McpPCGParameters.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Engine/EngineTypes.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG_PARAMETERS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGBoolValidationTest, "EngineWire.PCG.BooleanValuesAreNotSilentlyCoerced",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGBoolValidationTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    FInstancedPropertyBag Source, Staged;
    Source.AddProperty(TEXT("Enabled"), EPropertyBagPropertyType::Bool);
    Source.AddContainerProperty(TEXT("Flags"), EPropertyBagContainerType::Array, EPropertyBagPropertyType::Bool);
    Source.SetValueBool(TEXT("Enabled"), true);
    const FProperty* Property = Source.FindPropertyDescByName(TEXT("Enabled"))->CachedProperty;
    const FString Before = McpPropertyReflection::FingerprintReflectedValue(Property, const_cast<uint8*>(Source.GetValue().GetMemory()));
    FString Error;
    for (const TCHAR* Invalid : {TEXT("invalid-bool"), TEXT(""), TEXT("false suffix"), TEXT("1")})
    {
        TestFalse(TEXT("Unknown boolean strings are refused"), StagePCGParameterValue(Source, TEXT("Enabled"),
            MakeShared<FJsonValueString>(Invalid), Staged, Error));
    }
    TestFalse(TEXT("Arbitrary numbers are not boolean values"), StagePCGParameterValue(Source, TEXT("Enabled"),
        MakeShared<FJsonValueNumber>(2), Staged, Error));
    const TArray<TSharedPtr<FJsonValue>> InvalidArray = {MakeShared<FJsonValueBoolean>(false), MakeShared<FJsonValueString>(TEXT("invalid-bool"))};
    TestFalse(TEXT("Array elements use the same strict validation"), StagePCGParameterValue(Source, TEXT("Flags"),
        MakeShared<FJsonValueArray>(InvalidArray), Staged, Error));
    TestEqual(TEXT("Refused values preserve the source"),
        McpPropertyReflection::FingerprintReflectedValue(Property, const_cast<uint8*>(Source.GetValue().GetMemory())), Before);
    TestTrue(TEXT("Existing true/false text remains supported"), StagePCGParameterValue(Source, TEXT("Enabled"),
        MakeShared<FJsonValueString>(TEXT("FALSE")), Staged, Error));
    TestTrue(TEXT("Numeric one remains supported"), StagePCGParameterValue(Source, TEXT("Enabled"),
        MakeShared<FJsonValueNumber>(1), Staged, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGEnumCommitTest, "EngineWire.PCG.NativeEnumScalarCommitsActualValue",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGEnumCommitTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    FPropertyBagPropertyDesc Desc(FName(TEXT("Collision")), EPropertyBagContainerType::None,
        EPropertyBagPropertyType::Enum, StaticEnum<ECollisionChannel>());
    Desc.ID = FGuid::NewGuid();
    TestTrue(TEXT("Native enum parameter added"), Graph->AddUserParameters({Desc}) == EPropertyBagAlterationResult::Success);
    const FInstancedPropertyBag* Source = Graph->GetUserParametersStruct();
    if (!TestNotNull(TEXT("Source parameter bag"), Source)) return false;
    FInstancedPropertyBag Staged;
    FString Error;
    TestTrue(TEXT("Enum value staged"), StagePCGParameterValue(*Source, TEXT("Collision"),
        MakeShared<FJsonValueString>(TEXT("ECC_WorldDynamic")), Staged, Error));
    TestTrue(TEXT("Both byte-backed and FEnumProperty enum representations commit"),
        CommitPCGParameterValue(Graph.Get(), TEXT("Collision"), Staged, Error));
    const FPropertyBagPropertyDesc* Current = Graph->GetUserParametersStruct()->FindPropertyDescByName(TEXT("Collision"));
    if (!TestNotNull(TEXT("Committed descriptor"), Current)) return false;
    const auto Value = McpPropertyReflection::ExportPropertyToJsonValueOrText(
        const_cast<uint8*>(Graph->GetUserParametersStruct()->GetValue().GetMemory()), const_cast<FProperty*>(Current->CachedProperty));
    TestEqual(TEXT("Non-default native enum value was actually written"), Value->AsString(), FString(TEXT("ECC_WorldDynamic")));
    return true;
}
#endif
