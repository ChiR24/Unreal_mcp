#include "McpOpenCodeAcpClientPrivate.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

#if PLATFORM_LINUX
#include <signal.h>
#endif

namespace UnrealAgent::OpenCodeAcp
{
TSharedPtr<FJsonObject> MakeObject()
    {
        return MakeShared<FJsonObject>();
    }

TSharedPtr<FJsonValue> CloneJsonId(const TSharedPtr<FJsonObject>& Message)
    {
        return Message.IsValid() ? Message->TryGetField(TEXT("id")) : nullptr;
    }

FString GetStringFieldOrEmpty(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
    {
        FString Value;
        if (Object.IsValid())
        {
            Object->TryGetStringField(FieldName, Value);
        }
        return Value;
    }

FString GetConfigOptionId(const TSharedPtr<FJsonObject>& Object)
    {
        FString OptionId = GetStringFieldOrEmpty(Object, TEXT("id"));
        if (OptionId.IsEmpty())
        {
            OptionId = GetStringFieldOrEmpty(Object, TEXT("configId"));
        }
        if (OptionId.IsEmpty())
        {
            OptionId = GetStringFieldOrEmpty(Object, TEXT("configOptionId"));
        }
        return OptionId;
    }

FString GetConfigOptionValue(const TSharedPtr<FJsonObject>& Object)
    {
        FString Value = GetStringFieldOrEmpty(Object, TEXT("currentValue"));
        if (Value.IsEmpty())
        {
            Value = GetStringFieldOrEmpty(Object, TEXT("value"));
        }
        if (Value.IsEmpty())
        {
            Value = GetStringFieldOrEmpty(Object, TEXT("optionValue"));
        }
        return Value;
    }

bool IsAgentConfigOption(const TSharedPtr<FJsonObject>& Option)
    {
        const FString OptionId = GetConfigOptionId(Option).ToLower();
        const FString Category = GetStringFieldOrEmpty(Option, TEXT("category")).ToLower();
        const FString Name = GetStringFieldOrEmpty(Option, TEXT("name")).ToLower();
        return OptionId == TEXT("agent")
            || OptionId == TEXT("agents")
            || OptionId == TEXT("mode")
            || OptionId == TEXT("modes")
            || Category == TEXT("agent")
            || Category == TEXT("agents")
            || Category == TEXT("mode")
            || Category == TEXT("modes")
            || Name == TEXT("agent")
            || Name == TEXT("agents")
            || Name == TEXT("mode")
            || Name == TEXT("modes");
    }

bool IsModelConfigOption(const TSharedPtr<FJsonObject>& Option)
    {
        const FString OptionId = GetConfigOptionId(Option).ToLower();
        const FString Category = GetStringFieldOrEmpty(Option, TEXT("category")).ToLower();
        const FString Name = GetStringFieldOrEmpty(Option, TEXT("name")).ToLower();
        return OptionId == TEXT("model")
            || OptionId == TEXT("models")
            || OptionId == TEXT("current_model")
            || OptionId == TEXT("currentmodel")
            || Category == TEXT("model")
            || Category == TEXT("models")
            || Category == TEXT("current_model")
            || Category == TEXT("currentmodel")
            || Name == TEXT("model")
            || Name == TEXT("models")
            || Name == TEXT("current_model")
            || Name == TEXT("currentmodel");
    }

bool IsThinkingConfigOption(const TSharedPtr<FJsonObject>& Option)
    {
        const FString OptionId = GetConfigOptionId(Option).ToLower();
        const FString Category = GetStringFieldOrEmpty(Option, TEXT("category")).ToLower();
        const FString Name = GetStringFieldOrEmpty(Option, TEXT("name")).ToLower();
        return OptionId == TEXT("thinking")
            || OptionId == TEXT("think")
            || OptionId == TEXT("reasoning")
            || OptionId == TEXT("reasoning_effort")
            || OptionId == TEXT("reasoningeffort")
            || OptionId == TEXT("reasoning.effort")
            || OptionId == TEXT("thinking_effort")
            || OptionId == TEXT("thinkingeffort")
            || OptionId == TEXT("effort")
            || Category == TEXT("thinking")
            || Category == TEXT("thought_level")
            || Category == TEXT("reasoning")
            || Category == TEXT("reasoning effort")
            || Category == TEXT("thinking effort")
            || Name == TEXT("thinking")
            || Name == TEXT("reasoning")
            || Name == TEXT("reasoning effort")
            || Name == TEXT("thinking effort");
    }

FString NormalizeMcpHostForUrl(FString Host)
    {
        Host = Host.TrimStartAndEnd();
        if (Host.IsEmpty() || Host == TEXT("0.0.0.0") || Host == TEXT("::") || Host == TEXT("[::]") || Host == TEXT("*"))
        {
            return TEXT("127.0.0.1");
        }

        if (Host.Contains(TEXT(":")) && !Host.StartsWith(TEXT("[")) && !Host.EndsWith(TEXT("]")))
        {
            return FString::Printf(TEXT("[%s]"), *Host);
        }
        return Host;
    }

FString GetModelProviderField(const TSharedPtr<FJsonObject>& Object)
    {
        if (!Object.IsValid())
        {
            return FString();
        }

        FString Provider;
        if (Object->TryGetStringField(TEXT("providerName"), Provider) && !Provider.IsEmpty())
        {
            return Provider;
        }
        if (Object->TryGetStringField(TEXT("provider"), Provider) && !Provider.IsEmpty())
        {
            return Provider;
        }
        if (Object->TryGetStringField(TEXT("vendorName"), Provider) && !Provider.IsEmpty())
        {
            return Provider;
        }
        if (Object->TryGetStringField(TEXT("vendor"), Provider) && !Provider.IsEmpty())
        {
            return Provider;
        }

        const TSharedPtr<FJsonObject>* ProviderObject = nullptr;
        if (Object->TryGetObjectField(TEXT("provider"), ProviderObject) && ProviderObject && ProviderObject->IsValid())
        {
            if ((*ProviderObject)->TryGetStringField(TEXT("name"), Provider) && !Provider.IsEmpty())
            {
                return Provider;
            }
            if ((*ProviderObject)->TryGetStringField(TEXT("id"), Provider) && !Provider.IsEmpty())
            {
                return Provider;
            }
        }

        return FString();
    }

int32 GetModelContextWindowTokens(const TSharedPtr<FJsonObject>& Object)
    {
        if (!Object.IsValid())
        {
            return 0;
        }

        static const TCHAR* ContextFields[] =
        {
            TEXT("contextWindow"),
            TEXT("contextWindowTokens"),
            TEXT("contextLength"),
            TEXT("contextSize"),
            TEXT("maxContextTokens"),
            TEXT("maxInputTokens")
        };

        for (const TCHAR* FieldName : ContextFields)
        {
            double NumericValue = 0.0;
            if (Object->TryGetNumberField(FieldName, NumericValue) && NumericValue > 0.0)
            {
                return FMath::RoundToInt(NumericValue);
            }
        }

        return 0;
    }
}
