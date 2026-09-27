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

MCPAUTOMATIONBRIDGE_API void AddVerification(TSharedPtr<FJsonObject>& Result, UObject* Object);
}
