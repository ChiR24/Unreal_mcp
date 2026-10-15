#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Modules/ModuleManager.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "Domains/Niagara/McpNiagaraPrivate.h"
#include "Domains/NiagaraAuthoring/McpNiagaraEmitterName.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpNiagaraEmitterNameTest,
	"EngineWire.Niagara.CreateEmitter.KeepsName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpNiagaraEmitterNameTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
#if MCP_HAS_NIAGARA_EMITTER_FACTORY_NEW
	// A name ending in "_<number>" is the case that used to be renamed; the leading-zero and plain names never were.
	const TCHAR* Names[] = { TEXT("NE_McpNameProbe_0"), TEXT("NE_McpNameProbe_7"), TEXT("NE_McpNameProbe_01"), TEXT("NE_McpNameProbe") };
	const FString Root = FString::Printf(TEXT("/Temp/McpEmitterName_%s/"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	FModuleManager::Get().LoadModule(TEXT("NiagaraEditor"));
	for (const TCHAR* Name : Names)
	{
		// The same steps create_niagara_emitter takes, up to naming the emitter.
		UPackage* Package = CreatePackage(*(Root + Name));
		UNiagaraEmitter* Emitter = NewObject<UNiagaraEmitter>(Package, FName(Name), RF_Public | RF_Standalone | RF_Transactional);
		UNiagaraEmitterFactoryNew::InitializeEmitter(Emitter, true);
		McpNameNewEmitterAsset(*Emitter);

		TestEqual(FString::Printf(TEXT("'%s' keeps its object name"), Name), Emitter->GetName(), FString(Name));
		TArray<UObject*> InPackage;
		GetObjectsWithOuter(Package, InPackage, false);
		int32 Redirectors = 0;
		for (const UObject* Object : InPackage)
		{
			Redirectors += Object && Object->IsA<UObjectRedirector>() ? 1 : 0;
		}
		TestEqual(FString::Printf(TEXT("'%s' leaves no redirector in its package"), Name), Redirectors, 0);

		for (UObject* Object : InPackage)
		{
			Object->ClearFlags(RF_Public | RF_Standalone);
			Object->MarkAsGarbage();
		}
		Package->MarkAsGarbage();
	}
#else
	AddInfo(TEXT("create_niagara_emitter names the emitter only where UNiagaraEmitterFactoryNew exists (UE 5.1+)."));
#endif
	return true;
}

#endif
