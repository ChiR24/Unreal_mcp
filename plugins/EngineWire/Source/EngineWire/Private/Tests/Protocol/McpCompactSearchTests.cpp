// McpCompactSearchTests.cpp — the compact search reply keeps the advice of a page with no row (nothing matched, or no row
// fits maxBytes), and drops the routine message of a page that has rows.

#include "MCP/Protocol/McpJsonRpcReplyCompaction.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpCompactSearchMessageTest,
	"EngineWire.Protocol.CompactSearchMessage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpCompactSearchMessageTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const auto Page = [](int32 Rows)
	{
		TSharedPtr<FJsonObject> Reply = MakeShared<FJsonObject>();
		Reply->SetBoolField(TEXT("success"), true);
		Reply->SetStringField(TEXT("operation"), TEXT("search"));
		Reply->SetStringField(TEXT("message"), TEXT("No capability matched 'x'. Use 2-4 plain words."));
		TArray<TSharedPtr<FJsonValue>> Results;
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			Results.Add(MakeShared<FJsonValueObject>(MakeShared<FJsonObject>()));
		}
		Reply->SetArrayField(TEXT("results"), Results);
		return McpJsonRpcReply::MakeCompactReply(Reply);
	};
	TestTrue(TEXT("a page with no row keeps its advice"), Page(0)->HasField(TEXT("message")));
	TestFalse(TEXT("a page with rows drops the routine message"), Page(2)->HasField(TEXT("message")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
