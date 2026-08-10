#include "Foundation/McpCapabilityAuthorization.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
FMcpCapabilityPrincipal MakePrincipal(const TArray<EMcpCapabilityScope>& Scopes)
{
	FMcpCapabilityPrincipal Principal;
	Principal.Identity = TEXT("scoped:test");
	Principal.Scopes = Scopes;
	Principal.bAuthenticated = true;
	return Principal;
}

FMcpCapabilityDemand MakeDemand(EMcpCapabilityScope Scope, const TCHAR* Consent)
{
	FMcpCapabilityDemand Demand;
	Demand.RequiredScope = Scope;
	Demand.ConsentMode = Consent;
	Demand.CapabilityId = TEXT("control_actor.delete");
	return Demand;
}

TSharedPtr<FJsonObject> ParseJson(const FString& Text)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpCapabilityAuthorizationScopeConsentTest,
	"McpAutomationBridge.Foundation.CapabilityAuthorization.ScopeAndConsent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpCapabilityAuthorizationScopeConsentTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpCapabilityAuthorization;

	const FMcpCapabilityPrincipal Writer = MakePrincipal({ EMcpCapabilityScope::Write });
	const FMcpCapabilityPrincipal Admin = MakePrincipal({ EMcpCapabilityScope::Admin });
	const FMcpCapabilityPrincipal Nothing = MakePrincipal({});

	const FMcpCapabilityDemand NeedsWrite = MakeDemand(EMcpCapabilityScope::Write, TEXT("none"));
	const FMcpCapabilityDemand NeedsDestructive =
		MakeDemand(EMcpCapabilityScope::Destructive, TEXT("none"));

	TestTrue(TEXT("write principal satisfies write"), CheckScope(Writer, NeedsWrite).bAllowed);
	TestFalse(TEXT("write does NOT imply destructive"), CheckScope(Writer, NeedsDestructive).bAllowed);
	TestFalse(TEXT("write does NOT imply read"),
		CheckScope(Writer, MakeDemand(EMcpCapabilityScope::Read, TEXT("none"))).bAllowed);
	TestTrue(TEXT("admin is a wildcard"), CheckScope(Admin, NeedsDestructive).bAllowed);
	TestFalse(TEXT("empty scope set authorizes nothing"), CheckScope(Nothing, NeedsWrite).bAllowed);

	const FMcpAuthorizationDecision Denied = CheckScope(Writer, NeedsDestructive);
	TestEqual(TEXT("typed code"), Denied.ErrorCode, FString(TEXT("SCOPE_NOT_GRANTED")));
	TestEqual(TEXT("requiredScope carried"), Denied.RequiredScope, FString(TEXT("destructive")));
	TestEqual(TEXT("grantedScopes carried"), Denied.GrantedScopes.Num(), 1);
	TestFalse(TEXT("refusal never carries a token"), Denied.Message.Contains(TEXT("secret")));

	// Consent is never inferred: only a grant naming THIS capability counts.
	FMcpAuthorizationGrant None;
	FMcpAuthorizationGrant Explicit;
	Explicit.bConsentPresent = true;
	Explicit.ConsentCapability = TEXT("control_actor.delete");
	Explicit.ConsentAcknowledge = TEXT("explicit");
	FMcpAuthorizationGrant Elevated = Explicit;
	Elevated.ConsentAcknowledge = TEXT("elevated");
	FMcpAuthorizationGrant WrongCapability = Explicit;
	WrongCapability.ConsentCapability = TEXT("manage_asset.delete");

	const FMcpCapabilityDemand ConsentNone = MakeDemand(EMcpCapabilityScope::Write, TEXT("none"));
	const FMcpCapabilityDemand ConsentExplicit =
		MakeDemand(EMcpCapabilityScope::Write, TEXT("explicit"));
	const FMcpCapabilityDemand ConsentElevated =
		MakeDemand(EMcpCapabilityScope::Write, TEXT("elevated"));

	TestTrue(TEXT("none needs no grant"), CheckConsent(ConsentNone, None).bAllowed);
	TestFalse(TEXT("explicit needs a grant"), CheckConsent(ConsentExplicit, None).bAllowed);
	TestTrue(TEXT("explicit accepts explicit"), CheckConsent(ConsentExplicit, Explicit).bAllowed);
	TestTrue(TEXT("explicit accepts elevated"), CheckConsent(ConsentExplicit, Elevated).bAllowed);
	TestFalse(TEXT("elevated rejects explicit"), CheckConsent(ConsentElevated, Explicit).bAllowed);
	TestTrue(TEXT("elevated accepts elevated"), CheckConsent(ConsentElevated, Elevated).bAllowed);
	TestFalse(TEXT("consent for another capability does not transfer"),
		CheckConsent(ConsentExplicit, WrongCapability).bAllowed);
	TestEqual(TEXT("consent typed code"),
		CheckConsent(ConsentExplicit, None).ErrorCode, FString(TEXT("CONSENT_REQUIRED")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpCapabilityAuthorizationPathProjectTest,
	"McpAutomationBridge.Foundation.CapabilityAuthorization.PathAndProject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpCapabilityAuthorizationPathProjectTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpCapabilityAuthorization;

	// Boundary-aware containment: a sibling folder sharing a name prefix is out.
	TestTrue(TEXT("exact prefix"), IsPathWithinPrefix(TEXT("/Game/Team"), TEXT("/Game/Team")));
	TestTrue(TEXT("child"), IsPathWithinPrefix(TEXT("/Game/Team/Sub/A"), TEXT("/Game/Team")));
	TestFalse(TEXT("sibling sharing a name prefix is refused"),
		IsPathWithinPrefix(TEXT("/Game/TeamOther/A"), TEXT("/Game/Team")));
	TestFalse(TEXT("parent is not inside child"),
		IsPathWithinPrefix(TEXT("/Game"), TEXT("/Game/Team")));
	TestTrue(TEXT("trailing slash is insignificant"),
		IsPathWithinPrefix(TEXT("/Game/Team/"), TEXT("/Game/Team")));

	FMcpCapabilityPrincipal Restricted = MakePrincipal({ EMcpCapabilityScope::Write });
	Restricted.AllowedPathPrefixes = { TEXT("/Game/Team") };
	const FMcpCapabilityPrincipal Unrestricted = MakePrincipal({ EMcpCapabilityScope::Write });

	TestTrue(TEXT("unrestricted principal passes any path"),
		CheckPaths(Unrestricted, { TEXT("/Game/Anything") }).bAllowed);
	TestTrue(TEXT("a request with no path passes"), CheckPaths(Restricted, {}).bAllowed);
	TestTrue(TEXT("in-prefix path passes"),
		CheckPaths(Restricted, { TEXT("/Game/Team/Hero") }).bAllowed);
	TestFalse(TEXT("out-of-prefix path is refused"),
		CheckPaths(Restricted, { TEXT("/Game/Other/Hero") }).bAllowed);
	TestFalse(TEXT("EVERY path must be permitted, not just one"),
		CheckPaths(Restricted, { TEXT("/Game/Team/Ok"), TEXT("/Game/Other/Bad") }).bAllowed);
	TestEqual(TEXT("path typed code"),
		CheckPaths(Restricted, { TEXT("/Game/Other") }).ErrorCode,
		FString(TEXT("PATH_NOT_PERMITTED")));

	FMcpCapabilityPrincipal ProjectBound = MakePrincipal({ EMcpCapabilityScope::Write });
	ProjectBound.AllowedProjects = { TEXT("MCPtest") };
	TestTrue(TEXT("allowed project passes"), CheckProject(ProjectBound, TEXT("MCPtest")).bAllowed);
	TestFalse(TEXT("other project refused"), CheckProject(ProjectBound, TEXT("OtherGame")).bAllowed);
	TestTrue(TEXT("unrestricted project passes"),
		CheckProject(Unrestricted, TEXT("Anything")).bAllowed);
	TestEqual(TEXT("project typed code"),
		CheckProject(ProjectBound, TEXT("OtherGame")).ErrorCode,
		FString(TEXT("PROJECT_NOT_PERMITTED")));

	// Paths are found by scanning values, so an unusual key cannot smuggle one past.
	const TSharedPtr<FJsonObject> Payload = ParseJson(TEXT(
		"{\"objectPath\":\"/Game/Team/A\",\"nested\":{\"weirdKey\":\"/Game/Other/B\"},"
		"\"list\":[\"/Game/Third/C\"],\"name\":\"NotAPath\",\"count\":3}"));
	const TArray<FString> Found = CollectPayloadPaths(Payload).Paths;
	TestEqual(TEXT("all three UE paths found regardless of key name"), Found.Num(), 3);
	TestFalse(TEXT("non-path strings ignored"), Found.Contains(TEXT("NotAPath")));
	TestFalse(TEXT("a restricted principal is refused via a nested unusual key"),
		CheckPaths(Restricted, Found).bAllowed);

	return true;
}
#endif
