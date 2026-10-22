#include "Foundation/McpCapabilityAuthorization.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Tests/McpTestFixtures.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpSubobjectPathConfinementTest,
    "EngineWire.Foundation.CapabilityAuthorization.SubobjectPaths",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpSubobjectPathConfinementTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace McpCapabilityAuthorization;
    const FString Component = TEXT("/Game/TeamA/Maps/Test.Test:PersistentLevel.Actor_0.PCGComponent_0");
    const FMcpCapabilityPrincipal Principal = McpTestPrincipal(TEXT("scoped:subobject"),
        {EMcpCapabilityScope::Write}, {TEXT("/Game/TeamA")});
    FMcpCapabilityDemand Demand;
    Demand.RequiredScope = EMcpCapabilityScope::Write;
    Demand.bDeclaresPathParameter = true;
    auto Allows = [&](const FString& Path)
    {
        const auto Scan = CollectPayloadPaths(McpTestFields({{TEXT("componentPath"), Path}}), true);
        return CheckPaths(Principal, Scan.Paths).bAllowed && CheckPathCoverage(Principal, Demand, Scan).bAllowed;
    };

    TestTrue(TEXT("an exact in-prefix component is admitted"), Allows(Component));
    TestTrue(TEXT("the owning package can be granted exactly"),
        CheckPaths(McpTestPrincipal(TEXT("scoped:package"), {EMcpCapabilityScope::Read},
            {TEXT("/Game/TeamA/Maps/Test")}), {Component}).bAllowed);
    TestTrue(TEXT("ordinary exact object-asset prefixes retain their behavior"),
        CheckPaths(McpTestPrincipal(TEXT("scoped:asset"), {EMcpCapabilityScope::Read},
            {TEXT("/Game/TeamA/Graph.Graph")}), {TEXT("/Game/TeamA/Graph.Graph")}).bAllowed);
    TestTrue(TEXT("a Content alias resolves into the same confined package"),
        Allows(TEXT("/Content/TeamA/Maps/Test.Test:PersistentLevel.Actor_0.PCGComponent_0")));
    TestTrue(TEXT("a backslash alias resolves into the same confined package"),
        Allows(TEXT("\\Content\\TeamA\\Maps\\Test.Test:PersistentLevel.Actor_0.PCGComponent_0")));
    TestFalse(TEXT("a sibling prefix never matches"),
        Allows(TEXT("/Game/TeamAOther/Maps/Test.Test:PersistentLevel.Actor_0.PCGComponent_0")));
    TestFalse(TEXT("a valid outside object path stays confined"),
        Allows(TEXT("/Game/TeamB/Test.Test:PersistentLevel.Actor_0.PCGComponent_0")));
    TestFalse(TEXT("a shape-valid but outside mount is not implicitly granted"),
        Allows(TEXT("/UnknownMount/Test.Test:PersistentLevel.Actor_0.PCGComponent_0")));

    const TCHAR* const Malformed[] = {
        TEXT("/Game/TeamA/../TeamB/Test.Test:PersistentLevel.Actor.Component"),
        TEXT("/Game/TeamA/Test.Test:PersistentLevel.Actor..Component"),
        TEXT("/Game/TeamA/Test.Test:PersistentLevel.Actor:Component"),
        TEXT("/Game/TeamA/Test.Test:"), TEXT("/Game/TeamA/Test.:Actor.Component"),
        TEXT("/Game/TeamA/Test:PersistentLevel.Actor.Component"),
        TEXT("/Game/TeamA/Test.Test:PersistentLevel.Actor.Component/Other"),
        TEXT("/Game/TeamA/Test.Test:PersistentLevel.Actor.Component?Other"),
        TEXT("/Game/TeamA/Test.Test:PersistentLevel.Actor.Component\n"),
        TEXT("//Game/TeamA/Test.Test:PersistentLevel.Actor.Component"),
        TEXT("/Game//TeamA/Test.Test:PersistentLevel.Actor.Component"),
        TEXT("C:/TeamA/Test.Test:PersistentLevel.Actor.Component"),
        TEXT("\\\\host\\share\\Test.Test:PersistentLevel.Actor.Component"),
        TEXT("/home/Test.Test:PersistentLevel.Actor.Component")
    };
    for (const TCHAR* Path : Malformed)
    {
        TestFalse(FString::Printf(TEXT("malformed object path refused: %s"), Path), Allows(Path));
    }
    FString EmbeddedNul = Component;
    EmbeddedNul.GetCharArray().Insert(TEXT('\0'), EmbeddedNul.Len());
    EmbeddedNul.Append(TEXT("Hidden"));
    TestTrue(TEXT("the fixture contains a real embedded NUL"), EmbeddedNul.Len() > FCString::Strlen(*EmbeddedNul));
    TestFalse(TEXT("embedded NUL cannot hide an unvalidated suffix"), Allows(EmbeddedNul));
    TestFalse(TEXT("the canonical predicate does not normalize a raw subobject alias"),
        CheckPaths(Principal, {TEXT("/Content/TeamA/Test.Test:PersistentLevel.Actor.Component")}).bAllowed);

    const auto Nested = MakeShared<FJsonObject>();
    Nested->SetArrayField(TEXT("unusualKey"), {MakeShared<FJsonValueString>(Component),
        MakeShared<FJsonValueString>(TEXT("/Game/TeamB/Test.Test:PersistentLevel.Actor.Component"))});
    const auto Payload = MakeShared<FJsonObject>();
    Payload->SetObjectField(TEXT("settings"), Nested);
    const auto Scan = CollectPayloadPaths(Payload, true);
    TestEqual(TEXT("nested values retain both object paths"), Scan.Paths.Num(), 2);
    TestFalse(TEXT("an allowed target does not mask a nested outside object"), CheckPaths(Principal, Scan.Paths).bAllowed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpSubobjectPathScanBoundsTest,
    "EngineWire.Foundation.CapabilityAuthorization.SubobjectScanBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpSubobjectPathScanBoundsTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace McpCapabilityAuthorization;
    const auto Principal = McpTestPrincipal(TEXT("scoped:subobject"),
        {EMcpCapabilityScope::Write}, {TEXT("/Game/TeamA")});
    FMcpCapabilityDemand Demand;
    Demand.RequiredScope = EMcpCapabilityScope::Write;
    Demand.bDeclaresPathParameter = true;
    const FString Path = TEXT("/Game/TeamA/Test.Test:PersistentLevel.Actor.Component");
    auto Payload = McpTestFields({{TEXT("componentPath"), Path}});
    const auto Shallow = CollectPayloadPaths(Payload, true);
    TestFalse(TEXT("positive control scan is complete"), Shallow.bTruncated);
    TestTrue(TEXT("positive control has a provable component target"),
        CheckPathCoverage(Principal, Demand, Shallow).bAllowed);
    for (int32 Index = 0; Index < 32; ++Index)
    {
        auto Wrapper = MakeShared<FJsonObject>();
        Wrapper->SetObjectField(TEXT("nested"), Payload);
        Payload = Wrapper;
    }
    const auto Deep = CollectPayloadPaths(Payload, true);
    TestTrue(TEXT("an over-limit object scan reports truncation"), Deep.bTruncated);
    TestFalse(TEXT("an over-limit object scan fails closed"), CheckPathCoverage(Principal, Demand, Deep).bAllowed);
    Payload = McpTestFields({{TEXT("componentPath"), Path}});
    TArray<TSharedPtr<FJsonValue>> Values;
    for (int32 Index = 0; Index < 4200; ++Index) Values.Add(MakeShared<FJsonValueString>(Path));
    Payload->SetArrayField(TEXT("nestedObjects"), Values);
    const auto Wide = CollectPayloadPaths(Payload, true);
    TestTrue(TEXT("a node-exhausted object scan reports truncation"), Wide.bTruncated);
    TestFalse(TEXT("a node-exhausted object scan fails closed"), CheckPathCoverage(Principal, Demand, Wide).bAllowed);
    return true;
}
#endif
