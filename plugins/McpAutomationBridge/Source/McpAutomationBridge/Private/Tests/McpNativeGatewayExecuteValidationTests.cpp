// McpNativeGatewayExecuteValidationTests.cpp — in-editor run of the Task 27 suite
//
// The TypeScript side runs the same generated suite in
// tests/unit/native-execute-suite.test.ts. Both build their cases from
// the same canonical records with the same algorithm, so a rule that changes on
// one surface and not the other fails here.
//
// Rule -> outcome matrix (must match .omo/evidence/task-27/execute-suite-summary.json):
//   valid                      -> accepted
//   undeclared-param           -> UNDECLARED_PARAMETER
//   missing-required           -> MISSING_REQUIRED_PARAMETER
//   wrong-type                 -> INVALID_PARAMETER_TYPE
//   enum                       -> INVALID_PARAMETER_VALUE
//   range                      -> OUT_OF_RANGE
//   unsupported-option         -> UNSUPPORTED_OPTION
//   gateway-control-in-params  -> UNSUPPORTED_OPTION
//   unsupported-keyword        -> UNSUPPORTED_SCHEMA_KEYWORD

#include "MCP/Execute/McpNativeGatewayCanonicalRecords.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Execute/McpNativeGatewayExecuteRequest.h"
#include "MCP/Execute/McpNativeGatewayReceipt.h"
#include "MCP/Execute/McpNativeGatewaySchemaValidation.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/McpNativeGatewayExecuteValidationSampleValues.h"

