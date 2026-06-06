#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpHandlerUtilsTransforms.h"

class FMcpBridgeWebSocket;
class AActor;
class UActorComponent;

namespace McpHandlerUtils
{
inline FString NormalizeAction(const FString& Action, const TSharedPtr<FJsonObject>& Payload = nullptr)
{
    FString Normalized = Action.ToLower();
    if (Payload.IsValid())
    {
        FString SubAction;
        if (Payload->TryGetStringField(TEXT("subAction"), SubAction) && !SubAction.IsEmpty())
        {
            Normalized = SubAction.ToLower();
        }
    }
    return Normalized;
}

inline bool ActionMatches(const FString& Action, const FString& Pattern)
{
    return Action.ToLower().Equals(Pattern.ToLower());
}

inline bool ActionMatchesAny(const FString& Action, const TArray<FString>& Patterns)
{
    const FString LowerAction = Action.ToLower();
    for (const FString& Pattern : Patterns)
    {
        if (LowerAction.Equals(Pattern.ToLower()))
        {
            return true;
        }
    }
    return false;
}

MCPAUTOMATIONBRIDGE_API FString ValidateAssetPath(const FString& Path);

inline FString ExtractAssetName(const FString& Path)
{
    int32 LastSlash = INDEX_NONE;
    return Path.FindLastChar('/', LastSlash) ? Path.Mid(LastSlash + 1) : Path;
}

inline FString ExtractPackagePath(const FString& AssetPath)
{
    int32 LastSlash = INDEX_NONE;
    return AssetPath.FindLastChar('/', LastSlash) && LastSlash > 0 ? AssetPath.Left(LastSlash) : AssetPath;
}

#if WITH_EDITOR
MCPAUTOMATIONBRIDGE_API AActor* FindActorByName(const FString& ActorName, bool bExactMatch = true);
MCPAUTOMATIONBRIDGE_API UActorComponent* FindActorComponentByName(
    AActor* Actor,
    const FString& ComponentName);
MCPAUTOMATIONBRIDGE_API UObject* ResolveObjectFromPath(
    const FString& ObjectPath,
    FString* OutResolvedPath = nullptr);
#endif

MCPAUTOMATIONBRIDGE_API FString ToSafeAssetName(const FString& Input);
MCPAUTOMATIONBRIDGE_API FString MakeUniqueAssetName(const FString& BaseName, const FString& PackagePath);

inline void LogAutomationRequest(const FString& RequestId, const FString& Action, const FString& PayloadPreview)
{
    UE_LOG(LogTemp, Verbose, TEXT("[MCP] Request %s: Action='%s' Payload='%s'"),
        *RequestId, *Action, *PayloadPreview.Left(200));
}

struct FPropertyResolveResult
{
    FProperty* Property = nullptr;
    void* Container = nullptr;
    FString Error;
    bool IsValid() const { return Property != nullptr && Container != nullptr; }
};

MCPAUTOMATIONBRIDGE_API FPropertyResolveResult ResolveProperty(
    UObject* Object,
    const FString& PropertyName);

inline FString TruncateForLog(const FString& Input, int32 MaxLength = 256)
{
    return Input.Len() <= MaxLength ? Input : Input.Left(MaxLength) + TEXT("...");
}
}
