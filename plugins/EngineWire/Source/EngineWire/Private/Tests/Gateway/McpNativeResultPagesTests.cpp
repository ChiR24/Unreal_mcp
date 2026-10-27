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
	Owner.Call = MakeShared<FJsonObject>();
	Owner.Call->SetStringField(TEXT("tool"), TEXT("manage_asset"));
	Owner.Call->SetStringField(TEXT("action"), TEXT("list"));
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
	TestEqual(TEXT("the next call repeats the call's own routing"),
		Page.Page->GetObjectField(TEXT("nextCall"))->GetStringField(TEXT("action")), FString(TEXT("list")));

	FMcpResultPage Wrong;
	TestEqual(TEXT("another principal is refused"),
		McpNextResultPage(Cursor, PagesTestOwner(TEXT("scoped:other")), PagesTestBudget, Wrong), FString(TEXT("PAGE_CURSOR_MISMATCH")));

	const int32 FirstReturned = PagesTestNumber(Page.Page, TEXT("returned"));
	int32 Read = FirstReturned;
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
	// A cursor that moved its result on read the page after it the second time: a replayed first receipt skipped one.
	TestEqual(TEXT("an earlier cursor reads its page again"), McpNextResultPage(Cursor, Owner, PagesTestBudget, Wrong), FString());
	TestEqual(TEXT("from the same offset"), PagesTestNumber(Wrong.Page, TEXT("offset")), FirstReturned);
	FString Id;
	Cursor.Split(TEXT(":"), &Id, nullptr, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	for (const TCHAR* Forged : {TEXT(":0"), TEXT(":60"), TEXT(":-1"), TEXT(":01"), TEXT(":1x"), TEXT("")})
	{
		TestEqual(TEXT("an offset no page issued is refused"),
			McpNextResultPage(Id + Forged, Owner, PagesTestBudget, Wrong), FString(TEXT("PAGE_CURSOR_EXPIRED")));
	}
	// An offset inside the result that no page handed out read a page overlapping the first.
	const FString Inside = Id + TEXT(":") + FString::FromInt(static_cast<int32>(FirstReturned) - 1);
	TestEqual(TEXT("an inside offset no page issued is refused"),
		McpNextResultPage(Inside, Owner, PagesTestBudget, Wrong), FString(TEXT("PAGE_CURSOR_EXPIRED")));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpNativeResultPagesImageOnceTest,
	"EngineWire.Gateway.ResultPages.ImageOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// An image kept with the rest went out again with every page and, against the flat later-page budget, left room for
// one item per page.
bool FMcpNativeResultPagesImageOnceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FMcpPageOwner Owner = PagesTestOwner(TEXT("scoped:reader"));
	const TSharedPtr<FJsonObject> Result = PagesTestResult(60, 300);
	Result->SetStringField(TEXT("imageBase64"), FString::ChrN(6000, TEXT('i')));
	FMcpResultPage Page;
	if (!TestTrue(TEXT("a result with an image pages"), McpFirstResultPage(Result, PagesTestBudget + 6000, Owner, Page)))
	{
		return false;
	}
	TestTrue(TEXT("the first page carries the image"), Page.Result->HasField(TEXT("imageBase64")));
	const int32 FirstReturned = PagesTestNumber(Page.Page, TEXT("returned"));
	TestEqual(TEXT("the next page follows on"), McpNextResultPage(Page.Page->GetStringField(TEXT("cursor")), Owner, PagesTestBudget, Page), FString());
	TestFalse(TEXT("later pages leave the image out"), Page.Result->HasField(TEXT("imageBase64")));
	TestTrue(TEXT("later pages keep their whole budget"), PagesTestNumber(Page.Page, TEXT("returned")) >= FirstReturned);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
