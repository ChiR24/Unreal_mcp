// McpConsentWindowsTests.cpp — the window ledger (McpConsentWindows.h): its folder, what it covers, its uses, its
// pending state and revoking. It closes only the windows it opened.

#include "Core/Security/McpConsentWindows.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Foundation/McpCapabilityAuthorization.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpConsentWindowsTest,
    "EngineWire.Core.Security.ConsentWindows",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpConsentWindowsTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TestEqual(TEXT("the folder of one asset"), FMcpConsentWindows::FolderOf({TEXT("/Game/Sandbox/Props/SM_A")}), FString(TEXT("/Game/Sandbox/Props/")));
    TestEqual(TEXT("the folder two share"), FMcpConsentWindows::FolderOf({TEXT("/Game/Sandbox/Props/SM_A"), TEXT("/Game/Sandbox/Lights/L_B")}), FString(TEXT("/Game/Sandbox/")));
    TestEqual(TEXT("never a content root"), FMcpConsentWindows::FolderOf({TEXT("/Game/A"), TEXT("/Game/Sandbox/B")}), FString());
    TestEqual(TEXT("none without paths"), FMcpConsentWindows::FolderOf({}), FString());
    TestEqual(TEXT("none for a traversal"), FMcpConsentWindows::FolderOf({TEXT("/Game/Sandbox/../Other/A")}), FString());

    FMcpConsentWindows& Windows = FMcpConsentWindows::Get();
    const FString Who = TEXT("consent-window-test");
    FMcpCapabilityDemand Delete;
    Delete.CapabilityId = TEXT("manage_asset.delete");
    Delete.ConsentMode = TEXT("explicit");
    FMcpCapabilityDemand Duplicate = Delete;
    Duplicate.CapabilityId = TEXT("manage_asset.duplicate");
    const FString Id = Windows.Open(Who, Delete.CapabilityId, TEXT("/Game/Sandbox/"), 2, 5, true);
    TestTrue(TEXT("a pending window covers nothing"), Windows.Take(Who, Delete, {TEXT("/Game/Sandbox/A")}).Id.IsEmpty());
    Windows.Settle(Id, true);
    TestTrue(TEXT("not a traversal out of its folder"), Windows.Take(Who, Delete, {TEXT("/Game/Sandbox/../Other/A")}).Id.IsEmpty());
    TestTrue(TEXT("not a relative path"), Windows.Take(Who, Delete, {TEXT("Sandbox/A")}).Id.IsEmpty());
    const FMcpConsentWindowUse First = Windows.Take(Who, Delete, {TEXT("/Game/Sandbox/A")});
    TestEqual(TEXT("allowed, it covers a call under its folder"), First.Id, Id);
    TestEqual(TEXT("with one use left"), First.UsesLeft, 1);
    TestTrue(TEXT("not outside its folder"), Windows.Take(Who, Delete, {TEXT("/Game/Sandbox/A"), TEXT("/Game/Other/B")}).Id.IsEmpty());
    TestTrue(TEXT("not a sibling folder sharing its name's start"), Windows.Take(Who, Delete, {TEXT("/Game/SandboxOld/A")}).Id.IsEmpty());
    TestTrue(TEXT("not another capability"), Windows.Take(Who, Duplicate, {TEXT("/Game/Sandbox/A")}).Id.IsEmpty());
    TestTrue(TEXT("not another caller"), Windows.Take(TEXT("someone-else"), Delete, {TEXT("/Game/Sandbox/A")}).Id.IsEmpty());
    TestTrue(TEXT("not a call naming no path"), Windows.Take(Who, Delete, {}).Id.IsEmpty());
    TestEqual(TEXT("its last use"), Windows.Take(Who, Delete, {TEXT("/Game/Sandbox/B")}).Id, Id);
    TestTrue(TEXT("then it is gone"), Windows.Take(Who, Delete, {TEXT("/Game/Sandbox/C")}).Id.IsEmpty());

    const auto Has = [&Windows](const FString& WindowId)
    {
        return Windows.List().ContainsByPredicate([&WindowId](const FMcpConsentWindow& Window) { return Window.Id == WindowId; });
    };
    const FString Refused = Windows.Open(Who, Delete.CapabilityId, TEXT("/Game/Sandbox/"), 5, 5, true);
    Windows.Settle(Refused, false);
    TestFalse(TEXT("a window the user refused is gone"), Has(Refused));
    const FString Open = Windows.Open(Who, Delete.CapabilityId, TEXT("/Game/Sandbox/"), 5, 5, false);
    TestTrue(TEXT("listed while open"), Has(Open));
    TestEqual(TEXT("revoked by its id"), Windows.Revoke(Open), 1);
    TestFalse(TEXT("then not listed"), Has(Open));

    FMcpCapabilityDemand Elevated = Delete;
    Elevated.ConsentMode = TEXT("elevated");
    FMcpCapabilityDemand Admin = Delete;
    Admin.RequiredScope = EMcpCapabilityScope::Admin;
    const TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("assetPath"), TEXT("/Game/Sandbox/Props/SM_A"));
    TestTrue(TEXT("never offered for elevated consent"), FMcpConsentWindows::OfferFor(Elevated, Payload).IsEmpty());
    TestTrue(TEXT("never offered for Admin scope"), FMcpConsentWindows::OfferFor(Admin, Payload).IsEmpty());
    FMcpCapabilityPrincipal Principal;
    Principal.Identity = Who;
    TestTrue(TEXT("never taken on the WebSocket door"), Windows.TakeFor(Principal, Delete, Payload, false).Id.IsEmpty());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
