// McpNativeResultPagesTests.cpp — a result too large for one reply is paged, not refused (McpNativeResultPages.cpp).

#include "MCP/Execute/McpNativeGatewayReceipt.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
constexpr int64 PagesTestBudget = 8000;

FMcpPageOwner PagesTestOwner(const TCHAR* Principal)
{
	FMcpPageOwner Owner;
	Owner.Principal = Principal;
	Owner.CapabilityId = TEXT("asset.list");
	Owner.Fingerprint = TEXT("fp-pages");
	Owner.Params = MakeShared<FJsonObject>();
	return Owner;
}

TSharedPtr<FJsonObject> PagesTestResult(int32 Count, int32 ItemChars)
{
	TArray<TSharedPtr<FJsonValue>> Items;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetNumberField(TEXT("index"), Index);
		Item->SetStringField(TEXT("path"), FString::ChrN(ItemChars, TEXT('a')));
		Items.Add(MakeShared<FJsonValueObject>(Item));
	}
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("message"), TEXT("listed"));
	Result->SetArrayField(TEXT("assets"), Items);
	return Result;
}

int32 PagesTestNumber(const TSharedPtr<FJsonObject>& Page, const TCHAR* Field)
{
	return static_cast<int32>(Page->GetNumberField(Field));
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpNativeResultPagesReadThroughTest,
	"EngineWire.Gateway.ResultPages.ReadThrough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpNativeResultPagesReadThroughTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FMcpPageOwner Owner = PagesTestOwner(TEXT("scoped:reader"));
	FMcpResultPage Page;
	if (!TestTrue(TEXT("an oversized array pages"), McpFirstResultPage(PagesTestResult(60, 300), PagesTestBudget, Owner, Page)))
	{
		return false;
	}
	TestEqual(TEXT("the page names the cut array"), Page.Page->GetStringField(TEXT("field")), FString(TEXT("assets")));
	TestEqual(TEXT("the total is every item"), PagesTestNumber(Page.Page, TEXT("total")), 60);
	TestTrue(TEXT("the rest of the result is kept"), Page.Result->HasField(TEXT("message")));
	const FString Cursor = Page.Page->GetStringField(TEXT("cursor"));
	TestTrue(TEXT("a cursor names the next page"), !Cursor.IsEmpty() && Page.Page->HasField(TEXT("nextCall")));

	FMcpResultPage Wrong;
	TestEqual(TEXT("another principal is refused"),
		McpNextResultPage(Cursor, PagesTestOwner(TEXT("scoped:other")), PagesTestBudget, Wrong), FString(TEXT("PAGE_CURSOR_MISMATCH")));

	int32 Read = PagesTestNumber(Page.Page, TEXT("returned"));
	int32 Pages = 1;
	while (Page.Page->HasField(TEXT("cursor")) && Pages < 60)
	{
		const FString Next = Page.Page->GetStringField(TEXT("cursor"));
		if (!TestEqual(TEXT("each page follows on"), McpNextResultPage(Next, Owner, PagesTestBudget, Page), FString()))
		{
			return false;
		}
		TestEqual(TEXT("the page starts where the last ended"), PagesTestNumber(Page.Page, TEXT("offset")), Read);
		Read += PagesTestNumber(Page.Page, TEXT("returned"));
		++Pages;
	}
	TestEqual(TEXT("every item is read exactly once"), Read, 60);
	TestTrue(TEXT("it took more than one page"), Pages > 1);
	TestEqual(TEXT("a cursor read to the end is gone"),
		McpNextResultPage(Cursor, Owner, PagesTestBudget, Wrong), FString(TEXT("PAGE_CURSOR_EXPIRED")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpNativeResultPagesRefusalTest,
	"EngineWire.Gateway.ResultPages.Refusal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpNativeResultPagesRefusalTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FMcpPageOwner Owner = PagesTestOwner(TEXT("scoped:reader"));
	FMcpResultPage Page;
	TSharedPtr<FJsonObject> Text = MakeShared<FJsonObject>();
	Text->SetStringField(TEXT("log"), FString::ChrN(20000, TEXT('x')));
	TestFalse(TEXT("a result with no array is not paged"), McpFirstResultPage(Text, PagesTestBudget, Owner, Page));
	TestFalse(TEXT("an item too large for a reply is not paged"),
		McpFirstResultPage(PagesTestResult(3, 9000), PagesTestBudget, Owner, Page));
	TestEqual(TEXT("a cursor never issued is refused"),
		McpNextResultPage(TEXT("never-issued"), Owner, PagesTestBudget, Page), FString(TEXT("PAGE_CURSOR_EXPIRED")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
