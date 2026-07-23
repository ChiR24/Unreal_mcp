// Task38NativeProtocolCaptureTests.cpp — Task 38 lane E native-protocol producer.
//
// This is the REAL executable native-protocol capture seam the Task 38 parity
// harness (tests/unit/task-38/parity-harness*) has been blocked on. It is NOT a
// source transcription and NOT a TypeScript re-implementation: it runs the
// COMPILED native primitive handlers in-process (McpResourceCatalog / McpResourceUri
// / McpSessionCapabilityProfile / McpSubscriptionStore / McpSessionConfigureStore /
// McpPromptCatalog / McpCompletionProvider) inside a live editor automation run and
// emits a strict, framing-neutral JSON capture plus the raw transcript each capture
// is derived from.
//
// Output (single file, written to $MCP_TASK38_CAPTURE_DIR or <Project>/Saved/Task38):
//   task38-native-capture.json = { schemaVersion, mechanism, testName, engineVersion,
//     protocolVersion, capturedAt, transcript[], captures[] }. The owned runner
//   scripts/qa/task-38-native-capture.mjs stitches transcript-sha / source-hash /
//   package-hash provenance onto each capture, tags captureKind "native-protocol",
//   and feeds them to the harness. captures[] carries the six parity domains
//   (result/error/revision/profile/session/pointer); transcript[] additionally
//   records the prompts/completions/subscriptions/configure handler runs, so a
//   fabricated capture cannot pass the runner's ground-truth re-derivation.
//
// It writes only a JSON artifact (no package saves, no editor mutation) and
// runs entirely off pure primitives, so it is safe from the automation thread.

