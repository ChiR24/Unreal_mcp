#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace McpHandlerUtils
{
inline TSharedPtr<FJsonObject> BuildErrorResponse(
    const FString& ErrorCode,
    const FString& Message,
    const TSharedPtr<FJsonObject>& Details = nullptr)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), false);
    Response->SetStringField(TEXT("error"), Message);
    Response->SetStringField(TEXT("code"), ErrorCode);
    if (Details.IsValid())
    {
        Response->SetObjectField(TEXT("details"), Details);
    }
    return Response;
}

inline TSharedPtr<FJsonObject> CreateResultObject()
{
    return MakeShared<FJsonObject>();
}

ENGINEWIRE_API void AddVerification(TSharedPtr<FJsonObject>& Result, UObject* Object);

// A reply that names an asset it only looked at or used (a widget preview, a sound that played)
// states that it changed none. The receipt's changes[] then skips the assetPath/widgetPath it
// would otherwise infer as a change (see McpExtractReceiptChanges / extractChanges).
inline void MarkNoAssetsChanged(const TSharedPtr<FJsonObject>& Result)
{
    if (Result.IsValid())
    {
        Result->SetArrayField(TEXT("changedAssets"), TArray<TSharedPtr<FJsonValue>>());
    }
}

// Adds Text to the reply's warnings[], after the ones it already holds.
inline void AppendWarning(const TSharedPtr<FJsonObject>& Result, const FString& Text)
{
    TArray<TSharedPtr<FJsonValue>> Warnings;
    const TArray<TSharedPtr<FJsonValue>>* Existing = nullptr;
    if (Result->TryGetArrayField(TEXT("warnings"), Existing) && Existing)
    {
        Warnings = *Existing;
    }
    Warnings.Add(MakeShared<FJsonValueString>(Text));
    Result->SetArrayField(TEXT("warnings"), Warnings);
}
}
