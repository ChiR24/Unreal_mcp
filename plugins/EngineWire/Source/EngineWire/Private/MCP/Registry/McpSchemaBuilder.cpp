// McpSchemaBuilder.cpp — Fluent builder for MCP tool inputSchema JSON

#include "MCP/Registry/McpSchemaBuilder.h"

#include "Core/Compatibility/McpVersionCompatibility.h"

namespace
{
TArray<TSharedPtr<FJsonValue>> MakeStringValueArray(const TArray<FString>& Values)
{
	TArray<TSharedPtr<FJsonValue>> Result;
	for (const FString& Value : Values)
	{
		Result.Add(MakeShared<FJsonValueString>(Value));
	}
	return Result;
}

TSharedPtr<FJsonObject> MakeTypedProperty(const TCHAR* Type, const FString& Description)
{
	auto Prop = MakeShared<FJsonObject>();
	Prop->SetStringField(TEXT("type"), Type);
	if (!Description.IsEmpty())
	{
		Prop->SetStringField(TEXT("description"), Description);
	}
	return Prop;
}

void AddRequiredFields(TSharedPtr<FJsonObject> Schema, const TArray<FString>& RequiredFields)
{
	if (RequiredFields.Num() > 0)
	{
		Schema->SetArrayField(TEXT("required"), MakeStringValueArray(RequiredFields));
	}
}
}

FMcpSchemaBuilder::FMcpSchemaBuilder()
	: Properties(MakeShared<FJsonObject>())
{
}

void FMcpSchemaBuilder::AddProperty(const FString& Name, const TSharedPtr<FJsonObject>& PropSchema)
{
	Properties->SetObjectField(Name, PropSchema);
}

FMcpSchemaBuilder& FMcpSchemaBuilder::String(const FString& Name, const FString& Description)
{
	auto Prop = MakeTypedProperty(TEXT("string"), Description);
	AddProperty(Name, Prop);
	return *this;
}

FMcpSchemaBuilder& FMcpSchemaBuilder::String(const FString& Name, const FString& Description,
	int32 MinLength, int32 MaxLength)
{
	auto Prop = MakeTypedProperty(TEXT("string"), Description);
	Prop->SetNumberField(TEXT("minLength"), MinLength);
	Prop->SetNumberField(TEXT("maxLength"), MaxLength);
	AddProperty(Name, Prop);
	return *this;
}

FMcpSchemaBuilder& FMcpSchemaBuilder::StringEnum(const FString& Name,
	const TArray<FString>& Values, const FString& Description)
{
	auto Prop = MakeTypedProperty(TEXT("string"), Description);
	Prop->SetArrayField(TEXT("enum"), MakeStringValueArray(Values));

	AddProperty(Name, Prop);
	return *this;
}

FMcpSchemaBuilder& FMcpSchemaBuilder::StringArray(const FString& Name, const FString& Description,
	int32 MinItems, int32 MaxItems, int32 MaxLength)
{
	auto Item = MakeShared<FJsonObject>();
	Item->SetStringField(TEXT("type"), TEXT("string"));
	Item->SetNumberField(TEXT("minLength"), 1);
	Item->SetNumberField(TEXT("maxLength"), MaxLength);
	auto Prop = MakeTypedProperty(TEXT("array"), Description);
	Prop->SetObjectField(TEXT("items"), Item);
	Prop->SetNumberField(TEXT("minItems"), MinItems);
	Prop->SetNumberField(TEXT("maxItems"), MaxItems);
	AddProperty(Name, Prop);
	return *this;
}

FMcpSchemaBuilder& FMcpSchemaBuilder::Integer(const FString& Name, const FString& Description)
{
	auto Prop = MakeTypedProperty(TEXT("integer"), Description);
	AddProperty(Name, Prop);
	return *this;
}

FMcpSchemaBuilder& FMcpSchemaBuilder::Integer(const FString& Name, const FString& Description, int64 Minimum)
{
	auto Prop = MakeTypedProperty(TEXT("integer"), Description);
	Prop->SetNumberField(TEXT("minimum"), static_cast<double>(Minimum));
	AddProperty(Name, Prop);
	return *this;
}

FMcpSchemaBuilder& FMcpSchemaBuilder::Integer(const FString& Name, const FString& Description,
	int64 Minimum, int64 Maximum)
{
	auto Prop = MakeTypedProperty(TEXT("integer"), Description);
	Prop->SetNumberField(TEXT("minimum"), static_cast<double>(Minimum));
	Prop->SetNumberField(TEXT("maximum"), static_cast<double>(Maximum));
	AddProperty(Name, Prop);
	return *this;
}

FMcpSchemaBuilder& FMcpSchemaBuilder::ClosedObject(const FString& Name, const FString& Description,
	TFunction<void(FMcpSchemaBuilder&)> SubBuilder)
{
	auto Prop = MakeTypedProperty(TEXT("object"), Description);
	FMcpSchemaBuilder Sub;
	if (SubBuilder)
	{
		SubBuilder(Sub);
	}
	Prop->SetObjectField(TEXT("properties"), Sub.Properties);
	AddRequiredFields(Prop, Sub.RequiredFields);
	Prop->SetBoolField(TEXT("additionalProperties"), false);
	AddProperty(Name, Prop);
	return *this;
}

FMcpSchemaBuilder& FMcpSchemaBuilder::OpenObject(const FString& Name, const FString& Description)
{
	auto Prop = MakeTypedProperty(TEXT("object"), Description);
	Prop->SetBoolField(TEXT("additionalProperties"), true);
	AddProperty(Name, Prop);
	return *this;
}

FMcpSchemaBuilder& FMcpSchemaBuilder::Required(const TArray<FString>& Names)
{
	for (const FString& Name : Names)
	{
		RequiredFields.AddUnique(Name);
	}
	return *this;
}

FMcpSchemaBuilder& FMcpSchemaBuilder::Closed()
{
	bClosed = true;
	return *this;
}

TSharedPtr<FJsonObject> FMcpSchemaBuilder::Build() const
{
	auto Schema = MakeShared<FJsonObject>();
	Schema->SetStringField(TEXT("type"), TEXT("object"));
	Schema->SetObjectField(TEXT("properties"), Properties);

	AddRequiredFields(Schema, RequiredFields);
	if (bClosed)
	{
		Schema->SetBoolField(TEXT("additionalProperties"), false);
	}

	return Schema;
}
