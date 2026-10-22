#include "Misc/AutomationTest.h"
#include "Core/Requests/McpRequestOriginRegistry.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpRequestPrincipalLifetimeTest, "EngineWire.Requests.PrincipalLifetime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpRequestPrincipalLifetimeTest::RunTest(const FString& Parameters)
{
    FMcpRequestOriginRegistry Registry;
    FString Principal = TEXT("stale output");
    Registry.RecordPrincipal(TEXT("request"), TEXT("test-principal-a"));
    TestFalse(TEXT("No principal can be attached without an admitted origin"), Registry.ResolvePrincipal(TEXT("request"), Principal));
    TestTrue(TEXT("A failed lookup clears caller output"), Principal.IsEmpty());
    Registry.Record(TEXT("request"), ERequestOrigin::NativeHTTP);
    Registry.RecordPrincipal(TEXT("request"), TEXT("test-principal-a"));
    TestTrue(TEXT("Admitted principal is retained"), Registry.ResolvePrincipal(TEXT("request"), Principal));
    TestEqual(TEXT("Only the authenticated identity is retained"), Principal, FString(TEXT("test-principal-a")));
    Registry.Record(TEXT("request"), ERequestOrigin::NativeHTTP);
    TestTrue(TEXT("Same-origin queue admission preserves the authenticated principal"), Registry.ResolvePrincipal(TEXT("request"), Principal));
    TestEqual(TEXT("Queue bookkeeping does not replace the principal"), Principal, FString(TEXT("test-principal-a")));
    Registry.Record(TEXT("request"), ERequestOrigin::WebSocket);
    TestFalse(TEXT("Replacement admission cannot inherit a prior principal"), Registry.ResolvePrincipal(TEXT("request"), Principal));
    Registry.RecordPrincipal(TEXT("request"), TEXT("test-principal-b"));
    TestTrue(TEXT("Replacement principal can be bound"), Registry.ResolvePrincipal(TEXT("request"), Principal));
    TestEqual(TEXT("Replacement identity is exact"), Principal, FString(TEXT("test-principal-b")));
    Registry.Forget(TEXT("request"));
    TestFalse(TEXT("Forgetting origin also removes principal"), Registry.ResolvePrincipal(TEXT("request"), Principal));
    Registry.Record(TEXT("reset"), ERequestOrigin::NativeHTTP);
    Registry.RecordPrincipal(TEXT("reset"), TEXT("test-principal-a"));
    Registry.Reset();
    TestFalse(TEXT("Subsystem reset cannot retain principal metadata"), Registry.ResolvePrincipal(TEXT("reset"), Principal));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpRequestPrincipalEvictionTest, "EngineWire.Requests.PrincipalEviction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpRequestPrincipalEvictionTest::RunTest(const FString& Parameters)
{
    FMcpRequestOriginRegistry Registry;
    for (int32 Index = 0; Index <= FMcpRequestOriginRegistry::MaxTrackedRequests; ++Index)
    {
        const FString Id = FString::FromInt(Index);
        Registry.Record(Id, ERequestOrigin::NativeHTTP);
        Registry.RecordPrincipal(Id, TEXT("test-principal"));
    }
    FString Principal;
    TestEqual(TEXT("Origin metadata stays bounded"), Registry.Num(), FMcpRequestOriginRegistry::MaxTrackedRequests);
    TestFalse(TEXT("Eviction removes matching principal metadata"), Registry.ResolvePrincipal(TEXT("0"), Principal));
    TestTrue(TEXT("Newest admission survives eviction"), Registry.ResolvePrincipal(FString::FromInt(FMcpRequestOriginRegistry::MaxTrackedRequests), Principal));
    return true;
}
#endif
