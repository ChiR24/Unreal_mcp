// McpSchemaBuilder.h — Fluent builder for MCP tool inputSchema JSON

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

/**
 * Fluent builder for JSON Schema objects used as MCP tool inputSchema.
 *
 * Usage:
 *   FMcpSchemaBuilder()
 *       .StringEnum(TEXT("action"), {TEXT("spawn"), TEXT("delete")}, TEXT("Action to perform"))
 *       .String(TEXT("name"), TEXT("Actor name"))
 *       .Object(TEXT("location"), TEXT("3D location"), [](FMcpSchemaBuilder& S) {
 *           S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
 *       })
 *       .Required({TEXT("action")})
 *       .Build();
 */
class FMcpSchemaBuilder
{
public:
	FMcpSchemaBuilder();

	/** String property. */
	FMcpSchemaBuilder& String(const FString& Name, const FString& Description);

	/** String property constrained to an enum of values. */
	FMcpSchemaBuilder& StringEnum(const FString& Name, const TArray<FString>& Values,
		const FString& Description);

	/** Number property. */
	FMcpSchemaBuilder& Number(const FString& Name, const FString& Description = FString());

	/** Boolean property. */
	FMcpSchemaBuilder& Bool(const FString& Name, const FString& Description = FString());

	/** Integer property. */
	FMcpSchemaBuilder& Integer(const FString& Name, const FString& Description = FString());

	/** Object property with optional nested sub-schema. */
	FMcpSchemaBuilder& Object(const FString& Name, const FString& Description,
		TFunction<void(FMcpSchemaBuilder&)> SubBuilder = nullptr);

	/** Array property with item type (default: "string"). */
	FMcpSchemaBuilder& Array(const FString& Name, const FString& Description,
		const FString& ItemType = TEXT("string"));

	/** Array of unconstrained items (items: {}); Array() would narrow them to a type. */
	FMcpSchemaBuilder& ArrayOfAny(const FString& Name, const FString& Description);

	/** Array of objects property with item schema. */
	FMcpSchemaBuilder& ArrayOfObjects(const FString& Name, const FString& Description,
		TFunction<void(FMcpSchemaBuilder&)> ItemBuilder = nullptr);

	/** Freeform object property (type: "object", no properties constraint). */
	FMcpSchemaBuilder& FreeformObject(const FString& Name, const FString& Description);

	/** Unconstrained "any" property (no type constraint). */
	FMcpSchemaBuilder& AnyValue(const FString& Name, const FString& Description = FString());

	/** JSON Schema type union property (e.g. type: ["number", "string"]). */
	FMcpSchemaBuilder& TypeUnion(const FString& Name, const TArray<FString>& Types,
		const FString& Description = FString());

	/** Declare required property names. Can be called multiple times. */
	FMcpSchemaBuilder& Required(const TArray<FString>& Names);

	/** Build the final inputSchema JSON object. */
	TSharedPtr<FJsonObject> Build() const;

	/** Direct read-only access to the accumulated properties JSON. */
	const TSharedPtr<FJsonObject> &GetProperties() const { return Properties; }

private:
	TSharedPtr<FJsonObject> Properties;
	TArray<FString> RequiredFields;

	void AddProperty(const FString& Name, const TSharedPtr<FJsonObject>& PropSchema);
};
