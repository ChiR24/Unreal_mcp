// McpStructReflection.h
#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"

namespace McpStructReflection
{
    /** Resolve a logical field name to the internal name (UUserDefinedStruct has GUID suffixes). */
    FName ResolveFieldName(const UStruct* Struct, const FString& LogicalName);

    /** Write a JSON value into a struct field by FName lookup. Returns false + OutError on failure. */
    bool SetStructFieldFromJson(
        const UStruct* Struct,
        void* StructInstance,
        FName FieldName,
        const TSharedPtr<FJsonValue>& Value,
        FString& OutError);

    /**
     * Write all fields from a JSON object into a struct instance.
     * Returns false + OutError on first failure.
     *
     * NOTE: Aborts on first field failure, leaving any previously-written fields
     * in place (partial-write). Caller is responsible for snapshotting the struct
     * before this call if rollback-on-failure semantics are required.
     */
    bool SetStructFieldsFromJsonObject(
        const UStruct* Struct,
        void* StructInstance,
        const TSharedPtr<FJsonObject>& Fields,
        FString& OutError);

    /** Read a struct instance back to a JSON object (all fields). */
    TSharedPtr<FJsonObject> StructInstanceToJson(const UStruct* Struct, const void* StructInstance);

    /** Read a single field by name. Returns null on missing field. */
    TSharedPtr<FJsonValue> GetStructFieldAsJson(
        const UStruct* Struct,
        const void* StructInstance,
        FName FieldName);
}
