// Behavioral automation coverage for McpCoerceCanonicalVectorShapes. The TS<->C++ parity
// test (tests/unit/tools/vector-shape-coercion-parity.test.ts) pins call order and the
// predicate bodies as source text; these tests pin the native BEHAVIOR, so weakening a
// guard fails here even when the pinned fragments survive a refactor.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "MCP/Execute/McpNativeGatewaySchemaValidation.h"

namespace
{
	TSharedPtr<FJsonObject> ParseJsonObject(const FString& Text)
	{
		TSharedPtr<FJsonObject> Object;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		FJsonSerializer::Deserialize(Reader, Object);
		return Object;
	}

	// A schema whose "location" declares an {x, y, z} object — the shape behind the 74
	// served sites the regression fixtures model (create_light/location and friends).
	TSharedPtr<FJsonObject> XyzSchema()
	{
		return ParseJsonObject(TEXT(
			R"({"type":"object","properties":{"location":{"type":"object","properties":{)"
			R"("x":{"type":"number"},"y":{"type":"number"},"z":{"type":"number"}}}}})"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpGatewayVectorCoercionShadowedShortArrayTest,
	"McpAutomationBridge.Gateway.VectorCoercion.ShadowedShortArrayKeepsShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpGatewayVectorCoercionShadowedShortArrayTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const TSharedPtr<FJsonObject> Schema = XyzSchema();

	// Positive control first: if coercion never runs, every "stays an array" assert below
	// would pass vacuously. A full-length array must become the declared object.
	{
		const TSharedPtr<FJsonObject> Params = ParseJsonObject(TEXT(R"({"location":[1,2,3]})"));
		const TSharedPtr<FJsonObject> Out = McpCoerceCanonicalVectorShapes(Params, Schema);
		const TSharedPtr<FJsonObject>* Coerced = nullptr;
		if (TestTrue(TEXT("[1,2,3] coerces to the declared {x,y,z} object"),
			Out.IsValid() && Out->TryGetObjectField(TEXT("location"), Coerced) && Coerced))
		{
			TestEqual(TEXT("x"), (*Coerced)->GetNumberField(TEXT("x")), 1.0);
			TestEqual(TEXT("y"), (*Coerced)->GetNumberField(TEXT("y")), 2.0);
			TestEqual(TEXT("z"), (*Coerced)->GetNumberField(TEXT("z")), 3.0);
		}
	}

	// The regression: a two-number array matches the trailing ['x','y'] key set unless that
	// set is recognised as shadowed by the declared {x,y,z}. It must come back unchanged —
	// a fabricated {x:0,y:-980} here is how (0,-980,0) once shipped with success:true.
	{
		const TSharedPtr<FJsonObject> Params = ParseJsonObject(TEXT(R"({"location":[0,-980]})"));
		const TSharedPtr<FJsonObject> Out = McpCoerceCanonicalVectorShapes(Params, Schema);
		const TSharedPtr<FJsonValue> Field = Out.IsValid() ? Out->TryGetField(TEXT("location")) : nullptr;
		if (TestTrue(TEXT("two-number array survives untouched"), Field.IsValid()))
		{
			TestTrue(TEXT("still an array (no fabricated {x,y})"), Field->Type == EJson::Array);
			if (Field->Type == EJson::Array)
			{
				TestEqual(TEXT("length unchanged"), Field->AsArray().Num(), 2);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpGatewayVectorCoercionNonNumericArrayTest,
	"McpAutomationBridge.Gateway.VectorCoercion.NonNumericArrayKeepsShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpGatewayVectorCoercionNonNumericArrayTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const TSharedPtr<FJsonObject> Schema = XyzSchema();

	// AsNumber() reads "a" as 0: without the all-finite-numbers guard these two shipped as
	// (0,0,0) and (0,10,-100). Both must keep their array shape so validation can refuse them.
	const TCHAR* Cases[] = {
		TEXT(R"({"location":["a","b","c"]})"),
		TEXT(R"({"location":["0","10","-100"]})"),
	};
	for (const TCHAR* Case : Cases)
	{
		const TSharedPtr<FJsonObject> Params = ParseJsonObject(Case);
		const TSharedPtr<FJsonObject> Out = McpCoerceCanonicalVectorShapes(Params, Schema);
		const TSharedPtr<FJsonValue> Field = Out.IsValid() ? Out->TryGetField(TEXT("location")) : nullptr;
		if (TestTrue(FString::Printf(TEXT("%s: field survives"), Case), Field.IsValid()))
		{
			TestTrue(FString::Printf(TEXT("%s: still an array (no AsNumber fabrication)"), Case),
				Field->Type == EJson::Array);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpGatewayVectorCoercionRotationOrderTest,
	"McpAutomationBridge.Gateway.VectorCoercion.RotationArrayPrefersPitchYawRoll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpGatewayVectorCoercionRotationOrderTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// A rotation schema that also declares x/y/z aliases: [10,20,30] must read as
	// {pitch,yaw,roll} — handlers reading only pitch/yaw/roll once got a zero rotation
	// while the reply said rotationApplied:true.
	const TSharedPtr<FJsonObject> Schema = ParseJsonObject(TEXT(
		R"({"type":"object","properties":{"rotation":{"type":"object","properties":{)"
		R"("pitch":{"type":"number"},"yaw":{"type":"number"},"roll":{"type":"number"},)"
		R"("x":{"type":"number"},"y":{"type":"number"},"z":{"type":"number"}}}}})"));
	const TSharedPtr<FJsonObject> Params = ParseJsonObject(TEXT(R"({"rotation":[10,20,30]})"));
	const TSharedPtr<FJsonObject> Out = McpCoerceCanonicalVectorShapes(Params, Schema);
	const TSharedPtr<FJsonObject>* Coerced = nullptr;
	if (TestTrue(TEXT("[10,20,30] coerces to an object"),
		Out.IsValid() && Out->TryGetObjectField(TEXT("rotation"), Coerced) && Coerced))
	{
		TestEqual(TEXT("pitch"), (*Coerced)->GetNumberField(TEXT("pitch")), 10.0);
		TestEqual(TEXT("yaw"), (*Coerced)->GetNumberField(TEXT("yaw")), 20.0);
		TestEqual(TEXT("roll"), (*Coerced)->GetNumberField(TEXT("roll")), 30.0);
		TestFalse(TEXT("no x alias written"), (*Coerced)->HasField(TEXT("x")));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
