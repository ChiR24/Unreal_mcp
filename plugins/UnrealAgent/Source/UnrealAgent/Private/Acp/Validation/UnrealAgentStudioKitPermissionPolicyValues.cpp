#include "Acp/Validation/UnrealAgentStudioKitPermissionPolicyValues.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace UnrealAgent::Validation::PermissionPolicyValues
{
bool PermissionValueContainsAllow(const TSharedPtr<FJsonValue>& Value)
{
    if (!Value.IsValid())
    {
        return false;
    }
    if (Value->Type == EJson::String)
    {
        return Value->AsString().Equals(TEXT("allow"), ESearchCase::IgnoreCase);
    }
    if (Value->Type == EJson::Array)
    {
        for (const TSharedPtr<FJsonValue>& Element : Value->AsArray())
        {
            if (PermissionValueContainsAllow(Element))
            {
                return true;
            }
        }
        return false;
    }
    if (Value->Type != EJson::Object || !Value->AsObject().IsValid())
    {
        return false;
    }
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Value->AsObject()->Values)
    {
        // OpenCode's "skill" and "task" keys are skill/task routing configuration,
        // not tool permissions. Allowing an entire skill (e.g. "unreal-*": "allow")
        // is the documented Studio Kit behavior and should not be conflated with
        // a global tool-permission allow. Inverse of IsProtectedOpenCodePermissionPattern.
        const FString TrimmedFieldKey = Field.Key.TrimStartAndEnd();
        if (TrimmedFieldKey.Equals(TEXT("skill"), ESearchCase::IgnoreCase)
            || TrimmedFieldKey.Equals(TEXT("task"), ESearchCase::IgnoreCase))
        {
            continue;
        }
        if (PermissionValueContainsAllow(Field.Value))
        {
            return true;
        }
    }
    return false;
}

bool LegacyToolValueEnablesAccess(const TSharedPtr<FJsonValue>& Value)
{
    if (!Value.IsValid() || Value->IsNull())
    {
        return false;
    }
    if (Value->Type == EJson::Boolean)
    {
        return Value->AsBool();
    }
    if (Value->Type == EJson::String)
    {
        const FString Setting = Value->AsString();
        return Setting.Equals(TEXT("allow"), ESearchCase::IgnoreCase)
            || Setting.Equals(TEXT("true"), ESearchCase::IgnoreCase);
    }
    if (Value->Type == EJson::Array)
    {
        for (const TSharedPtr<FJsonValue>& Element : Value->AsArray())
        {
            if (LegacyToolValueEnablesAccess(Element))
            {
                return true;
            }
        }
        return false;
    }
    if (Value->Type != EJson::Object || !Value->AsObject().IsValid())
    {
        return false;
    }
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Value->AsObject()->Values)
    {
        if (LegacyToolValueEnablesAccess(Field.Value))
        {
            return true;
        }
    }
    return false;
}

bool ConfiguresExternalRuntime(const TSharedPtr<FJsonValue>& Value)
{
    if (!Value.IsValid() || Value->IsNull())
    {
        return false;
    }
    if (Value->Type == EJson::String)
    {
        return !Value->AsString().TrimStartAndEnd().IsEmpty();
    }
    if (Value->Type == EJson::Array)
    {
        return !Value->AsArray().IsEmpty();
    }
    if (Value->Type == EJson::Object && Value->AsObject().IsValid())
    {
        return !Value->AsObject()->Values.IsEmpty();
    }
    return Value->Type != EJson::Boolean || Value->AsBool();
}
}
