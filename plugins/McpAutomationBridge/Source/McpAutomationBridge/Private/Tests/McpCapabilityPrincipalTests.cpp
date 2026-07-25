#include "Foundation/McpCapabilityPrincipal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "McpAutomationBridgeSettings.h"
#include "McpCapabilityScopes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpCapabilityPrincipalResolveTest,
	"McpAutomationBridge.Foundation.CapabilityPrincipal.Resolve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpCapabilityPrincipalResolveTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UMcpAutomationBridgeSettings* Settings = NewObject<UMcpAutomationBridgeSettings>();
	// This is a config=Game class, so a fresh instance inherits whatever the
	// DEPLOYED DefaultGame.ini holds. Clear the fields under test first: an
	// inherited entry whose profile collides with one below would otherwise be
	// dropped as a duplicate and silently change what this test proves.
	Settings->ScopedCapabilityTokens.Empty();
	Settings->CapabilityToken = TEXT("legacy-secret");

	FMcpScopedCapabilityToken Reader;
	Reader.Profile = TEXT("Reader");
	Reader.Token = TEXT("reader-secret");
	Reader.Scopes = { EMcpCapabilityScope::Read };
	Settings->ScopedCapabilityTokens.Add(Reader);

	FMcpScopedCapabilityToken BadAdmin;
	BadAdmin.Profile = TEXT("BadAdmin");
	BadAdmin.Token = TEXT("badadmin-secret");
	BadAdmin.Scopes = { EMcpCapabilityScope::Admin };
	Settings->ScopedCapabilityTokens.Add(BadAdmin);

	{
		FMcpPrincipalResolveRequest Request;
		Request.PresentedToken = TEXT("legacy-secret");
		Request.bRequireToken = true;
		const FMcpCapabilityPrincipal Principal = McpCapabilityPrincipal::Resolve(Request, *Settings);
		TestTrue(TEXT("legacy identity"), Principal.Identity == TEXT("legacy"));
		TestTrue(TEXT("legacy authenticated"), Principal.bAuthenticated);
		TestTrue(TEXT("legacy is admin (destructive allowed)"), Principal.IsScopeAuthorized(EMcpCapabilityScope::Destructive));
		TestTrue(TEXT("legacy is explicitly deprecated"), Principal.bDeprecated);
	}

	{
		FMcpPrincipalResolveRequest Request;
		Request.PresentedToken = TEXT("reader-secret");
		Request.bRequireToken = true;
		const FMcpCapabilityPrincipal Principal = McpCapabilityPrincipal::Resolve(Request, *Settings);
		TestTrue(TEXT("scoped identity normalized"), Principal.Identity == TEXT("scoped:reader"));
		TestTrue(TEXT("reader has read"), Principal.IsScopeAuthorized(EMcpCapabilityScope::Read));
		TestFalse(TEXT("read does not imply write"), Principal.IsScopeAuthorized(EMcpCapabilityScope::Write));
		TestFalse(TEXT("read does not imply destructive"), Principal.IsScopeAuthorized(EMcpCapabilityScope::Destructive));
		TestFalse(TEXT("scoped never gets admin"), Principal.IsScopeAuthorized(EMcpCapabilityScope::Admin));
		TestFalse(TEXT("scoped is not deprecated"), Principal.bDeprecated);
	}

	{
		FMcpPrincipalResolveRequest Request;
		Request.PresentedToken = TEXT("badadmin-secret");
		Request.bRequireToken = true;
		const FMcpCapabilityPrincipal Principal = McpCapabilityPrincipal::Resolve(Request, *Settings);
		TestFalse(TEXT("scoped-admin entry is ignored (token unauthenticated)"), Principal.bAuthenticated);
	}

	{
		FMcpPrincipalResolveRequest Request;
		Request.PresentedToken = TEXT("does-not-match");
		Request.bRequireToken = true;
		const FMcpCapabilityPrincipal Principal = McpCapabilityPrincipal::Resolve(Request, *Settings);
		TestFalse(TEXT("wrong token is unauthenticated"), Principal.bAuthenticated);
	}

	{
		FMcpPrincipalResolveRequest Request;
		Request.bIsLoopback = true;
		Request.bRequireToken = false;
		const FMcpCapabilityPrincipal Principal = McpCapabilityPrincipal::Resolve(Request, *Settings);
		TestTrue(TEXT("no-token loopback binds loopback identity"), Principal.Identity == TEXT("loopback"));
		TestTrue(TEXT("no-token loopback is admin"), Principal.IsScopeAuthorized(EMcpCapabilityScope::Admin));
	}

	return true;
}
#endif