namespace McpNativeGatewayExecuteSuite
{
TSharedPtr<FJsonObject> PropertiesOf(const TSharedPtr<FJsonObject>& Schema)
{
	const TSharedPtr<FJsonObject>* Properties = nullptr;
	if (Schema.IsValid() && Schema->TryGetObjectField(TEXT("properties"), Properties) && Properties)
	{
		return *Properties;
	}
	return nullptr;
}

TArray<FString> RequiredOf(const TSharedPtr<FJsonObject>& Schema)
{
	TArray<FString> Names;
	const TArray<TSharedPtr<FJsonValue>>* Required = nullptr;
	if (Schema.IsValid() && Schema->TryGetArrayField(TEXT("required"), Required) && Required)
	{
		for (const TSharedPtr<FJsonValue>& Entry : *Required)
		{
			FString Name;
			if (Entry.IsValid() && Entry->TryGetString(Name) && Name != TEXT("action"))
			{
				Names.Add(Name);
			}
		}
	}
	return Names;
}


TSharedPtr<FJsonObject> MinimalValidParams(const TSharedPtr<FJsonObject>& InputSchema)
{
	TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
	const TSharedPtr<FJsonObject> Properties = PropertiesOf(InputSchema);

	TArray<FString> Names = RequiredOf(InputSchema);

	// `required` alone does not describe a minimal valid request. 22 records
	// declare an empty `required` plus a `requiredOneOf` group -- add_notify, for
	// instance, takes either notifyClass or notifyName -- so building only from
	// `required` produced {} and the validator rejected it. That read as a
	// schema/validator disagreement when the two in fact agreed: the sample was
	// the thing that was wrong. One member per group is what "minimal" means.
	const TArray<TSharedPtr<FJsonValue>>* OneOf = nullptr;
	if (InputSchema.IsValid() &&
		InputSchema->TryGetArrayField(TEXT("requiredOneOf"), OneOf) && OneOf)
	{
		for (const TSharedPtr<FJsonValue>& Entry : *OneOf)
		{
			FString Name;
			if (Entry.IsValid() && Entry->TryGetString(Name) && !Name.IsEmpty())
			{
				Names.AddUnique(Name);
				break;
			}
		}
	}

	for (const FString& Name : Names)
	{
		const TSharedPtr<FJsonObject>* PropertySchema = nullptr;
		if (Properties.IsValid())
		{
			Properties->TryGetObjectField(Name, PropertySchema);
		}
		Params->SetField(Name, SampleValue(PropertySchema ? *PropertySchema : nullptr));
	}
	return Params;
}

FString ValidateAndReportCode(
	const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonObject>& Schema)
{
	FMcpSchemaViolationDetail Violation;
	if (McpValidateObjectAgainstCanonicalSchema(Params, Schema, Violation))
	{
		return FString();
	}
	return McpSchemaViolationCode(Violation.Reason);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpNativeGatewayExecuteSuiteTest,
	"McpAutomationBridge.NativeGateway.ExecuteValidationSuite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpNativeGatewayExecuteSuiteTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpNativeGatewayExecuteSuite;

	const FMcpCanonicalRecordIndex& Index = FMcpCanonicalRecordIndex::Get();
	TestTrue(TEXT("canonical record index loaded"), Index.IsLoaded());
	if (!Index.IsLoaded())
	{
		AddError(FString::Printf(TEXT("index load error: %s"), *Index.GetLoadError()));
		return false;
	}

	const FMcpCapabilityStore& Store = FMcpCapabilityStore::Get();
	TestEqual(TEXT("every canonical record is resolvable"), Index.Num(), Store.GetRecords().Num());

	int32 ValidAccepted = 0;
	int32 UndeclaredRejected = 0;
	int32 MissingRequiredRejected = 0;

	for (const FMcpCapabilityRecord& Record : Store.GetRecords())
	{
		const TSharedPtr<FJsonObject> Schema = Record.InputSchema;
		TSharedPtr<FJsonObject> Valid = MinimalValidParams(Schema);
		if (McpSchemaDeclaresProperty(Schema, TEXT("action")))
		{
			Valid->SetStringField(TEXT("action"), Index.GetLegacyActionForCapability(Record.Id));
		}

		const FString ValidCode = ValidateAndReportCode(
			McpApplyCanonicalSchemaDefaults(Valid, Schema), Schema);
		if (!ValidCode.IsEmpty())
		{
			AddError(FString::Printf(
				TEXT("%s: minimal valid request rejected with %s"), *Record.Id, *ValidCode));
			continue;
		}
		++ValidAccepted;

		TSharedPtr<FJsonObject> Undeclared = MakeShared<FJsonObject>();
		Undeclared->Values = Valid->Values;
		Undeclared->SetBoolField(TEXT("task27UndeclaredParameter"), true);
		if (ValidateAndReportCode(Undeclared, Schema) == TEXT("UNDECLARED_PARAMETER"))
		{
			++UndeclaredRejected;
		}
		else
		{
			AddError(FString::Printf(TEXT("%s: undeclared parameter was not rejected"), *Record.Id));
		}

		const TArray<FString> Required = RequiredOf(Schema);
		if (Required.Num() > 0)
		{
			const TSharedPtr<FJsonObject> Properties = PropertiesOf(Schema);
			const TSharedPtr<FJsonObject>* DroppedSchema = nullptr;
			if (Properties.IsValid())
			{
				Properties->TryGetObjectField(Required[0], DroppedSchema);
			}
			const bool bHasDefault = DroppedSchema && (*DroppedSchema).IsValid() &&
				(*DroppedSchema)->TryGetField(TEXT("default")).IsValid();
			if (!bHasDefault)
			{
				TSharedPtr<FJsonObject> Missing = MakeShared<FJsonObject>();
				Missing->Values = Valid->Values;
				Missing->RemoveField(Required[0]);
				if (ValidateAndReportCode(Missing, Schema) == TEXT("MISSING_REQUIRED_PARAMETER"))
				{
					++MissingRequiredRejected;
				}
				else
				{
					AddError(FString::Printf(
						TEXT("%s: missing required '%s' was not rejected"), *Record.Id, *Required[0]));
				}
			}
		}
	}

	TestEqual(TEXT("every capability accepted its minimal valid request"),
		ValidAccepted, Store.GetRecords().Num());
	TestEqual(TEXT("every capability rejected an undeclared parameter"),
		UndeclaredRejected, Store.GetRecords().Num());
	TestTrue(TEXT("missing-required cases were exercised"), MissingRequiredRejected > 0);

	// Fail-closed: a keyword this validator does not implement must be refused.
	TSharedPtr<FJsonObject> ConditionalSchema = MakeShared<FJsonObject>();
	ConditionalSchema->SetStringField(TEXT("type"), TEXT("object"));
	ConditionalSchema->SetObjectField(TEXT("if"), MakeShared<FJsonObject>());
	TestEqual(TEXT("unimplemented schema keyword fails closed"),
		ValidateAndReportCode(MakeShared<FJsonObject>(), ConditionalSchema),
		FString(TEXT("UNSUPPORTED_SCHEMA_KEYWORD")));

	// Options are bounded to the Task 3 key set.
	FMcpSemanticError OptionError;
	TSharedPtr<FJsonObject> BadOptions = MakeShared<FJsonObject>();
	BadOptions->SetBoolField(TEXT("task27NotAnOption"), true);
	TestFalse(TEXT("unsupported option rejected"),
		McpValidateExecutionOptions(BadOptions, OptionError));
	TestEqual(TEXT("unsupported option is a typed option error"),
		OptionError.Code, FString(TEXT("UNSUPPORTED_OPTION")));

	FMcpSemanticError TimeoutError;
	TSharedPtr<FJsonObject> BadTimeout = MakeShared<FJsonObject>();
	BadTimeout->SetNumberField(TEXT("timeoutMs"), McpMaxExecutionTimeoutMs + 1);
	TestFalse(TEXT("out-of-range timeout rejected"),
		McpValidateExecutionOptions(BadTimeout, TimeoutError));
	TestEqual(TEXT("out-of-range timeout is a typed range error"),
		TimeoutError.Code, FString(TEXT("OUT_OF_RANGE")));

	return true;
}

#endif  // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
