#include "Acp/Validation/UnrealAgentStudioKitPermissionPolicies.h"

#include "Acp/Validation/UnrealAgentStudioKitPermissionPolicyValues.h"
#include "Acp/Validation/UnrealAgentStudioKitValidationChecks.h"
#include "Acp/Validation/UnrealAgentValidationRunner.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace UnrealAgent::Validation
{
namespace
{
void AddLegacyToolsErrors(
    FUnrealAgentValidationResult& Result,
    const TSharedPtr<FJsonValue>& ToolsValue,
    const FString& Source)
{
    if (!ToolsValue.IsValid()
        || ToolsValue->Type != EJson::Object
        || !ToolsValue->AsObject().IsValid())
    {
        Result.bPassed = false;
        Result.Errors.Add(FString::Printf(
            TEXT("OpenCode legacy tools policy has unsupported shape: %s"),
            *Source));
        return;
    }
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : ToolsValue->AsObject()->Values)
    {
        if (IsProtectedOpenCodePermissionPattern(Field.Key)
            && PermissionPolicyValues::LegacyToolValueEnablesAccess(Field.Value))
        {
            Result.bPassed = false;
            Result.Errors.Add(FString::Printf(
                TEXT("Unsafe OpenCode legacy tools policy enables protected tool '%s': %s"),
                *Field.Key,
                *Source));
        }
    }
}

void AddPermissionPolicyErrors(
    FUnrealAgentValidationResult& Result,
    const TSharedPtr<FJsonValue>& PermissionValue,
    const FString& Source)
{
    if (!PermissionValue.IsValid())
    {
        Result.bPassed = false;
        Result.Errors.Add(FString::Printf(
            TEXT("OpenCode permission policy is missing: %s"),
            *Source));
        return;
    }
    if (PermissionValue->Type == EJson::String)
    {
        if (PermissionPolicyValues::PermissionValueContainsAllow(PermissionValue))
        {
            Result.bPassed = false;
            Result.Errors.Add(FString::Printf(
                TEXT("Unsafe OpenCode config globally allows tools: %s"),
                *Source));
        }
        return;
    }
    if (PermissionValue->Type != EJson::Object
        || !PermissionValue->AsObject().IsValid())
    {
        Result.bPassed = false;
        Result.Errors.Add(FString::Printf(
            TEXT("OpenCode permission policy has unsupported shape: %s"),
            *Source));
        return;
    }
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : PermissionValue->AsObject()->Values)
    {
        if (IsProtectedOpenCodePermissionPattern(Field.Key)
            && PermissionPolicyValues::PermissionValueContainsAllow(Field.Value))
        {
            Result.bPassed = false;
            Result.Errors.Add(FString::Printf(
                TEXT("Unsafe OpenCode config auto-allows protected tool pattern '%s': %s"),
                *Field.Key,
                *Source));
        }
    }
}

void AddNestedPermissionErrors(
    FUnrealAgentValidationResult& Result,
    const TSharedPtr<FJsonValue>& Value,
    const FString& Source);

void AddConfigFieldErrors(
    FUnrealAgentValidationResult& Result,
    const FString& FieldName,
    const TSharedPtr<FJsonValue>& Value,
    const FString& Source)
{
    if (FieldName.Equals(TEXT("permission"), ESearchCase::IgnoreCase))
    {
        AddPermissionPolicyErrors(Result, Value, Source);
    }
    else if (FieldName.Equals(TEXT("tools"), ESearchCase::IgnoreCase))
    {
        AddLegacyToolsErrors(Result, Value, Source);
    }
    else if ((FieldName.Equals(TEXT("plugin"), ESearchCase::IgnoreCase)
            || FieldName.Equals(TEXT("plugins"), ESearchCase::IgnoreCase))
        && PermissionPolicyValues::ConfiguresExternalRuntime(Value))
    {
        Result.bPassed = false;
        Result.Errors.Add(FString::Printf(
            TEXT("OpenCode config loads an untrusted plugin: %s"),
            *Source));
    }
    else if ((FieldName.Equals(TEXT("mcp"), ESearchCase::IgnoreCase)
            || FieldName.Equals(TEXT("mcpServers"), ESearchCase::IgnoreCase))
        && PermissionPolicyValues::ConfiguresExternalRuntime(Value))
    {
        Result.bPassed = false;
        Result.Errors.Add(FString::Printf(
            TEXT("OpenCode config defines an untrusted MCP server: %s"),
            *Source));
    }
    else
    {
        AddNestedPermissionErrors(Result, Value, Source);
    }
}

void AddNestedPermissionErrors(
    FUnrealAgentValidationResult& Result,
    const TSharedPtr<FJsonValue>& Value,
    const FString& Source)
{
    if (!Value.IsValid())
    {
        return;
    }
    if (Value->Type == EJson::Array)
    {
        for (const TSharedPtr<FJsonValue>& Element : Value->AsArray())
        {
            AddNestedPermissionErrors(Result, Element, Source);
        }
        return;
    }
    if (Value->Type != EJson::Object || !Value->AsObject().IsValid())
    {
        return;
    }
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Value->AsObject()->Values)
    {
        AddConfigFieldErrors(Result, Field.Key, Field.Value, Source);
    }
}
}

void AddOpenCodePermissionConfigErrors(
    FUnrealAgentValidationResult& Result,
    const TSharedPtr<FJsonObject>& ConfigObject,
    const FString& Source,
    const bool bRequireTopLevelPolicy)
{
    const TSharedPtr<FJsonValue> PermissionValue =
        ConfigObject->TryGetField(TEXT("permission"));
    if (bRequireTopLevelPolicy || PermissionValue.IsValid())
    {
        AddPermissionPolicyErrors(Result, PermissionValue, Source);
    }
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : ConfigObject->Values)
    {
        if (!Field.Key.Equals(TEXT("permission"), ESearchCase::IgnoreCase))
        {
            AddConfigFieldErrors(Result, Field.Key, Field.Value, Source);
        }
    }
}
}
