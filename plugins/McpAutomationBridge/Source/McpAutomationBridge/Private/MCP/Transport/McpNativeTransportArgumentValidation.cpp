#include "MCP/Transport/McpNativeTransportArgumentValidation.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "MCP/Execute/McpNativeGatewayCanonicalRecords.h"
#include "MCP/Execute/McpNativeGatewayValidation.h"
#include "MCP/Registry/McpToolDefinition.h"

namespace McpNativeArgumentValidation {
namespace {

bool SchemaAllowsType(const TSharedPtr<FJsonObject> &Schema,
                      const FString &ExpectedType) {
  FString SingleType;
  if (Schema->TryGetStringField(TEXT("type"), SingleType))
    return SingleType == ExpectedType;
  const TArray<TSharedPtr<FJsonValue>> *Types = nullptr;
  if (!Schema->TryGetArrayField(TEXT("type"), Types) || !Types)
    return true;
  for (const TSharedPtr<FJsonValue> &Type : *Types) {
    FString TypeName;
    if (Type.IsValid() && Type->TryGetString(TypeName) &&
        TypeName == ExpectedType) {
      return true;
    }
  }
  return false;
}

bool ValidateEnum(const TSharedPtr<FJsonValue> &Value,
                  const TSharedPtr<FJsonObject> &Schema) {
  const TArray<TSharedPtr<FJsonValue>> *Allowed = nullptr;
  if (!Schema->TryGetArrayField(TEXT("enum"), Allowed) || !Allowed)
    return true;
  FString Actual;
  if (!Value.IsValid() || !Value->TryGetString(Actual))
    return false;
  for (const TSharedPtr<FJsonValue> &Candidate : *Allowed) {
    FString AllowedValue;
    if (Candidate.IsValid() && Candidate->TryGetString(AllowedValue) &&
        Actual == AllowedValue) {
      return true;
    }
  }
  return false;
}

bool ValidateValueAgainstSchema(const TSharedPtr<FJsonValue> &Value,
                                const TSharedPtr<FJsonObject> &Schema,
                                const FString &Path,
                                FString &OutArgumentPath,
                                FString &OutErrorCode,
                                FString &OutErrorMessage);

bool ValidateObject(const TSharedPtr<FJsonObject> &Object,
                    const TSharedPtr<FJsonObject> &Schema,
                    const FString &Path, FString &OutArgumentPath,
                    FString &OutErrorCode, FString &OutErrorMessage) {
  const TSharedPtr<FJsonObject> *Properties = nullptr;
  if (!Schema->TryGetObjectField(TEXT("properties"), Properties) ||
      !Properties || !Properties->IsValid()) {
    return true;
  }
  const TArray<TSharedPtr<FJsonValue>> *Required = nullptr;
  if (Schema->TryGetArrayField(TEXT("required"), Required) && Required) {
    for (const TSharedPtr<FJsonValue> &RequiredValue : *Required) {
      FString RequiredName;
      if (RequiredValue.IsValid() &&
          RequiredValue->TryGetString(RequiredName) &&
          !Object->HasField(RequiredName)) {
        OutArgumentPath =
            Path.IsEmpty() ? RequiredName : Path + TEXT(".") + RequiredName;
        OutErrorCode = TEXT("INVALID_TOOL_ARGUMENT");
        OutErrorMessage =
            FString::Printf(TEXT("Missing required argument '%s'"),
                            *OutArgumentPath);
        return false;
      }
    }
  }
  for (const TPair<FString, TSharedPtr<FJsonValue>> &Entry : Object->Values) {
    const TSharedPtr<FJsonObject> *PropertySchema = nullptr;
    const FString EntryPath =
        Path.IsEmpty() ? Entry.Key : Path + TEXT(".") + Entry.Key;
    if (!(*Properties)->TryGetObjectField(Entry.Key, PropertySchema) ||
        !PropertySchema || !PropertySchema->IsValid()) {
      OutArgumentPath = EntryPath;
      OutErrorCode = TEXT("UNKNOWN_TOOL_ARGUMENT");
      OutErrorMessage =
          FString::Printf(TEXT("Unknown argument '%s'"), *EntryPath);
      return false;
    }
    if (!ValidateValueAgainstSchema(Entry.Value, *PropertySchema, EntryPath,
                                    OutArgumentPath, OutErrorCode,
                                    OutErrorMessage)) {
      return false;
    }
  }
  return true;
}

bool ValidateValueAgainstSchema(const TSharedPtr<FJsonValue> &Value,
                                const TSharedPtr<FJsonObject> &Schema,
                                const FString &Path,
                                FString &OutArgumentPath,
                                FString &OutErrorCode,
                                FString &OutErrorMessage) {
  if (!Value.IsValid() || !Schema.IsValid()) {
    OutArgumentPath = Path;
    OutErrorCode = TEXT("INVALID_TOOL_ARGUMENT");
    OutErrorMessage = FString::Printf(TEXT("Argument '%s' is invalid"), *Path);
    return false;
  }
  bool bTypeValid = false;
  switch (Value->Type) {
  case EJson::String:
    bTypeValid = SchemaAllowsType(Schema, TEXT("string"));
    break;
	  case EJson::Number: {
	    const double Number = Value->AsNumber();
	    constexpr double MaxExactJsonInteger = 9007199254740991.0;
	    const bool bExactInteger =
	        FMath::Abs(Number) <= MaxExactJsonInteger &&
	        Number == FMath::TruncToDouble(Number);
	    bTypeValid =
	        FMath::IsFinite(Number) &&
	        (SchemaAllowsType(Schema, TEXT("number")) ||
	         (SchemaAllowsType(Schema, TEXT("integer")) && bExactInteger));
    break;
  }
  case EJson::Boolean:
    bTypeValid = SchemaAllowsType(Schema, TEXT("boolean"));
    break;
  case EJson::Array:
    bTypeValid = SchemaAllowsType(Schema, TEXT("array"));
    break;
  case EJson::Object:
    bTypeValid = SchemaAllowsType(Schema, TEXT("object"));
    break;
  case EJson::Null:
    bTypeValid = SchemaAllowsType(Schema, TEXT("null"));
    break;
  default:
    bTypeValid = false;
    break;
  }
  if (!bTypeValid || !ValidateEnum(Value, Schema)) {
    OutArgumentPath = Path;
    OutErrorCode = TEXT("INVALID_TOOL_ARGUMENT");
    OutErrorMessage =
        FString::Printf(TEXT("Argument '%s' does not match its schema"), *Path);
    return false;
  }
  if (Value->Type == EJson::Object) {
    return ValidateObject(Value->AsObject(), Schema, Path, OutArgumentPath,
                          OutErrorCode, OutErrorMessage);
  }
  if (Value->Type == EJson::Array) {
    const TSharedPtr<FJsonObject> *ItemSchema = nullptr;
    if (Schema->TryGetObjectField(TEXT("items"), ItemSchema) &&
        ItemSchema && ItemSchema->IsValid()) {
      const TArray<TSharedPtr<FJsonValue>> &Items = Value->AsArray();
      for (int32 Index = 0; Index < Items.Num(); ++Index) {
        if (!ValidateValueAgainstSchema(
                Items[Index], *ItemSchema,
                FString::Printf(TEXT("%s[%d]"), *Path, Index),
                OutArgumentPath, OutErrorCode, OutErrorMessage)) {
          return false;
        }
      }
    }
  }
  return true;
}

}

bool ValidateToolArguments(const FMcpToolDefinition *ToolDefinition,
                           const TSharedPtr<FJsonObject> &Arguments,
                           FString &OutArgumentPath, FString &OutErrorCode,
                           FString &OutErrorMessage) {
  OutArgumentPath.Reset();
  OutErrorCode.Reset();
  OutErrorMessage.Reset();
  if (!ToolDefinition)
    return true;

  // Until the canonical records load, validation degrades to the pre-Task-27
  // per-tool opt-in against the tool-union schema — never to no validation.
  if (!McpCanonicalRecordsAvailable()) {
    if (!ToolDefinition->EnforceStrictArguments())
      return true;
    const TSharedPtr<FJsonObject> Schema = ToolDefinition->BuildInputSchema();
    if (!Schema.IsValid() || !Arguments.IsValid()) {
      OutErrorCode = TEXT("INVALID_TOOL_ARGUMENT");
      OutErrorMessage = TEXT("Tool arguments could not be validated");
      return false;
    }
    return ValidateObject(Arguments, Schema, FString(), OutArgumentPath,
                          OutErrorCode, OutErrorMessage);
  }

  // Records are loaded: every action of every tool is validated against its own
  // exact schema, replacing the opt-in that covered a single parent tool.
  return McpValidateCanonicalToolArguments(ToolDefinition->GetName(), Arguments,
                                           OutArgumentPath, OutErrorCode,
                                           OutErrorMessage);
}

}
