// McpNativeGatewayExecuteValidationSampleValues.h — synthesizing a schema-shaped
// sample value for the Task 27 execute-validation suite.
//
// Split out of McpNativeGatewayExecuteValidationTests.cpp so both stay under the
// 250 pure-line ceiling. This half answers one question: given a declared
// property schema, what is the smallest value that satisfies it? The suite file
// keeps the rule -> outcome matrix and the test bodies.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace McpNativeGatewayExecuteSuite
{
TSharedPtr<FJsonObject> PropertiesOf(const TSharedPtr<FJsonObject>& Schema);
TArray<FString> RequiredOf(const TSharedPtr<FJsonObject>& Schema);
TSharedPtr<FJsonValue> SampleValue(const TSharedPtr<FJsonObject>& PropertySchema);

/** `type` as a bare string, or the first entry when it is declared as a union. */
inline FString FirstDeclaredType(const TSharedPtr<FJsonObject>& PropertySchema)
{
	FString Declared;
	if (PropertySchema.IsValid() && PropertySchema->TryGetStringField(TEXT("type"), Declared))
	{
		return Declared;
	}
	const TArray<TSharedPtr<FJsonValue>>* Types = nullptr;
	if (PropertySchema.IsValid() && PropertySchema->TryGetArrayField(TEXT("type"), Types) &&
		Types && Types->Num() > 0)
	{
		(*Types)[0]->TryGetString(Declared);
	}
	return Declared;
}

/**
 * Smallest value satisfying PropertySchema.
 *
 * `default` and `enum` win over a synthesized value, because a schema that names
 * an acceptable value has already answered the question. Numeric bounds are
 * honoured so the sample cannot trip OUT_OF_RANGE, and maxLength truncates the
 * string sample for the same reason.
 */
inline TSharedPtr<FJsonValue> SampleValue(const TSharedPtr<FJsonObject>& PropertySchema)
{
	if (!PropertySchema.IsValid())
	{
		return MakeShared<FJsonValueString>(TEXT("sample"));
	}
	const TSharedPtr<FJsonValue> Default = PropertySchema->TryGetField(TEXT("default"));
	if (Default.IsValid())
	{
		return Default;
	}
	const TArray<TSharedPtr<FJsonValue>>* Allowed = nullptr;
	if (PropertySchema->TryGetArrayField(TEXT("enum"), Allowed) && Allowed && Allowed->Num() > 0)
	{
		return (*Allowed)[0];
	}

	const FString Declared = FirstDeclaredType(PropertySchema);
	if (Declared == TEXT("boolean"))
	{
		return MakeShared<FJsonValueBoolean>(true);
	}
	if (Declared == TEXT("number") || Declared == TEXT("integer"))
	{
		double Minimum = 0.0;
		double Maximum = 0.0;
		const bool bHasMin = PropertySchema->TryGetNumberField(TEXT("minimum"), Minimum);
		const bool bHasMax = PropertySchema->TryGetNumberField(TEXT("maximum"), Maximum);
		double Value = bHasMin ? FMath::Max(Minimum, 1.0) : 1.0;
		if (bHasMax) Value = FMath::Min(Value, Maximum);
		if (bHasMin) Value = FMath::Max(Value, Minimum);
		return MakeShared<FJsonValueNumber>(Value);
	}
	if (Declared == TEXT("array"))
	{
		const TSharedPtr<FJsonObject>* ItemSchema = nullptr;
		PropertySchema->TryGetObjectField(TEXT("items"), ItemSchema);
		double MinItems = 0.0;
		PropertySchema->TryGetNumberField(TEXT("minItems"), MinItems);
		const int32 Count = FMath::Max(static_cast<int32>(MinItems), 1);
		TArray<TSharedPtr<FJsonValue>> Items;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Items.Add(SampleValue(ItemSchema ? *ItemSchema : nullptr));
		}
		return MakeShared<FJsonValueArray>(Items);
	}
	if (Declared == TEXT("object"))
	{
		TSharedPtr<FJsonObject> Nested = MakeShared<FJsonObject>();
		const TSharedPtr<FJsonObject> NestedProperties = PropertiesOf(PropertySchema);
		for (const FString& Name : RequiredOf(PropertySchema))
		{
			const TSharedPtr<FJsonObject>* Child = nullptr;
			if (NestedProperties.IsValid())
			{
				NestedProperties->TryGetObjectField(Name, Child);
			}
			Nested->SetField(Name, SampleValue(Child ? *Child : nullptr));
		}
		return MakeShared<FJsonValueObject>(Nested);
	}
	if (Declared == TEXT("null"))
	{
		return MakeShared<FJsonValueNull>();
	}

	FString MaxLengthValue = TEXT("/Game/Task27/Sample");
	double MaxLength = 0.0;
	if (PropertySchema->TryGetNumberField(TEXT("maxLength"), MaxLength) &&
		MaxLengthValue.Len() > static_cast<int32>(MaxLength))
	{
		MaxLengthValue = MaxLengthValue.Left(static_cast<int32>(MaxLength));
	}
	return MakeShared<FJsonValueString>(MaxLengthValue);
}
} // namespace McpNativeGatewayExecuteSuite
