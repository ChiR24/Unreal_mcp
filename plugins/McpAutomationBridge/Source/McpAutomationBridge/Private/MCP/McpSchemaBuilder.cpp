// McpSchemaBuilder.cpp — Fluent builder for MCP tool inputSchema JSON

#include "McpVersionCompatibility.h"
#include "MCP/McpSchemaBuilder.h"

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
	auto Prop = MakeShared<FJsonObject>();
	Prop->SetStringField(TEXT("type"), TEXT("string"));
	if (!Description.IsEmpty())
	{
		Prop->SetStringField(TEXT("description"), Description);
	}
	AddProperty(Name, Prop);
	return *this;
}

FMcpSchemaBuilder& FMcpSchemaBuilder::StringEnum(const FString& Name,
	const TArray<FString>& Values, const FString& Description)
{
	auto Prop = MakeShared<FJsonObject>();
	Prop->SetStringField(TEXT("type"), TEXT("string"));
	if (!Description.IsEmpty())
	{
		Prop->SetStringField(TEXT("description"), Description);
	}

	TArray<TSharedPtr<FJsonValue>> EnumValues;
	for (const FString& Val : Values)
	{
		EnumValues.Add(MakeShared<FJsonValueString>(Val));
	}
	Prop->SetArrayField(TEXT("enum"), EnumValues);

	AddProperty(Name, Prop);
	return *this;
}

FMcpSchemaBuilder& FMcpSchemaBuilder::Number(const FString& Name, const FString& Description)
{
	auto Prop = MakeShared<FJsonObject>();
	Prop->SetStringField(TEXT("type"), TEXT("number"));
	if (!Description.IsEmpty())
	{
		Prop->SetStringField(TEXT("description"), Description);
	}
	AddProperty(Name, Prop);
	return *this;
}

FMcpSchemaBuilder& FMcpSchemaBuilder::Bool(const FString& Name, const FString& Description)
{
	auto Prop = MakeShared<FJsonObject>();
	Prop->SetStringField(TEXT("type"), TEXT("boolean"));
	if (!Description.IsEmpty())
	{
		Prop->SetStringField(TEXT("description"), Description);
	}
	AddProperty(Name, Prop);
	return *this;
}

FMcpSchemaBuilder& FMcpSchemaBuilder::Integer(const FString& Name, const FString& Description)
{
	auto Prop = MakeShared<FJsonObject>();
	Prop->SetStringField(TEXT("type"), TEXT("integer"));
	if (!Description.IsEmpty())
	{
		Prop->SetStringField(TEXT("description"), Description);
	}
	AddProperty(Name, Prop);
	return *this;
}

FMcpSchemaBuilder& FMcpSchemaBuilder::Object(const FString& Name, const FString& Description,
	TFunction<void(FMcpSchemaBuilder&)> SubBuilder)
{
	auto Prop = MakeShared<FJsonObject>();
	Prop->SetStringField(TEXT("type"), TEXT("object"));
	if (!Description.IsEmpty())
	{
		Prop->SetStringField(TEXT("description"), Description);
	}

	if (SubBuilder)
	{
		FMcpSchemaBuilder Sub;
		SubBuilder(Sub);
		Prop->SetObjectField(TEXT("properties"), Sub.Properties);
		if (Sub.RequiredFields.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Req;
			for (const FString& RequiredName : Sub.RequiredFields)
			{
				Req.Add(MakeShared<FJsonValueString>(RequiredName));
			}
			Prop->SetArrayField(TEXT("required"), Req);
		}
	}

	AddProperty(Name, Prop);
	return *this;
}

FMcpSchemaBuilder& FMcpSchemaBuilder::Array(const FString& Name, const FString& Description,
	const FString& ItemType)
{
	auto Prop = MakeShared<FJsonObject>();
	Prop->SetStringField(TEXT("type"), TEXT("array"));
	if (!Description.IsEmpty())
	{
		Prop->SetStringField(TEXT("description"), Description);
	}

	auto Items = MakeShared<FJsonObject>();
	Items->SetStringField(TEXT("type"), ItemType);
	Prop->SetObjectField(TEXT("items"), Items);

	AddProperty(Name, Prop);
	return *this;
}

FMcpSchemaBuilder& FMcpSchemaBuilder::ArrayOfObjects(const FString& Name,
	const FString& Description, TFunction<void(FMcpSchemaBuilder&)> ItemBuilder)
{
	auto Prop = MakeShared<FJsonObject>();
	Prop->SetStringField(TEXT("type"), TEXT("array"));
	if (!Description.IsEmpty())
	{
		Prop->SetStringField(TEXT("description"), Description);
	}

	auto Items = MakeShared<FJsonObject>();
	Items->SetStringField(TEXT("type"), TEXT("object"));
	if (ItemBuilder)
	{
		FMcpSchemaBuilder Sub;
		ItemBuilder(Sub);
		Items->SetObjectField(TEXT("properties"), Sub.Properties);
		if (Sub.RequiredFields.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Req;
			for (const FString& RequiredName : Sub.RequiredFields)
			{
				Req.Add(MakeShared<FJsonValueString>(RequiredName));
			}
			Items->SetArrayField(TEXT("required"), Req);
		}
	}
	Prop->SetObjectField(TEXT("items"), Items);

	AddProperty(Name, Prop);
	return *this;
}

FMcpSchemaBuilder& FMcpSchemaBuilder::FreeformObject(const FString& Name,
	const FString& Description)
{
	auto Prop = MakeShared<FJsonObject>();
	Prop->SetStringField(TEXT("type"), TEXT("object"));
	if (!Description.IsEmpty())
	{
		Prop->SetStringField(TEXT("description"), Description);
	}
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

	if (RequiredFields.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> Req;
		for (const FString& Name : RequiredFields)
		{
			Req.Add(MakeShared<FJsonValueString>(Name));
		}
		Schema->SetArrayField(TEXT("required"), Req);
	}

	return Schema;
}
