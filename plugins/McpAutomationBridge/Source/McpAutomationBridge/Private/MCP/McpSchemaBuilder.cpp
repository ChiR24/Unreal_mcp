// McpSchemaBuilder.cpp — Fluent builder for MCP tool inputSchema JSON

#include "McpVersionCompatibility.h"
#include "MCP/McpSchemaBuilder.h"

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

FMcpSchemaBuilder& FMcpSchemaBuilder::StringEnum(const FString& Name,
	const TArray<FString>& Values, const FString& Description)
{
	auto Prop = MakeTypedProperty(TEXT("string"), Description);
	Prop->SetArrayField(TEXT("enum"), MakeStringValueArray(Values));

	AddProperty(Name, Prop);
	return *this;
}

FMcpSchemaBuilder& FMcpSchemaBuilder::Number(const FString& Name, const FString& Description)
{
	auto Prop = MakeTypedProperty(TEXT("number"), Description);
	AddProperty(Name, Prop);
	return *this;
}

FMcpSchemaBuilder& FMcpSchemaBuilder::Bool(const FString& Name, const FString& Description)
{
	auto Prop = MakeTypedProperty(TEXT("boolean"), Description);
	AddProperty(Name, Prop);
	return *this;
}

FMcpSchemaBuilder& FMcpSchemaBuilder::Integer(const FString& Name, const FString& Description)
{
	auto Prop = MakeTypedProperty(TEXT("integer"), Description);
	AddProperty(Name, Prop);
	return *this;
}

FMcpSchemaBuilder& FMcpSchemaBuilder::Object(const FString& Name, const FString& Description,
	TFunction<void(FMcpSchemaBuilder&)> SubBuilder)
{
	auto Prop = MakeTypedProperty(TEXT("object"), Description);

	if (SubBuilder)
	{
		FMcpSchemaBuilder Sub;
		SubBuilder(Sub);
		Prop->SetObjectField(TEXT("properties"), Sub.Properties);
		AddRequiredFields(Prop, Sub.RequiredFields);
	}

	AddProperty(Name, Prop);
	return *this;
}

FMcpSchemaBuilder& FMcpSchemaBuilder::Array(const FString& Name, const FString& Description,
	const FString& ItemType)
{
	auto Prop = MakeTypedProperty(TEXT("array"), Description);

	auto Items = MakeShared<FJsonObject>();
	Items->SetStringField(TEXT("type"), ItemType);
	Prop->SetObjectField(TEXT("items"), Items);

	AddProperty(Name, Prop);
	return *this;
}

FMcpSchemaBuilder& FMcpSchemaBuilder::ArrayOfObjects(const FString& Name,
	const FString& Description, TFunction<void(FMcpSchemaBuilder&)> ItemBuilder)
{
	auto Prop = MakeTypedProperty(TEXT("array"), Description);

	auto Items = MakeShared<FJsonObject>();
	Items->SetStringField(TEXT("type"), TEXT("object"));
	if (ItemBuilder)
	{
		FMcpSchemaBuilder Sub;
		ItemBuilder(Sub);
		Items->SetObjectField(TEXT("properties"), Sub.Properties);
		AddRequiredFields(Items, Sub.RequiredFields);
	}
	Prop->SetObjectField(TEXT("items"), Items);

	AddProperty(Name, Prop);
	return *this;
}

FMcpSchemaBuilder& FMcpSchemaBuilder::FreeformObject(const FString& Name,
	const FString& Description)
{
	auto Prop = MakeTypedProperty(TEXT("object"), Description);
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

TSharedPtr<FJsonObject> FMcpSchemaBuilder::Build() const
{
	auto Schema = MakeShared<FJsonObject>();
	Schema->SetStringField(TEXT("type"), TEXT("object"));
	Schema->SetObjectField(TEXT("properties"), Properties);

	AddRequiredFields(Schema, RequiredFields);

	return Schema;
}
