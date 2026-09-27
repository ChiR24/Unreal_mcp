#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"

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


// NameOrPath unchanged when it carries a folder, else beside SourcePath (/Game for a root-level source).
inline FString ResolveSiblingAssetPath(const FString& SourcePath, const FString& NameOrPath)
{
    if (NameOrPath.IsEmpty() || !FPaths::GetPath(NameOrPath).IsEmpty())
    {
        return NameOrPath;
    }
    const FString ParentDir = FPaths::GetPath(SourcePath);
    return (ParentDir.IsEmpty() || ParentDir == TEXT("/") ? FString(TEXT("/Game")) : ParentDir) / NameOrPath;
}

inline FString ExtractAssetName(const FString& Path)
{
    int32 LastSlash = INDEX_NONE;
    return Path.FindLastChar('/', LastSlash) ? Path.Mid(LastSlash + 1) : Path;
}

// The editor world (not PIE); null without an editor.
MCPAUTOMATIONBRIDGE_API UWorld* GetEditorWorld();
// Label, name or path (case-insensitive) in the PIE world, else the editor world.
MCPAUTOMATIONBRIDGE_API AActor* FindActorByName(const FString& ActorName);
MCPAUTOMATIONBRIDGE_API UObject* ResolveObjectFromPath(
    const FString& ObjectPath,
    FString* OutResolvedPath = nullptr);

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
}

class AActor;
class UWorld;

// Label, name or path (case-insensitive); when !bExactMatchOnly, also a unique
// label substring. Defined in Domains/ControlActor/...ControlActorResolution.cpp.
AActor* FindActorByNameInWorldForMcp(UWorld* World, const FString& Target, bool bExactMatchOnly);

// The actor of class T whose label, name or path is Target (case-insensitive).
template <class T>
T* FindActorOfClassForMcp(UWorld* World, const FString& Target)
{
	if (!World || Target.IsEmpty())
	{
		return nullptr;
	}
	for (TActorIterator<T> It(World); It; ++It)
	{
		if (It->GetActorLabel().Equals(Target, ESearchCase::IgnoreCase) ||
			It->GetName().Equals(Target, ESearchCase::IgnoreCase) ||
			It->GetPathName().Equals(Target, ESearchCase::IgnoreCase))
		{
			return *It;
		}
	}
	return nullptr;
}
