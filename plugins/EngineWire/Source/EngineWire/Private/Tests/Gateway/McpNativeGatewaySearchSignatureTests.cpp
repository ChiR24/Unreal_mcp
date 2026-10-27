// McpNativeGatewaySearchSignatureTests.cpp — a clear first search hit carries its signature and execute call, so a
// caller can skip describe; never one that needs consent, never past the first page (McpNativeGatewaySearch.cpp).

#include "MCP/Gateway/McpNativeGatewaySearch.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
TSharedPtr<FJsonObject> SignatureTestFirstRow(const TCHAR* Query, int32 Offset)
{
	FMcpDiscoveryQuery Input;
	Input.Query = Query;
	Input.Offset = Offset;
	const TSharedPtr<FJsonObject> Out =
		McpGatewaySearchCapabilities(Input, FMcpCapabilityStore::Get(), [](const FString&) { return true; });
	const TArray<TSharedPtr<FJsonValue>>* Results = nullptr;
	if (!Out.IsValid() || !Out->TryGetArrayField(TEXT("results"), Results) || !Results || Results->Num() == 0)
	{
		return nullptr;
	}
	return (*Results)[0]->AsObject();
}

const TSharedPtr<FJsonObject>* SignatureTestObject(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
{
	const TSharedPtr<FJsonObject>* Found = nullptr;
	return Object.IsValid() && Object->TryGetObjectField(Field, Found) && Found ? Found : nullptr;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpNativeSearchSignatureTest,
	"EngineWire.Gateway.SearchSignature",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpNativeSearchSignatureTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (!TestTrue(TEXT("capability store is ready"), FMcpCapabilityStore::Get().IsReady()))
	{
		return false;
	}
	const TSharedPtr<FJsonObject> Row = SignatureTestFirstRow(TEXT("save level"), 0);
	const TSharedPtr<FJsonObject>* Signature = SignatureTestObject(Row, TEXT("signature"));
	if (!TestNotNull(TEXT("a clear hit carries a signature"), Signature))
	{
		return false;
	}
	const TSharedPtr<FJsonObject>* Selector = SignatureTestObject(*Signature, TEXT("selector"));
	TestTrue(TEXT("a family names its selector"), Selector && (*Selector)->GetStringField(TEXT("name")) == TEXT("saveMode"));
	const TSharedPtr<FJsonObject>* Example = SignatureTestObject(*Signature, TEXT("example"));
	TestTrue(TEXT("the example is params, without action"), Example && !(*Example)->HasField(TEXT("action")));
	const TSharedPtr<FJsonObject>* Execute = SignatureTestObject(Row, TEXT("executeCall"));
	TestTrue(TEXT("the execute call names the capability"), Execute &&
		(*Execute)->GetStringField(TEXT("operation")) == TEXT("execute") &&
		(*Execute)->GetStringField(TEXT("capability")) == Row->GetStringField(TEXT("capability")));

	TestNull(TEXT("never past the first page"), SignatureTestObject(SignatureTestFirstRow(TEXT("save level"), 1), TEXT("signature")));
	TestNull(TEXT("never for a capability that needs consent"),
		SignatureTestObject(SignatureTestFirstRow(TEXT("delete asset"), 0), TEXT("signature")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
