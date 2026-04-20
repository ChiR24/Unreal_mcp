// McpPropertyPath.h
#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonValue.h"

namespace McpPropertyPath
{
    /** Walk a dotted/indexed path like "Stats.Health" or "Effects.[0].Value" and SET the target JSON value. */
    bool SetValueAtPath(
        UObject* RootObject,
        const FString& PropertyPath,
        const TSharedPtr<FJsonValue>& Value,
        FString& OutError);

    /** Walk a path and READ the target as a JSON value. Returns null on miss. */
    TSharedPtr<FJsonValue> GetValueAtPath(
        UObject* RootObject,
        const FString& PropertyPath,
        FString& OutError);
}
