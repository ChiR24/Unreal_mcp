// McpSchemaBuilder.h — Fluent builder for MCP tool inputSchema JSON

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

/**
 * Fluent builder for JSON Schema objects used as MCP tool inputSchema. Every fact about a property is stated by the
 * call that adds it, so the chain reads as the schema it builds (a unit test rebuilds the gateway's from its text).
 *
 * Usage:
 *   FMcpSchemaBuilder()
 *       .StringEnum(TEXT("action"), {TEXT("spawn"), TEXT("delete")}, TEXT("Action to perform"))
 *       .Integer(TEXT("count"), TEXT("How many"), 1, 10)
 *       .ClosedObject(TEXT("location"), TEXT("3D location"), [](FMcpSchemaBuilder& S) {
 *           S.Integer(TEXT("x")).Integer(TEXT("y")).Integer(TEXT("z"));
 *       })
 *       .Required({TEXT("action")})
 *       .Closed()
 *       .Build();
 */
class FMcpSchemaBuilder
{
public:
	FMcpSchemaBuilder();

	/** String property. */
	FMcpSchemaBuilder& String(const FString& Name, const FString& Description);

	/** String property whose length lies in [MinLength, MaxLength]. */
	FMcpSchemaBuilder& String(const FString& Name, const FString& Description, int32 MinLength, int32 MaxLength);

	/** String property constrained to an enum of values. */
	FMcpSchemaBuilder& StringEnum(const FString& Name, const TArray<FString>& Values,
		const FString& Description);

	/** Array of MinItems..MaxItems strings, each of 1..MaxLength characters. */
	FMcpSchemaBuilder& StringArray(const FString& Name, const FString& Description, int32 MinItems, int32 MaxItems,
		int32 MaxLength);

	/** Integer property. */
	FMcpSchemaBuilder& Integer(const FString& Name, const FString& Description = FString());

	/** Integer property of at least Minimum. */
	FMcpSchemaBuilder& Integer(const FString& Name, const FString& Description, int64 Minimum);

	/** Integer property in [Minimum, Maximum]. */
	FMcpSchemaBuilder& Integer(const FString& Name, const FString& Description, int64 Minimum, int64 Maximum);

	/** Object property holding only the keys its sub-builder declares (additionalProperties false). */
	FMcpSchemaBuilder& ClosedObject(const FString& Name, const FString& Description,
		TFunction<void(FMcpSchemaBuilder&)> SubBuilder);

	/** Object property with free keys (additionalProperties true). */
	FMcpSchemaBuilder& OpenObject(const FString& Name, const FString& Description);

	/** Declare required property names. Can be called multiple times. */
	FMcpSchemaBuilder& Required(const TArray<FString>& Names);

	/** Refuse keys this schema does not declare (additionalProperties false). */
	FMcpSchemaBuilder& Closed();

	/** Build the final inputSchema JSON object. */
	TSharedPtr<FJsonObject> Build() const;


private:
	TSharedPtr<FJsonObject> Properties;
	TArray<FString> RequiredFields;
	bool bClosed = false;

	void AddProperty(const FString& Name, const TSharedPtr<FJsonObject>& PropSchema);
};
