// McpConsoleVariablesTests.cpp — known answers for get_console_variables' credential guard (McpConsoleVariables.cpp).

#include "Domains/ConsoleCommand/McpConsolePrivate.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpConsoleVariablesTest,
	"EngineWire.Console.Variables",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpConsoleVariablesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TestTrue(TEXT("a key at the end"), McpConsoleVariables::NamesACredential(TEXT("net.EncryptionKey")));
	TestTrue(TEXT("a key after an acronym"), McpConsoleVariables::NamesACredential(TEXT("Analytics.APIKey")));
	TestTrue(TEXT("an auth token"), McpConsoleVariables::NamesACredential(TEXT("Online.AuthToken")));
	TestTrue(TEXT("a word between underscores"), McpConsoleVariables::NamesACredential(TEXT("http.proxy_password")));
	TestTrue(TEXT("a plural"), McpConsoleVariables::NamesACredential(TEXT("s.Secrets")));
	TestFalse(TEXT("a keyboard is no key"), McpConsoleVariables::NamesACredential(TEXT("Slate.EnableKeyboardNavigation")));
	TestFalse(TEXT("keep is no key"), McpConsoleVariables::NamesACredential(TEXT("r.Shaders.KeepDebugInfo")));
	TestFalse(TEXT("an ordinary variable"), McpConsoleVariables::NamesACredential(TEXT("r.ScreenPercentage")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
