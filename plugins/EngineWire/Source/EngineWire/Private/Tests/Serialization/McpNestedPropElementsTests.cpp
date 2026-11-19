// McpNestedPropElementsTests.cpp — an element path (inspect get_property / set_property) reaches one entry of a list by
// index or by a field of it, a map value by its key, refuses a case-only, ambiguous or out-of-range pick with its own
// code, and names the chain from the member down for the change event.

#include "Foundation/BridgeHelpers/Properties/McpNestedPropElements.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/UserDefinedEnum.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpNestedPropElementsTest,
	"EngineWire.Foundation.NestedPropElements",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpNestedPropElementsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UMaterialInstanceConstant* Instance = NewObject<UMaterialInstanceConstant>(GetTransientPackage());
	for (const TCHAR* Name : {TEXT("Roughness"), TEXT("Metallic")})
	{
		FScalarParameterValue Value;
		Value.ParameterInfo.Name = Name;
		Value.ParameterValue = 0.25f;
		Instance->ScalarParameterValues.Add(Value);
	}
	const auto ValueOf = [](const FMcpElementPath& Path) { return Path.Property->ContainerPtrToValuePtr<float>(Path.Container); };

	FMcpElementPath Path;
	TestTrue(TEXT("an entry by a field of it"), McpResolveElementPath(Instance,
		TEXT("ScalarParameterValues[ParameterInfo.Name=Metallic].ParameterValue"), true, Path));
	TestTrue(TEXT("it is the second entry's value"), ValueOf(Path) == &Instance->ScalarParameterValues[1].ParameterValue);
	TestEqual(TEXT("the chain runs list, entry, leaf"), Path.Chain.Num(), 3);
	TestTrue(TEXT("an entry by index"), McpResolveElementPath(Instance, TEXT("ScalarParameterValues[0].ParameterValue"), false, Path)
		&& ValueOf(Path) == &Instance->ScalarParameterValues[0].ParameterValue);

	TestFalse(TEXT("a case-only match is a miss"), McpResolveElementPath(Instance,
		TEXT("ScalarParameterValues[ParameterInfo.Name=metallic].ParameterValue"), false, Path));
	TestEqual(TEXT("not found"), Path.Code, FString(TEXT("PROPERTY_ELEMENT_NOT_FOUND")));
	TestTrue(TEXT("the miss names the case-only entry"), Path.Error.Contains(TEXT("Metallic")));
	TestFalse(TEXT("an index past the end"), McpResolveElementPath(Instance, TEXT("ScalarParameterValues[5].ParameterValue"), false, Path));
	TestFalse(TEXT("an open bracket"), McpResolveElementPath(Instance, TEXT("ScalarParameterValues[0.ParameterValue"), false, Path));
	TestEqual(TEXT("is no path"), Path.Code, FString(TEXT("PROPERTY_NOT_FOUND")));
	Instance->ScalarParameterValues[1].ParameterInfo.Name = TEXT("Roughness");
	TestFalse(TEXT("two matches"), McpResolveElementPath(Instance,
		TEXT("ScalarParameterValues[ParameterInfo.Name=Roughness].ParameterValue"), true, Path));
	TestEqual(TEXT("are ambiguous"), Path.Code, FString(TEXT("AMBIGUOUS_KEY_MATCH")));

	UUserDefinedEnum* Enum = NewObject<UUserDefinedEnum>(GetTransientPackage());
	Enum->DisplayNameMap.Add(TEXT("Boss"), FText::FromString(TEXT("Big Boss")));
	TestTrue(TEXT("a map value by its key"), McpResolveElementPath(Enum, TEXT("DisplayNameMap[Boss]"), true, Path)
		&& Path.Property->ContainerPtrToValuePtr<FText>(Path.Container)->ToString() == TEXT("Big Boss"));
	TestEqual(TEXT("a path's head"), McpPathHead(TEXT("Items[2].Damage")), FString(TEXT("Items")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
