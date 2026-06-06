#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace McpHandlerUtils
{
inline TSharedPtr<FJsonObject> BuildSuccessResponse(
    const FString& Message,
    const TSharedPtr<FJsonObject>& Data = nullptr)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), Message);
    if (Data.IsValid())
    {
        Response->SetObjectField(TEXT("data"), Data);
    }
    return Response;
}

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

inline TSharedPtr<FJsonObject> CreateResultObject(const FString& Key, const FString& Value)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(Key, Value);
    return Result;
}

inline TSharedPtr<FJsonObject> CreateNamedResult(const FString& Name, const FString& Path)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), Name);
    Result->SetStringField(TEXT("path"), Path);
    return Result;
}

MCPAUTOMATIONBRIDGE_API void AddVerification(TSharedPtr<FJsonObject>& Result, UObject* Object);
}
