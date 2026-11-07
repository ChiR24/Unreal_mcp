#include "MCP/Execute/Request/McpNativeGatewayStringLimits.h"

#include "MCP/Execute/McpNativeGatewaySchemaKeywords.h"

namespace
{
// The pre-queue gate's nesting bound (McpPrequeueGate.cpp): no payload nested deeper reaches the queue, so refusing
// it here narrows nothing and keeps this walk, which follows reflection-boundary interiors, off a hostile nest's stack.
constexpr int32 StringLimitsMaxDepth = 24;

bool RefuseStringLimit(const FString& Pointer, const FString& Message, FMcpSchemaViolationDetail& OutViolation)
{
	OutViolation = McpSchemaKeywords::MakeViolation(EMcpSchemaViolation::Range, Pointer, Message);
	return false;
}

// Schema is null where none reaches (a reflection-boundary interior, the members of an untyped value): the default
// holds there, as it does for a string whose schema declares no maxLength.
bool WalkStringLimits(const TSharedPtr<FJsonValue>& Value, const TSharedPtr<FJsonObject>& Schema,
	const FString& Pointer, int32 Depth, FMcpSchemaViolationDetail& OutViolation)
{
	if (!Value.IsValid())
	{
		return true;
	}
	if (Depth > StringLimitsMaxDepth)
	{
		return RefuseStringLimit(Pointer,
			FString::Printf(TEXT("%s is nested more than %d levels deep"), *Pointer, StringLimitsMaxDepth), OutViolation);
	}
	if (Value->Type == EJson::String)
	{
		double Limit = McpDefaultStringMaxLength;
		if (Schema.IsValid())
		{
			Schema->TryGetNumberField(TEXT("maxLength"), Limit);
		}
		return Value->AsString().Len() <= static_cast<int32>(Limit) || RefuseStringLimit(Pointer,
			FString::Printf(TEXT("%s must be at most %d characters"), *Pointer, static_cast<int32>(Limit)), OutViolation);
	}
	if (Value->Type == EJson::Array)
	{
		const TSharedPtr<FJsonObject>* Items = nullptr;
		const TSharedPtr<FJsonObject> ItemSchema =
			Schema.IsValid() && Schema->TryGetObjectField(TEXT("items"), Items) && Items ? *Items : nullptr;
		const TArray<TSharedPtr<FJsonValue>>& Elements = Value->AsArray();
		for (int32 Index = 0; Index < Elements.Num(); ++Index)
		{
			if (!WalkStringLimits(Elements[Index], ItemSchema,
				McpSchemaKeywords::JoinPointer(Pointer, FString::FromInt(Index)), Depth + 1, OutViolation))
			{
				return false;
			}
		}
		return true;
	}
	if (Value->Type != EJson::Object)
	{
		return true;
	}
	const TSharedPtr<FJsonObject>* Properties = nullptr;
	const bool bHasProperties = Schema.IsValid() && Schema->TryGetObjectField(TEXT("properties"), Properties) && Properties;
	for (const auto& Entry : Value->AsObject()->Values)
	{
		// UE 5.8 keys are UE::TSharedString: read them as FString. A key names a property, a row or a setting.
		const FString Key(*Entry.Key);
		if (Key.Len() > McpDefaultStringMaxLength)
		{
			const FString Holder = McpSchemaKeywords::PointerOrRoot(Pointer);
			return RefuseStringLimit(Holder, FString::Printf(TEXT("A key in %s is %d characters long; a key must be at most %d"),
				*Holder, Key.Len(), McpDefaultStringMaxLength), OutViolation);
		}
		const TSharedPtr<FJsonObject>* PropertySchema = nullptr;
		const TSharedPtr<FJsonObject> ChildSchema =
			bHasProperties && (*Properties)->TryGetObjectField(Key, PropertySchema) && PropertySchema ? *PropertySchema : nullptr;
		if (!WalkStringLimits(Entry.Value, ChildSchema, McpSchemaKeywords::JoinPointer(Pointer, Key), Depth + 1, OutViolation))
		{
			return false;
		}
	}
	return true;
}
}

bool McpCheckInputStringLengths(
	const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonObject>& Schema, FMcpSchemaViolationDetail& OutViolation)
{
	return !Params.IsValid() || WalkStringLimits(MakeShared<FJsonValueObject>(Params), Schema, FString(), 0, OutViolation);
}
