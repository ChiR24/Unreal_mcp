// McpNativeGatewayPreviewTests.cpp — in-editor run of the Task 43 preview rule
//
// tests/unit/task-43/preview-is-not-a-fake-dry-run.test.ts pins the same rule on
// the TypeScript surface, and asserts native equivalence by reading this
// plugin's source text. Source text cannot prove the gate actually fires, so
// this exercises McpValidateExecuteOptionsForCapability in the editor: the
// refusal itself, its typed code/pointer/message, the executable nextCall, and
// the narrowness that keeps every other option dispatching as before.
//
// Rule -> outcome:
//   options.preview == true        -> refused, UNSUPPORTED_PREVIEW, nothing queued
//   options.preview == false       -> accepted (asks for real execution)
//   options.preview absent         -> accepted
//   options.preview non-boolean    -> refused, INVALID_OPTIONS (envelope stage)
//   any other honored option       -> accepted

#include "MCP/Execute/McpNativeGatewayCanonicalRecords.h"
#include "MCP/Execute/McpNativeGatewayExecuteRequest.h"
#include "MCP/Execute/McpNativeGatewayReceipt.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace McpNativeGatewayPreviewFixtures
{
struct FOutcome
{
	bool bAccepted = false;
	FMcpSemanticError Error;
	TSharedPtr<FJsonObject> Guidance;
};

FOutcome Validate(
	const TSharedPtr<FJsonObject>& Options, const TSharedPtr<FJsonObject>& Params,
	const FMcpCapabilityRecord* Record)
{
	FOutcome Outcome;
	Outcome.bAccepted = McpValidateExecuteOptionsForCapability(
		Options, Params, Record, Outcome.Error, Outcome.Guidance);
	return Outcome;
}

TSharedPtr<FJsonObject> PreviewOptions(bool bPreview)
{
	TSharedPtr<FJsonObject> Options = MakeShared<FJsonObject>();
	Options->SetBoolField(TEXT("preview"), bPreview);
	return Options;
}

TSharedPtr<FJsonObject> DoomedAssetParams()
{
	TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("assetPath"), TEXT("/Game/Task43/DoomedAsset"));
	return Params;
}

FString ObjectString(const TSharedPtr<FJsonObject>& Object, const FString& Field)
{
	FString Value;
	if (Object.IsValid())
	{
		Object->TryGetStringField(Field, Value);
	}
	return Value;
}

bool DeclaresSupportsPreview(const FMcpCapabilityRecord& Record)
{
	bool bDeclared = false;
	return Record.Behavior.IsValid() &&
		Record.Behavior->TryGetBoolField(TEXT("supportsPreview"), bDeclared) && bDeclared;
}

