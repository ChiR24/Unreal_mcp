#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Domains/NiagaraAuthoring/McpNiagaraCreatePackage.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpNiagaraCreatePackageNameTest,
	"EngineWire.Niagara.CreatePackage.Names",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpNiagaraCreatePackageNameTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// The package a create action may write: the folder and the name it was given, normalized.
	struct FAccepted { const TCHAR* Folder; const TCHAR* Name; const TCHAR* Package; };
	const FAccepted Accepted[] = {
		{ TEXT("/Game/FX"), TEXT("NE_Spark"), TEXT("/Game/FX/NE_Spark") },
		{ TEXT("/Game/FX/"), TEXT("NE_Spark"), TEXT("/Game/FX/NE_Spark") },
		{ TEXT("/Game"), TEXT("NE_Spark"), TEXT("/Game/NE_Spark") },
		{ TEXT("Game/FX"), TEXT("NE_Spark"), TEXT("/Game/FX/NE_Spark") },
	};
	for (const FAccepted& Case : Accepted)
	{
		FString Error;
		const FString Package = McpNiagaraCreatePackageName(Case.Folder, Case.Name, Error);
		TestEqual(FString::Printf(TEXT("'%s' + '%s'"), Case.Folder, Case.Name), Package, FString(Case.Package));
		TestTrue(FString::Printf(TEXT("'%s' + '%s' has no error"), Case.Folder, Case.Name), Error.IsEmpty());
	}

	// A folder with a space used to reach CreatePackage and make a package that cannot be saved (the emitter still
	// answered "Emitter created."); the others are not a writable content folder.
	const TCHAR* Refused[] = {
		TEXT("/Game/My FX"),
		TEXT("C:/Program Files/Git/Game/FX"),
		TEXT("/Game/FX/../Other"),
		TEXT("/Script/FX"),
		TEXT("/McpNoSuchRoot/FX"),
	};
	for (const TCHAR* Folder : Refused)
	{
		FString Error;
		const FString Package = McpNiagaraCreatePackageName(Folder, TEXT("NE_Spark"), Error);
		TestTrue(FString::Printf(TEXT("'%s' is refused (got '%s')"), Folder, *Package), Package.IsEmpty());
		TestTrue(FString::Printf(TEXT("'%s' says nothing was created"), Folder), Error.EndsWith(TEXT("Nothing was created.")));
	}
	return true;
}

#endif