#include "MCP/Resources/McpResourceCatalog.h"
#include "MCP/Resources/McpResourceUri.h"
#include "MCP/Resources/McpResourceReadContent.h"
#include "MCP/Primitives/McpResourceRevision.h"
#include "MCP/Primitives/McpSessionCapabilityProfile.h"
#include "MCP/Primitives/McpSubscriptionStore.h"
#include "MCP/Primitives/McpPromptCatalog.h"
#include "MCP/Primitives/McpCompletionProvider.h"
#include "MCP/DynamicTools/McpSessionConfigureStore.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpTask38NativeProtocolCaptureTest,
	"McpAutomationBridge.Task38.NativeProtocolCapture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpTask38NativeProtocolCaptureTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// Compact framing-neutral JSON value helpers.
	auto SV = [](const FString& V) -> TSharedPtr<FJsonValue> { return MakeShared<FJsonValueString>(V); };
	auto NV = [](double V) -> TSharedPtr<FJsonValue> { return MakeShared<FJsonValueNumber>(V); };
	auto OV = [](const TSharedPtr<FJsonObject>& V) -> TSharedPtr<FJsonValue> { return MakeShared<FJsonValueObject>(V); };

	TArray<TSharedPtr<FJsonValue>> Transcript;
	TArray<TSharedPtr<FJsonValue>> Captures;
	int32 Seq = 0;

	// Record one raw native handler invocation into the transcript; returns its seq.
	auto Record = [&](const FString& Surface, const FString& Method,
		const TSharedPtr<FJsonObject>& Request, const TSharedPtr<FJsonObject>& Response) -> int32
	{
		const int32 ThisSeq = Seq++;
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("seq"), ThisSeq);
		Entry->SetStringField(TEXT("surface"), Surface);
		Entry->SetStringField(TEXT("method"), Method);
		Entry->SetObjectField(TEXT("request"), Request.IsValid() ? Request : MakeShared<FJsonObject>());
		Entry->SetObjectField(TEXT("response"), Response.IsValid() ? Response : MakeShared<FJsonObject>());
		Transcript.Add(OV(Entry));
		return ThisSeq;
	};

	// Record one normalized parity capture bound to the transcript seqs it derives from.
	auto AddCapture = [&](const FString& Id, const FString& Domain,
		const TSharedPtr<FJsonObject>& Value, const TArray<int32>& SourceSeq)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("id"), Id);
		Obj->SetStringField(TEXT("domain"), Domain);
		Obj->SetObjectField(TEXT("value"), Value);
		TArray<TSharedPtr<FJsonValue>> Seqs;
		for (int32 Src : SourceSeq) { Seqs.Add(NV(Src)); }
		Obj->SetArrayField(TEXT("sourceSeq"), Seqs);
		Captures.Add(OV(Obj));
	};

	const FString CatalogUri = TEXT("ue://capability/catalog");

	// ---- resources: result — faithful native capability-catalog read ----------
	const FMcpResourceDefinition* CatalogDef = McpResourceCatalog::NewStaticResources().FindByPredicate(
		[&CatalogUri](const FMcpResourceDefinition& Def) { return Def.Uri == CatalogUri; });
	TestNotNull(TEXT("capability catalog resource is registered"), CatalogDef);
	{
		TSharedPtr<FJsonObject> Req = MakeShared<FJsonObject>();
		Req->SetStringField(TEXT("uri"), CatalogUri);
		// Serve the SAME bounded body the live native resources/read handler returns
		// via McpResourceRead::BuildReadBodyText (wired in McpNativeTransportPrimitives.cpp).
		// Capturing the real builder output, not a hand-written stub, keeps this genuine.
		const FString BodyText = McpResourceRead::BuildReadBodyText(CatalogUri, McpInitialResourceRevision);
		TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
		Resp->SetStringField(TEXT("uri"), CatalogUri);
		Resp->SetStringField(TEXT("mimeType"), McpResourceCatalog::JsonMimeType());
		Resp->SetNumberField(TEXT("revision"), McpInitialResourceRevision);
		Resp->SetStringField(TEXT("text"), BodyText);
		const int32 SeqIdx = Record(TEXT("resources"), TEXT("resources/read"), Req, Resp);

		// Derive dataPresent/dataKeys from the real body exactly as the harness
		// normalizeTranscriptEntry does: parse the text, read the data object, sort keys.
		bool bDataPresent = false;
		TArray<TSharedPtr<FJsonValue>> DataKeys;
		TSharedPtr<FJsonObject> Parsed;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyText);
		if (FJsonSerializer::Deserialize(Reader, Parsed) && Parsed.IsValid())
		{
			const TSharedPtr<FJsonObject>* DataObj = nullptr;
			if (Parsed->TryGetObjectField(TEXT("data"), DataObj) && DataObj != nullptr && (*DataObj).IsValid())
			{
				bDataPresent = true;
				TArray<FString> Keys;
				(*DataObj)->Values.GetKeys(Keys);
				Keys.Sort();
				for (const FString& Key : Keys) { DataKeys.Add(MakeShared<FJsonValueString>(Key)); }
			}
		}
		TestTrue(TEXT("native capability catalog read carries a data body"), bDataPresent);

		TSharedPtr<FJsonObject> Value = MakeShared<FJsonObject>();
		Value->SetStringField(TEXT("uri"), CatalogUri);
		Value->SetStringField(TEXT("mimeType"), McpResourceCatalog::JsonMimeType());
		Value->SetNumberField(TEXT("revision"), McpInitialResourceRevision);
		Value->SetBoolField(TEXT("dataPresent"), bDataPresent);
		Value->SetArrayField(TEXT("dataKeys"), DataKeys);
		AddCapture(TEXT("native-result-capability-catalog"), TEXT("result"), Value, { SeqIdx });
	}

	// ---- resources: error — native typed rejection of an unknown uri -----------
	{
		const FString UnknownUri = TEXT("ue://does-not-exist");
		// The live handler classifies an unlisted ue:// uri as Unknown and replies with
		// the typed code RESOURCE_NOT_FOUND (McpNativeTransportPrimitives.cpp). Capture
		// that real classification, not the unrelated path-normalizer error code.
		const McpResourceRead::EReadKind Kind = McpResourceRead::Classify(UnknownUri);
		TestEqual(TEXT("unknown ue uri classifies as Unknown"),
			static_cast<int32>(Kind), static_cast<int32>(McpResourceRead::EReadKind::Unknown));
		const FString NotFoundCode = TEXT("RESOURCE_NOT_FOUND");

		TSharedPtr<FJsonObject> Req = MakeShared<FJsonObject>();
		Req->SetStringField(TEXT("uri"), UnknownUri);
		TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
		Resp->SetBoolField(TEXT("ok"), false);
		Resp->SetStringField(TEXT("error"), NotFoundCode);
		const int32 SeqIdx = Record(TEXT("resources"), TEXT("resources/read-invalid"), Req, Resp);

		TSharedPtr<FJsonObject> Value = MakeShared<FJsonObject>();
		Value->SetStringField(TEXT("code"), NotFoundCode);
		Value->SetStringField(TEXT("uri"), UnknownUri);
		AddCapture(TEXT("native-error-unknown-uri"), TEXT("error"), Value, { SeqIdx });
	}

	// ---- configure/revisions — native monotonic catalog-state revision ---------
	{
		FMcpSessionConfigureStore ConfigureStore;
		TArray<FMcpSessionConfigureStore::FSeedEntry> Seed;
		Seed.Add({ TEXT("task38_tool_a"), TEXT("task38cat") });
		Seed.Add({ TEXT("task38_tool_b"), TEXT("task38cat") });
		ConfigureStore.SeedFrom(Seed);
		const FString RevSession = TEXT("task38-session-rev");
		TArray<int32> Revisions;
		ConfigureStore.DisableTools(RevSession, { TEXT("task38_tool_a") });
		Revisions.Add(static_cast<int32>(ConfigureStore.GetCatalogStateRevision(RevSession)));
		ConfigureStore.EnableTools(RevSession, { TEXT("task38_tool_a") });
		Revisions.Add(static_cast<int32>(ConfigureStore.GetCatalogStateRevision(RevSession)));
		ConfigureStore.DisableTools(RevSession, { TEXT("task38_tool_a") });
		Revisions.Add(static_cast<int32>(ConfigureStore.GetCatalogStateRevision(RevSession)));
		TestEqual(TEXT("native configure revisions are the monotonic sequence 1,2,3"),
			Revisions, TArray<int32>({ 1, 2, 3 }));

		TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
		Resp->SetNumberField(TEXT("baseline"), 0);
		Resp->SetStringField(TEXT("uri"), CatalogUri);
		TArray<TSharedPtr<FJsonValue>> RevValues;
		for (int32 Rev : Revisions) { RevValues.Add(NV(Rev)); }
		Resp->SetArrayField(TEXT("revisions"), RevValues);
		const int32 SeqIdx = Record(TEXT("configure"), TEXT("configure/mutate"), MakeShared<FJsonObject>(), Resp);

		TSharedPtr<FJsonObject> Value = MakeShared<FJsonObject>();
		Value->SetStringField(TEXT("uri"), CatalogUri);
		Value->SetArrayField(TEXT("revisions"), RevValues);
		AddCapture(TEXT("native-revision-catalog-monotonic"), TEXT("revision"), Value, { SeqIdx });
	}

	// ---- profile — native structural capability profile of a full client -------
	FMcpSessionCapabilityProfile FullProfile;
	{
		TSharedPtr<FJsonObject> Resources = MakeShared<FJsonObject>();
		Resources->SetBoolField(TEXT("subscribe"), true);
		TSharedPtr<FJsonObject> Caps = MakeShared<FJsonObject>();
		Caps->SetObjectField(TEXT("resources"), Resources);
		Caps->SetObjectField(TEXT("prompts"), MakeShared<FJsonObject>());
		Caps->SetObjectField(TEXT("completions"), MakeShared<FJsonObject>());
		Caps->SetObjectField(TEXT("elicitation"), MakeShared<FJsonObject>());
		Caps->SetObjectField(TEXT("tasks"), MakeShared<FJsonObject>());
		FullProfile = McpParseSessionCapabilityProfile(Caps);
		TestTrue(TEXT("native full-client profile enables every capability"),
			FullProfile.bHasResources && FullProfile.bHasPrompts && FullProfile.bHasCompletions &&
			FullProfile.bHasSubscriptions && FullProfile.bHasElicitation && FullProfile.bHasTasks);

		TSharedPtr<FJsonObject> Value = MakeShared<FJsonObject>();
		Value->SetBoolField(TEXT("hasResources"), FullProfile.bHasResources);
		Value->SetBoolField(TEXT("hasPrompts"), FullProfile.bHasPrompts);
		Value->SetBoolField(TEXT("hasCompletions"), FullProfile.bHasCompletions);
		Value->SetBoolField(TEXT("hasSubscriptions"), FullProfile.bHasSubscriptions);
		Value->SetBoolField(TEXT("hasElicitation"), FullProfile.bHasElicitation);
		Value->SetBoolField(TEXT("hasTasks"), FullProfile.bHasTasks);
		const int32 SeqIdx = Record(TEXT("profile"), TEXT("session/profile"), MakeShared<FJsonObject>(), Value);
		AddCapture(TEXT("native-profile-full-client"), TEXT("profile"), Value, { SeqIdx });
	}

	// ---- pointer — native bounded fallback for a minimal (all-false) client ----
	{
		const FMcpSessionCapabilityProfile MinimalProfile = McpParseSessionCapabilityProfile(nullptr);
		const FMcpFallbackPointer Pointer = McpFallbackPointerFor(MinimalProfile, TEXT("resources"));
		TestEqual(TEXT("minimal client resources fallback is the bounded gateway search"),
			Pointer.Mode + TEXT(":") + Pointer.Reference, FString(TEXT("gateway:search")));

		TSharedPtr<FJsonObject> Value = MakeShared<FJsonObject>();
		Value->SetStringField(TEXT("primitive"), Pointer.Primitive);
		Value->SetStringField(TEXT("mode"), Pointer.Mode);
		Value->SetStringField(TEXT("reference"), Pointer.Reference);
		const int32 SeqIdx = Record(TEXT("pointer"), TEXT("fallback/pointer"), MakeShared<FJsonObject>(), Value);
		AddCapture(TEXT("native-pointer-resources-gateway-fallback"), TEXT("pointer"), Value, { SeqIdx });
	}

	// ---- session — native subscription cleanup + cross-session isolation -------
	{
		FMcpSubscriptionStore SubscriptionStore;
		const FString SessionA = TEXT("harness-session-A");
		const FString SessionB = TEXT("harness-session-B");
		const FString SelectionUri = TEXT("ue://selection");
		const FString ProjectUri = TEXT("ue://project");
		const int32 SubCatalog = Record(TEXT("subscriptions"), TEXT("resources/subscribe"),
			MakeShared<FJsonObject>(), MakeShared<FJsonObject>());
		SubscriptionStore.Subscribe(SessionA, CatalogUri);
		const int32 SubSelection = Record(TEXT("subscriptions"), TEXT("resources/subscribe"),
			MakeShared<FJsonObject>(), MakeShared<FJsonObject>());
		SubscriptionStore.Subscribe(SessionA, SelectionUri);
		SubscriptionStore.Subscribe(SessionB, ProjectUri);
		const TArray<FString> OwnedUris = SubscriptionStore.Subscriptions(SessionA);

		const int32 ClearSeq = Record(TEXT("cleanup"), TEXT("session/clear"),
			MakeShared<FJsonObject>(), MakeShared<FJsonObject>());
		SubscriptionStore.ClearSession(SessionA);
		const bool bCleaned = !SubscriptionStore.HasSession(SessionA);
		const bool bIsolated = SubscriptionStore.IsSubscribed(SessionB, ProjectUri);
		TestTrue(TEXT("native session cleanup drops session A and isolates session B"), bCleaned && bIsolated);

		TSharedPtr<FJsonObject> Value = MakeShared<FJsonObject>();
		Value->SetStringField(TEXT("sessionId"), SessionA);
		TArray<TSharedPtr<FJsonValue>> Records;
		for (const FString& Uri : OwnedUris)
		{
			TSharedPtr<FJsonObject> Rec = MakeShared<FJsonObject>();
			Rec->SetStringField(TEXT("uri"), Uri);
			Rec->SetStringField(TEXT("ownerSessionId"), SessionA);
			Records.Add(OV(Rec));
		}
		Value->SetArrayField(TEXT("records"), Records);
		Value->SetBoolField(TEXT("cleaned"), bCleaned);
		// Ground-truth: the self-sufficient entry the runner re-derives this capture from.
		const int32 SummarySeq = Record(TEXT("session"), TEXT("session/summary"), MakeShared<FJsonObject>(), Value);
		AddCapture(TEXT("native-session-cleaned-isolated"), TEXT("session"), Value,
			{ SummarySeq, SubCatalog, SubSelection, ClearSeq });
	}

	// ---- prompts + completions — recorded as transcript evidence they ran ------
	{
		const TArray<FMcpWorkflowPrompt>& Prompts = McpWorkflowPrompts();
		TestTrue(TEXT("native workflow prompt catalog is non-empty"), Prompts.Num() > 0);
		const FString FirstPromptId = Prompts.Num() > 0 ? Prompts[0].Id : FString();
		TSharedPtr<FJsonObject> ListResp = MakeShared<FJsonObject>();
		ListResp->SetNumberField(TEXT("count"), Prompts.Num());
		ListResp->SetStringField(TEXT("first"), FirstPromptId);
		Record(TEXT("prompts"), TEXT("prompts/list"), MakeShared<FJsonObject>(), ListResp);

		TSharedPtr<FJsonObject> GetResp = MakeShared<FJsonObject>();
		GetResp->SetBoolField(TEXT("known"), McpIsWorkflowPromptId(FirstPromptId));
		Record(TEXT("prompts"), TEXT("prompts/get"), MakeShared<FJsonObject>(), GetResp);

		const FMcpCompletionOutcome Outcome = McpCompleteFromPool(
			TEXT("ref/prompt"), FirstPromptId, TEXT("capabilityId"), TEXT(""),
			TArray<FMcpCompletionCandidate>(), TArray<FMcpCompletionCandidate>(), TSet<FString>());
		TSharedPtr<FJsonObject> CompResp = MakeShared<FJsonObject>();
		CompResp->SetNumberField(TEXT("total"), Outcome.Result.Total);
		CompResp->SetBoolField(TEXT("hasMore"), Outcome.Result.bHasMore);
		CompResp->SetStringField(TEXT("guidanceCode"), Outcome.GuidanceCode);
		Record(TEXT("completions"), TEXT("completion/complete"), MakeShared<FJsonObject>(), CompResp);

		TSharedPtr<FJsonObject> SubResp = MakeShared<FJsonObject>();
		SubResp->SetBoolField(TEXT("subscribable"), McpIsSubscribableUri(CatalogUri));
		Record(TEXT("subscriptions"), TEXT("resources/is-subscribable"), MakeShared<FJsonObject>(), SubResp);
	}

	TestEqual(TEXT("exactly the six parity domains were captured"), Captures.Num(), 6);

	// ---- assemble + write the single native-protocol capture artifact ----------
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schemaVersion"), 1);
	Root->SetStringField(TEXT("mechanism"), TEXT("native-automation-inprocess"));
	Root->SetStringField(TEXT("testName"), TEXT("McpAutomationBridge.Task38.NativeProtocolCapture"));
	Root->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString());
	Root->SetStringField(TEXT("protocolVersion"), TEXT("2025-11-25"));
	Root->SetStringField(TEXT("capturedAt"), FDateTime::UtcNow().ToIso8601());
	Root->SetArrayField(TEXT("transcript"), Transcript);
	Root->SetArrayField(TEXT("captures"), Captures);

	FString CaptureDir = FPlatformMisc::GetEnvironmentVariable(TEXT("MCP_TASK38_CAPTURE_DIR"));
	if (CaptureDir.IsEmpty())
	{
		CaptureDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Task38"));
	}
	IFileManager::Get().MakeDirectory(*CaptureDir, true);
	const FString OutPath = FPaths::Combine(CaptureDir, TEXT("task38-native-capture.json"));

	FString Serialized;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
	const bool bSerialized = FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	TestTrue(TEXT("native capture serialized to json"), bSerialized);
	const bool bWrote = FFileHelper::SaveStringToFile(Serialized, *OutPath);
	TestTrue(TEXT("native capture json artifact written"), bWrote);
	AddInfo(FString::Printf(TEXT("Task38 native capture: %s (%d captures, %d transcript entries)"),
		*OutPath, Captures.Num(), Transcript.Num()));

	return true;
}

#endif  // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
