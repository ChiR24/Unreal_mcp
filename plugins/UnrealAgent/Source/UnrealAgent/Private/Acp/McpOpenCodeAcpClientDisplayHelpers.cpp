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
FString TruncateForDisplay(const FString& Text, int32 MaxChars)
    {
        return Text.Len() <= MaxChars ? Text : Text.Left(MaxChars) + TEXT("\n[truncated]");
    }

void AddToolActivityDetail(TArray<FString>& Details, const FString& Detail)
    {
        FString NormalizedDetail = Detail.TrimStartAndEnd();
        NormalizedDetail.ReplaceInline(TEXT("\r"), TEXT(" "));
        NormalizedDetail.ReplaceInline(TEXT("\n"), TEXT(" "));
        if (!NormalizedDetail.IsEmpty() && !Details.Contains(NormalizedDetail))
        {
            Details.Add(TruncateForDisplay(NormalizedDetail, MaxToolActivityDetailChars));
        }
    }

FString GetJsonScalarDisplayText(const TSharedPtr<FJsonValue>& Value)
    {
        if (!Value.IsValid())
        {
            return FString();
        }

        if (Value->Type == EJson::String)
        {
            return Value->AsString();
        }
        if (Value->Type == EJson::Number)
        {
            return FString::SanitizeFloat(Value->AsNumber());
        }
        if (Value->Type == EJson::Boolean)
        {
            return Value->AsBool() ? TEXT("true") : TEXT("false");
        }

        return FString();
    }

bool IsPathLikeToolField(const FString& FieldName)
    {
        const FString NormalizedField = FieldName.ToLower();
        return NormalizedField.Contains(TEXT("path"))
            || NormalizedField.Contains(TEXT("file"))
            || NormalizedField == TEXT("uri")
            || NormalizedField == TEXT("url");
    }

bool IsDescriptiveToolField(const FString& FieldName)
    {
        const FString NormalizedField = FieldName.ToLower();
        return IsPathLikeToolField(FieldName)
            || NormalizedField.Contains(TEXT("command"))
            || NormalizedField == TEXT("cmd")
            || NormalizedField.Contains(TEXT("query"))
            || NormalizedField.Contains(TEXT("pattern"))
            || NormalizedField.Contains(TEXT("description"));
    }

FString FormatRawInputToolDetail(const FString& FieldName, const FString& Value)
    {
        if (IsPathLikeToolField(FieldName))
        {
            return Value;
        }

        FString Label = FieldName.TrimStartAndEnd();
        Label.ReplaceInline(TEXT("_"), TEXT(" "));
        Label.ReplaceInline(TEXT("-"), TEXT(" "));
        return Label.IsEmpty()
            ? Value
            : FString::Printf(TEXT("%s: %s"), *Label, *Value);
    }

void CollectLocationToolDetails(const TSharedPtr<FJsonValue>& Value, TArray<FString>& Details)
    {
        if (!Value.IsValid())
        {
            return;
        }

        if (Value->Type == EJson::String)
        {
            AddToolActivityDetail(Details, Value->AsString());
            return;
        }

        if (Value->Type == EJson::Array)
        {
            for (const TSharedPtr<FJsonValue>& Entry : Value->AsArray())
            {
                CollectLocationToolDetails(Entry, Details);
            }
            return;
        }

        if (Value->Type != EJson::Object)
        {
            return;
        }

        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        if (!Object.IsValid())
        {
            return;
        }

        for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Object->Values)
        {
            if (!IsPathLikeToolField(Field.Key))
            {
                continue;
            }

            const FString ScalarText = GetJsonScalarDisplayText(Field.Value);
            if (!ScalarText.IsEmpty())
            {
                AddToolActivityDetail(Details, ScalarText);
            }
        }
    }

void CollectRawInputToolDetails(const TSharedPtr<FJsonValue>& Value, TArray<FString>& Details, int32 Depth = 0)
    {
        if (!Value.IsValid() || Depth > 2)
        {
            return;
        }

        if (Value->Type == EJson::Array)
        {
            for (const TSharedPtr<FJsonValue>& Entry : Value->AsArray())
            {
                CollectRawInputToolDetails(Entry, Details, Depth + 1);
            }
            return;
        }

        if (Value->Type != EJson::Object)
        {
            if (Depth == 0)
            {
                AddToolActivityDetail(Details, GetJsonScalarDisplayText(Value));
            }
            return;
        }

        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        if (!Object.IsValid())
        {
            return;
        }

        for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Object->Values)
        {
            const FString ScalarText = GetJsonScalarDisplayText(Field.Value);
            if (IsDescriptiveToolField(Field.Key) && !ScalarText.IsEmpty())
            {
                AddToolActivityDetail(Details, FormatRawInputToolDetail(Field.Key, ScalarText));
                continue;
            }

            if (Field.Value.IsValid() && (Field.Value->Type == EJson::Object || Field.Value->Type == EJson::Array))
            {
                CollectRawInputToolDetails(Field.Value, Details, Depth + 1);
            }
        }
    }

FString ExtractToolActivityDetail(const TSharedPtr<FJsonObject>& Update)
    {
        if (!Update.IsValid())
        {
            return FString();
        }

        TArray<FString> Details;
        CollectLocationToolDetails(Update->TryGetField(TEXT("locations")), Details);
        CollectRawInputToolDetails(Update->TryGetField(TEXT("rawInput")), Details);

        return FString::Join(Details, TEXT(", "));
    }

bool IsFinalToolActivityStatus(const FString& Status)
    {
        FString NormalizedStatus = Status.TrimStartAndEnd().ToLower();
        NormalizedStatus.ReplaceInline(TEXT("-"), TEXT("_"));
        return NormalizedStatus == TEXT("completed")
            || NormalizedStatus == TEXT("complete")
            || NormalizedStatus == TEXT("failed")
            || NormalizedStatus == TEXT("cancelled")
            || NormalizedStatus == TEXT("canceled");
    }
}
