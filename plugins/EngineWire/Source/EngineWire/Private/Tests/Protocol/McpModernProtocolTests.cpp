#include "MCP/Protocol/McpModernConsent.h"
#include "MCP/Protocol/McpModernRequest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Foundation/McpCapabilityAuthorization.h"
#include "MCP/DynamicTools/McpTaskResults.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
TSharedPtr<FJsonObject> ParseModernJson(const FString& Text)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

McpModern::FCall ModernToolsCall(bool bElicitation)
{
	McpModern::FCall Call;
	Call.bActive = true;
	Call.bElicitationForm = bElicitation;
	Call.Method = TEXT("tools/call");
	return Call;
}

TSharedPtr<FJsonObject> ConsentAnswer(const TCHAR* Action, bool bConfirm)
{
	return ParseModernJson(FString::Printf(
		TEXT("{\"consent\":{\"action\":\"%s\",\"content\":{\"confirm\":%s}}}"), Action, bConfirm ? TEXT("true") : TEXT("false")));
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpModernProtocolHeadersTest,
	"EngineWire.Protocol.Modern.HeadersAndResults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpModernProtocolHeadersTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const TSharedPtr<FJsonObject> Params = ParseModernJson(TEXT(
		"{\"name\":\"unreal\",\"_meta\":{\"io.modelcontextprotocol/protocolVersion\":\"2026-07-28\","
		"\"io.modelcontextprotocol/clientCapabilities\":{\"elicitation\":{},\"extensions\":{\"io.modelcontextprotocol/tasks\":{}}}}}"));
	TestTrue(TEXT("a _meta version makes a request modern"), McpModern::IsModern(Params));
	TestFalse(TEXT("an initialize-era request is not modern"), McpModern::IsModern(ParseModernJson(TEXT("{\"name\":\"unreal\"}"))));
	const McpModern::FMeta Meta = McpModern::ReadMeta(Params);
	TestTrue(TEXT("tasks extension read"), Meta.bTasks);
	TestTrue(TEXT("an elicitation capability naming no mode means form"), Meta.bElicitationForm);
	TestFalse(TEXT("url-only elicitation is not form"), McpModern::ReadMeta(ParseModernJson(TEXT(
		"{\"_meta\":{\"io.modelcontextprotocol/protocolVersion\":\"2026-07-28\","
		"\"io.modelcontextprotocol/clientCapabilities\":{\"elicitation\":{\"url\":{}}}}}"))).bElicitationForm);

	const FString V = McpModern::Version;
	TestTrue(TEXT("matching headers pass"),
		McpModern::HeaderMismatch(V, TEXT("tools/call"), TEXT("unreal"), TEXT("tools/call"), Params, V).IsEmpty());
	TestTrue(TEXT("a base64 sentinel name is decoded before the compare"),
		McpModern::HeaderMismatch(V, TEXT("tools/call"), TEXT("=?base64?dW5yZWFs?="), TEXT("tools/call"), Params, V).IsEmpty());
	TestTrue(TEXT("a missing Mcp-Method is a mismatch"),
		McpModern::HeaderMismatch(V, FString(), TEXT("unreal"), TEXT("tools/call"), Params, V).Contains(TEXT("Mcp-Method")));
	TestTrue(TEXT("a different Mcp-Name is a mismatch"),
		McpModern::HeaderMismatch(V, TEXT("tools/call"), TEXT("other"), TEXT("tools/call"), Params, V).Contains(TEXT("Mcp-Name")));
	TestTrue(TEXT("the version header must equal the body's"),
		McpModern::HeaderMismatch(TEXT("2025-11-25"), TEXT("tools/call"), TEXT("unreal"), TEXT("tools/call"), Params, V)
			.Contains(TEXT("MCP-Protocol-Version")));
	TestTrue(TEXT("tools/list needs no Mcp-Name"),
		McpModern::HeaderMismatch(V, TEXT("tools/list"), FString(), TEXT("tools/list"), Params, V).IsEmpty());

	const TSharedPtr<FJsonObject> Listing = MakeShared<FJsonObject>();
	McpModern::StampResult(TEXT("tools/list"), Listing);
	TestEqual(TEXT("resultType added"), Listing->GetStringField(TEXT("resultType")), FString(TEXT("complete")));
	TestEqual(TEXT("listing cached an hour"), Listing->GetNumberField(TEXT("ttlMs")), 3600000.0);
	TestEqual(TEXT("listing is public"), Listing->GetStringField(TEXT("cacheScope")), FString(TEXT("public")));
	const TSharedPtr<FJsonObject> Read = MakeShared<FJsonObject>();
	McpModern::StampResult(TEXT("resources/read"), Read);
	TestEqual(TEXT("a read is never reused"), Read->GetNumberField(TEXT("ttlMs")), 0.0);
	TestEqual(TEXT("a read is private"), Read->GetStringField(TEXT("cacheScope")), FString(TEXT("private")));
	const TSharedPtr<FJsonObject> Asked = ParseModernJson(TEXT("{\"resultType\":\"input_required\"}"));
	McpModern::StampResult(TEXT("tools/call"), Asked);
	TestEqual(TEXT("an existing resultType is kept"), Asked->GetStringField(TEXT("resultType")), FString(TEXT("input_required")));
	TestFalse(TEXT("a call result has no cache fields"), Asked->HasField(TEXT("ttlMs")));

	const FDateTime Now = FDateTime::UtcNow();
	const TSharedPtr<FJsonObject> Working = McpModern::TaskObject(TEXT("t1"), TEXT("working"), TEXT("busy"), Now, Now, TEXT("task"));
	TestEqual(TEXT("task handle resultType"), Working->GetStringField(TEXT("resultType")), FString(TEXT("task")));
	TestTrue(TEXT("a working task says how often to poll"), Working->HasField(TEXT("pollIntervalMs")));
	TestTrue(TEXT("ttlMs is always present"), Working->HasField(TEXT("ttlMs")));
	TestFalse(TEXT("a finished task has no poll interval"),
		McpModern::TaskObject(TEXT("t1"), TEXT("completed"), FString(), Now, Now, TEXT("complete"))->HasField(TEXT("pollIntervalMs")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpModernProtocolConsentTest,
	"EngineWire.Protocol.Modern.ConsentThroughClient",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpModernProtocolConsentTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using McpModernConsent::FOutcome;
	using McpModernConsent::Resolve;
	const FString Capability = TEXT("control_actor.delete");
	const TSharedPtr<FJsonObject> Args = ParseModernJson(TEXT("{\"action\":\"delete\",\"actorName\":\"Crate_01\"}"));

	FOutcome Outside = Resolve(TEXT("p1"), Capability, TEXT("elevated"), TEXT("control_actor.delete"), Args);
	TestFalse(TEXT("an older client is never asked"), Outside.Ask.IsValid() || Outside.Grant.IsValid());
	{
		const McpModern::FScope Scope(ModernToolsCall(false));
		const FOutcome NoElicitation = Resolve(TEXT("p1"), Capability, TEXT("elevated"), TEXT("control_actor.delete"), Args);
		TestFalse(TEXT("a client that cannot elicit is not asked"), NoElicitation.Ask.IsValid());
	}

	FString State;
	{
		const McpModern::FScope Scope(ModernToolsCall(true));
		const FOutcome First = Resolve(TEXT("p1"), Capability, TEXT("elevated"), TEXT("control_actor.delete"), Args);
		TestTrue(TEXT("a modern client that can elicit is asked"), First.Ask.IsValid());
		if (!First.Ask.IsValid())
		{
			return false;
		}
		TestEqual(TEXT("input_required"), First.Ask->GetStringField(TEXT("resultType")), FString(TEXT("input_required")));
		const FString Message = First.Ask->GetObjectField(TEXT("inputRequests"))->GetObjectField(TEXT("consent"))
			->GetObjectField(TEXT("params"))->GetStringField(TEXT("message"));
		TestTrue(TEXT("the question names the call and its target"),
			Message.Contains(TEXT("control_actor.delete")) && Message.Contains(TEXT("actorName=Crate_01")));
		State = First.Ask->GetStringField(TEXT("requestState"));
	}

	const auto Retry = [&](const FString& RetryState, const TSharedPtr<FJsonObject>& Responses, const TCHAR* Principal,
		const TSharedPtr<FJsonObject>& RetryArgs)
	{
		McpModern::FCall Call = ModernToolsCall(true);
		Call.RequestState = RetryState;
		Call.InputResponses = Responses;
		const McpModern::FScope Scope(Call);
		return Resolve(Principal, Capability, TEXT("elevated"), TEXT("control_actor.delete"), RetryArgs);
	};
	const FOutcome Accepted = Retry(State, ConsentAnswer(TEXT("accept"), true), TEXT("p1"), Args);
	TestTrue(TEXT("a yes becomes the grant"), Accepted.Grant.IsValid());
	if (!Accepted.Grant.IsValid())
	{
		return false;
	}
	TestEqual(TEXT("grant names the capability"), Accepted.Grant->GetStringField(TEXT("capability")), Capability);
	TestEqual(TEXT("grant carries the asked mode"), Accepted.Grant->GetStringField(TEXT("acknowledge")), FString(TEXT("elevated")));
	const FString Nonce = Accepted.Grant->GetStringField(TEXT("nonce"));
	auto& Ledger = McpCapabilityAuthorization::FMcpConsentLedger::Get();
	TestTrue(TEXT("the grant's nonce burns once"), Ledger.TryConsume(Nonce));
	TestFalse(TEXT("a replayed yes is refused by the ledger"), Ledger.TryConsume(Nonce));

	FString Tampered = State;
	Tampered[Tampered.Len() - 1] = Tampered[Tampered.Len() - 1] == TEXT('0') ? TEXT('1') : TEXT('0');
	const FOutcome Edited = Retry(Tampered, ConsentAnswer(TEXT("accept"), true), TEXT("p1"), Args);
	TestTrue(TEXT("an edited state refuses the request"), Edited.bInvalidState && !Edited.Grant.IsValid());
	TestTrue(TEXT("another principal cannot use the state"),
		Retry(State, ConsentAnswer(TEXT("accept"), true), TEXT("p2"), Args).bInvalidState);
	TestTrue(TEXT("the state is bound to the exact arguments"),
		Retry(State, ConsentAnswer(TEXT("accept"), true), TEXT("p1"),
			ParseModernJson(TEXT("{\"action\":\"delete\",\"actorName\":\"Other\"}"))).bInvalidState);
	TestTrue(TEXT("a verified state without an answer asks again"),
		Retry(State, TSharedPtr<FJsonObject>(), TEXT("p1"), Args).Ask.IsValid());
	TestTrue(TEXT("the refusal is the SDK's -32602"),
		McpModernConsent::InvalidStateError(MakeShared<FJsonValueNumber>(7)).Contains(TEXT("-32602")));
	const FOutcome Declined = Retry(State, ConsentAnswer(TEXT("decline"), false), TEXT("p1"), Args);
	TestTrue(TEXT("a no refuses with its own message"), !Declined.Declined.IsEmpty() && !Declined.Grant.IsValid());
	TestFalse(TEXT("an unticked accept is not a yes"), Retry(State, ConsentAnswer(TEXT("accept"), false), TEXT("p1"), Args).Grant.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpModernProtocolTaskResultsTest,
	"EngineWire.Protocol.Modern.TaskResults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpModernProtocolTaskResultsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpTaskResults Results;
	const FDateTime Started = FDateTime::UtcNow();
	Results.NoteRunning(TEXT("t1"), TEXT("Writer"), Started);
	FMcpTaskResults::FTask Task;
	TestFalse(TEXT("principal match is case-sensitive"), Results.Find(TEXT("t1"), TEXT("writer"), Task));
	TestTrue(TEXT("the sender reads its task"), Results.Find(TEXT("t1"), TEXT("Writer"), Task));
	TestTrue(TEXT("created at the call's start"), Task.CreatedAt == Started);
	Results.NoteDone(TEXT("t1"), MakeShared<FJsonObject>(), MakeShared<FJsonObject>());
	Results.NoteCancelled(TEXT("t1"));
	Results.Find(TEXT("t1"), TEXT("Writer"), Task);
	TestTrue(TEXT("a finished task stays finished"), Task.Outcome.IsValid() && Task.ToolResult.IsValid() && !Task.bCancelled);
	Results.NoteRunning(TEXT("t2"), TEXT("Writer"), Started);
	Results.NoteCancelled(TEXT("t2"));
	Results.Find(TEXT("t2"), TEXT("Writer"), Task);
	TestTrue(TEXT("a waiting task can be cancelled"), Task.bCancelled);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