TSharedPtr<FJsonObject> ClaimSupportsPreview(const FMcpCapabilityRecord& Record)
{
	TSharedPtr<FJsonObject> Behavior = MakeShared<FJsonObject>();
	if (Record.Behavior.IsValid())
	{
		Behavior->Values = Record.Behavior->Values;
	}
	Behavior->SetBoolField(TEXT("supportsPreview"), true);
	return Behavior;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpNativeGatewayPreviewRefusalTest,
	"McpAutomationBridge.NativeGateway.PreviewRefusal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpNativeGatewayPreviewRefusalTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpNativeGatewayPreviewFixtures;

	const FMcpCanonicalRecordIndex& Index = FMcpCanonicalRecordIndex::Get();
	if (!Index.IsLoaded())
	{
		AddError(FString::Printf(TEXT("index load error: %s"), *Index.GetLoadError()));
		return false;
	}

	const FMcpCapabilityRecord* Destructive = Index.FindById(TEXT("asset.delete"));
	const FMcpCapabilityRecord* Declared = Index.FindById(TEXT("asset.duplicate"));
	const FMcpCapabilityRecord* ReadOnly = Index.FindById(TEXT("asset.list"));
	if (!Destructive || !Declared || !ReadOnly)
	{
		AddError(TEXT("fixture capabilities absent from the generated registry"));
		return false;
	}

	const FOutcome Refused = Validate(PreviewOptions(true), DoomedAssetParams(), Destructive);
	TestFalse(TEXT("preview:true on a destructive capability is refused"), Refused.bAccepted);
	TestEqual(TEXT("refusal is UNSUPPORTED_PREVIEW"),
		Refused.Error.GatewayCode, FString(TEXT("UNSUPPORTED_PREVIEW")));
	TestEqual(TEXT("refusal is a validation error"),
		Refused.Error.Kind, FString(TEXT("validation")));
	TestEqual(TEXT("refusal points at options.preview"),
		Refused.Error.Pointer, FString(TEXT("/options/preview")));
	TestTrue(TEXT("refusal names the capability"),
		Refused.Error.Message.Contains(Destructive->Id));
	TestTrue(TEXT("refusal explains that no dry run exists"),
		Refused.Error.Message.Contains(TEXT("does not implement options.preview")));

	// A declared behavior.supportsPreview is metadata, not an implementation.
	// Honouring it would keep the fake dry run for the most dangerous records.
	// The claim is injected on a local copy so this proves the gate ignores the
	// declaration, whatever the generated catalog currently happens to declare.
	FMcpCapabilityRecord Claiming = *Declared;
	Claiming.Behavior = ClaimSupportsPreview(*Declared);
	TestTrue(TEXT("fixture claims supportsPreview"), DeclaresSupportsPreview(Claiming));
	const FOutcome DeclaredRefused = Validate(PreviewOptions(true), nullptr, &Claiming);
	TestFalse(TEXT("a supportsPreview declaration buys no pass"), DeclaredRefused.bAccepted);
	TestEqual(TEXT("declared record refused with the same code"),
		DeclaredRefused.Error.GatewayCode, FString(TEXT("UNSUPPORTED_PREVIEW")));

	// One uniform rule across the catalog: the two fixtures sit on opposite sides
	// of the declared effect boundary and are refused identically.
	const FOutcome ReadRefused = Validate(PreviewOptions(true), nullptr, ReadOnly);
	TestNotEqual(TEXT("fixtures straddle the declared effect boundary"),
		Destructive->Effect, ReadOnly->Effect);
	TestFalse(TEXT("a read-only capability is refused too"), ReadRefused.bAccepted);
	TestEqual(TEXT("read-only refused with the same code"),
		ReadRefused.Error.GatewayCode, Refused.Error.GatewayCode);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpNativeGatewayPreviewGuidanceTest,
	"McpAutomationBridge.NativeGateway.PreviewGuidance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpNativeGatewayPreviewGuidanceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpNativeGatewayPreviewFixtures;

	const FMcpCanonicalRecordIndex& Index = FMcpCanonicalRecordIndex::Get();
	const FMcpCapabilityRecord* Destructive =
		Index.IsLoaded() ? Index.FindById(TEXT("asset.delete")) : nullptr;
	if (!Destructive)
	{
		AddError(TEXT("asset.delete absent from the generated registry"));
		return false;
	}

	TSharedPtr<FJsonObject> Options = PreviewOptions(true);
	Options->SetNumberField(TEXT("timeoutMs"), 1000);
	const FOutcome Refused = Validate(Options, DoomedAssetParams(), Destructive);
	TestFalse(TEXT("preview:true is refused"), Refused.bAccepted);
	if (!Refused.Guidance.IsValid())
	{
		AddError(TEXT("refusal carried no guidance payload"));
		return false;
	}

	const TSharedPtr<FJsonObject>* NextCall = nullptr;
	if (!Refused.Guidance->TryGetObjectField(TEXT("nextCall"), NextCall) || !NextCall)
	{
		AddError(TEXT("refusal carried no nextCall"));
		return false;
	}

	TestEqual(TEXT("nextCall re-runs execute"),
		ObjectString(*NextCall, TEXT("operation")), FString(TEXT("execute")));
	TestEqual(TEXT("nextCall targets the parent tool"),
		ObjectString(*NextCall, TEXT("tool")), Destructive->Parent);
	TestEqual(TEXT("nextCall names the client-facing action"),
		ObjectString(*NextCall, TEXT("action")),
		Index.GetLegacyActionForCapability(Destructive->Id));

	const TSharedPtr<FJsonObject>* NextParams = nullptr;
	TestTrue(TEXT("nextCall keeps the caller's params"),
		(*NextCall)->TryGetObjectField(TEXT("params"), NextParams) && NextParams);
	if (NextParams)
	{
		TestEqual(TEXT("nextCall preserves the target path"),
			ObjectString(*NextParams, TEXT("assetPath")),
			FString(TEXT("/Game/Task43/DoomedAsset")));
	}

	// Only the control the gateway cannot honor is dropped; the rest survives so
	// the refusal hands back a call the client can run unchanged.
	const TSharedPtr<FJsonObject>* NextOptions = nullptr;
	TestTrue(TEXT("nextCall keeps the remaining options"),
		(*NextCall)->TryGetObjectField(TEXT("options"), NextOptions) && NextOptions);
	if (NextOptions)
	{
		double Timeout = 0.0;
		TestTrue(TEXT("nextCall preserves timeoutMs"),
			(*NextOptions)->TryGetNumberField(TEXT("timeoutMs"), Timeout));
		TestEqual(TEXT("timeoutMs is unchanged"), Timeout, 1000.0);
		TestFalse(TEXT("nextCall never re-sends preview"),
			(*NextOptions)->TryGetField(TEXT("preview")).IsValid());
	}

	const TArray<TSharedPtr<FJsonValue>>* Suggestions = nullptr;
	if (Refused.Guidance->TryGetArrayField(TEXT("suggestions"), Suggestions) && Suggestions)
	{
		for (const TSharedPtr<FJsonValue>& Entry : *Suggestions)
		{
			FString Suggestion;
			if (Entry.IsValid() && Entry->TryGetString(Suggestion))
			{
				TestNotEqual(TEXT("suggestions never advertise preview"),
					Suggestion, FString(TEXT("preview")));
			}
		}
	}

	const FOutcome OnlyPreview = Validate(PreviewOptions(true), nullptr, Destructive);
	const TSharedPtr<FJsonObject>* OnlyPreviewNext = nullptr;
	if (OnlyPreview.Guidance.IsValid() &&
		OnlyPreview.Guidance->TryGetObjectField(TEXT("nextCall"), OnlyPreviewNext) &&
		OnlyPreviewNext)
	{
		TestFalse(TEXT("an options envelope holding only preview is dropped"),
			(*OnlyPreviewNext)->HasField(TEXT("options")));
	}
	else
	{
		AddError(TEXT("preview-only refusal carried no nextCall"));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpNativeGatewayPreviewNarrownessTest,
	"McpAutomationBridge.NativeGateway.PreviewRefusalIsNarrow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpNativeGatewayPreviewNarrownessTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpNativeGatewayPreviewFixtures;

	const FMcpCanonicalRecordIndex& Index = FMcpCanonicalRecordIndex::Get();
	const FMcpCapabilityRecord* Destructive =
		Index.IsLoaded() ? Index.FindById(TEXT("asset.delete")) : nullptr;
	if (!Destructive)
	{
		AddError(TEXT("asset.delete absent from the generated registry"));
		return false;
	}

	const FOutcome NoOptions = Validate(nullptr, DoomedAssetParams(), Destructive);
	TestTrue(TEXT("a request without options still dispatches"), NoOptions.bAccepted);

	const FOutcome EmptyOptions =
		Validate(MakeShared<FJsonObject>(), DoomedAssetParams(), Destructive);
	TestTrue(TEXT("an empty options envelope still dispatches"), EmptyOptions.bAccepted);

	// preview:false asks for real execution and is honored, so the refusal cannot
	// be widened into "any request that mentions preview".
	const FOutcome ExplicitFalse =
		Validate(PreviewOptions(false), DoomedAssetParams(), Destructive);
	TestTrue(TEXT("preview:false still dispatches"), ExplicitFalse.bAccepted);
	TestTrue(TEXT("preview:false raises no guidance"), !ExplicitFalse.Guidance.IsValid());

	TSharedPtr<FJsonObject> Honored = MakeShared<FJsonObject>();
	Honored->SetNumberField(TEXT("timeoutMs"), 5000);
	TestTrue(TEXT("an honored option still dispatches"),
		Validate(Honored, DoomedAssetParams(), Destructive).bAccepted);

	TSharedPtr<FJsonObject> NonBoolean = MakeShared<FJsonObject>();
	NonBoolean->SetStringField(TEXT("preview"), TEXT("yes"));
	const FOutcome BadType = Validate(NonBoolean, DoomedAssetParams(), Destructive);
	TestFalse(TEXT("a non-boolean preview is refused"), BadType.bAccepted);
	TestEqual(TEXT("the envelope stage still owns the type error"),
		BadType.Error.GatewayCode, FString(TEXT("INVALID_OPTIONS")));

	return true;
}

#endif  // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
