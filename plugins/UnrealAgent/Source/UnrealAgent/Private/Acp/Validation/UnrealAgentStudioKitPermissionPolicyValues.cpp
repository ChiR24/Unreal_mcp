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
