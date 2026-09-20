#include "Core/Compatibility/McpVersionCompatibility.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

#include "Domains/AnimationAuthoring/McpAutomationBridge_AnimationAuthoringSupport.h"
#include "Domains/Texture/McpAutomationBridge_TextureHandlersShared.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersAssetPathCanonical.h"

// Regression tests for the EXECUTOR half of guard/executor path agreement.
//
// The pre-queue gate side of "\Content\TeamA\Thing" was already asserted (see
// McpPrequeueGateTests PathConfinement). Nothing asserted the executor side, and
// the executors were replaying the canonicalizer's steps in the WRONG ORDER:
// they mapped the /Content alias BEFORE normalizing separators, so a
// backslash-prefixed value was invisible to the alias map. The guard admitted
// "/Game/TeamA/Thing" while the executor wrote "/Game/Content/TeamA/Thing" —
// outside the very prefix the principal was confined to.

namespace
{
// The exact block the executors used to run. Kept here as the test's ORACLE:
// the case is only meaningful if this really does disagree with the gate.
FString LegacyExecutorNormalize(const FString& In)
{
	FString Out = In;
	McpAssetPathCanonical::MapContentRootInline(Out);
	Out.ReplaceInline(TEXT("\\"), TEXT("/"));
	if (!Out.StartsWith(TEXT("/Game")))
	{
		Out = TEXT("/Game") / Out;
	}
	while (Out.EndsWith(TEXT("/")))
	{
		Out.LeftChopInline(1);
	}
	return Out;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpExecutorPathParityTest,
	"McpAutomationBridge.Foundation.ExecutorPathParity.ContentAlias",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpExecutorPathParityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FString Hostile = TEXT("\\Content\\TeamA\\Thing");
	const FString GateAnswer = McpCanonicalizeContentPath(Hostile, /*bAssumeGameRoot=*/true);

	TestEqual(TEXT("the gate resolves the backslash alias to the confined path"),
		GateAnswer, FString(TEXT("/Game/TeamA/Thing")));

	// The escape, demonstrated rather than asserted in prose.
	TestEqual(TEXT("the OLD executor ordering resolved the same value elsewhere"),
		LegacyExecutorNormalize(Hostile), FString(TEXT("/Game/Content/TeamA/Thing")));
	TestFalse(TEXT("guard and OLD executor disagreed on one value"),
		LegacyExecutorNormalize(Hostile) == GateAnswer);

	// Every converged executor normalizer must answer exactly what the gate answered.
	TestEqual(TEXT("the animation executor now agrees with the gate"),
		McpAnimationAuthoring::NormalizeAnimPath(Hostile), GateAnswer);
	TestEqual(TEXT("the texture executor now agrees with the gate"),
		McpTextureHandlers::NormalizeTexturePath(Hostile), GateAnswer);

	// Positive controls: ordinary paths still resolve and are NOT refused, so the
	// agreement above is not the trivial one where everything returns empty.
	const TCHAR* const Ordinary[] = {
		TEXT("/Game/TeamA/Thing"), TEXT("/Content/TeamA/Thing"),
		TEXT("/content/TeamA/Thing"), TEXT("/Game/TeamA/Thing/")
	};
	for (const TCHAR* const Value : Ordinary)
	{
		const FString Expected = McpCanonicalizeContentPath(Value, /*bAssumeGameRoot=*/true);
		TestEqual(TEXT("control: an ordinary path resolves inside the prefix"),
			Expected, FString(TEXT("/Game/TeamA/Thing")));
		TestEqual(TEXT("control: animation normalizer matches the canonicalizer"),
			McpAnimationAuthoring::NormalizeAnimPath(Value), Expected);
		TestEqual(TEXT("control: texture normalizer matches the canonicalizer"),
			McpTextureHandlers::NormalizeTexturePath(Value), Expected);
	}

	// A value with no trustworthy canonical form is REFUSED, not passed through
	// for the engine to resolve however it likes.
	TestTrue(TEXT("a traversal value is refused by the animation normalizer"),
		McpAnimationAuthoring::NormalizeAnimPath(TEXT("/Game/TeamA/../TeamB")).IsEmpty());
	TestTrue(TEXT("a drive-letter value is refused by the texture normalizer"),
		McpTextureHandlers::NormalizeTexturePath(TEXT("C:/Windows/system32")).IsEmpty());
	TestTrue(TEXT("an empty value stays empty"),
		McpAnimationAuthoring::NormalizeAnimPath(FString()).IsEmpty());

	// Boundary awareness survives the conversion: /ContentOther is a different
	// folder, not the content root, and must never become /GameOther. It now
	// canonicalizes to ITSELF rather than to empty, because a root this stage
	// does not recognise is treated as a plugin mount and left for the
	// post-queue validator to accept or refuse against FPackageName. The
	// invariant under test is the rewrite, not the refusal.
	TestEqual(TEXT("/ContentOther is not rewritten to /GameOther"),
		McpCanonicalizeContentPath(TEXT("/ContentOther/Thing"), /*bAssumeGameRoot=*/true),
		FString(TEXT("/ContentOther/Thing")));
	TestEqual(TEXT("the animation normalizer agrees on /ContentOther"),
		McpAnimationAuthoring::NormalizeAnimPath(TEXT("/ContentOther/Thing")),
		FString(TEXT("/ContentOther/Thing")));

	// Plugin content roots must survive this stage: every enabled plugin mounts
	// one, and emptying them here made all plugin-shipped content unreachable.
	TestEqual(TEXT("a plugin mount root canonicalizes instead of emptying"),
		McpCanonicalizeContentPath(TEXT("/MoverExamples/Characters/Meshes/SKM_Manny"), /*bAssumeGameRoot=*/true),
		FString(TEXT("/MoverExamples/Characters/Meshes/SKM_Manny")));

	// Widening the root shape must not admit host filesystem paths.
	const TCHAR* const HostRooted[] = {
		TEXT("/etc/passwd"), TEXT("/proc/self/environ"), TEXT("/usr/bin/sh"),
		TEXT("/home/user/.ssh/id_rsa"), TEXT("/var/log/syslog")
	};
	for (const TCHAR* const Value : HostRooted)
	{
		TestTrue(TEXT("a host filesystem root is refused"),
			McpCanonicalizeContentPath(Value, /*bAssumeGameRoot=*/true).IsEmpty());
	}

	return true;
}
#endif
