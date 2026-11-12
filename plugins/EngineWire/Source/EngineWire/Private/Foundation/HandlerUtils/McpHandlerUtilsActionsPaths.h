#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"
#include "Foundation/McpCapabilityPrincipal.h"

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
ENGINEWIRE_API UWorld* GetEditorWorld();
// Label, name or path (case-insensitive) in the PIE world, else the editor world; while PIE runs, also a role
// ResolveRuntimeRole names (PlayerPawn, PlayerController, GameMode ...).
ENGINEWIRE_API AActor* FindActorByName(const FString& ActorName);
// A mesh component's staticMesh path and the material drawn in each slot (materials).
ENGINEWIRE_API void AddMeshAssetFields(const UActorComponent* Component, const TSharedPtr<FJsonObject>& Entry);
ENGINEWIRE_API UObject* ResolveObjectFromPath(
    const FString& ObjectPath,
    FString* OutResolvedPath = nullptr);
// While PIE runs: GameInstance, GameMode, GameState, PlayerController,
// PlayerPawn, PlayerState or HUD of the running game; nullptr otherwise.
ENGINEWIRE_API UObject* ResolveRuntimeRole(const FString& Role);
// "Unable to find object at path X." plus the names that reach runtime objects.
ENGINEWIRE_API FString DescribeObjectNotFound(const FString& ObjectPath);

struct FPropertyResolveResult
{
    FProperty* Property = nullptr;
    void* Container = nullptr;
    FString Error;
    bool IsValid() const { return Property != nullptr && Container != nullptr; }
};

ENGINEWIRE_API FPropertyResolveResult ResolveProperty(
    UObject* Object,
    const FString& PropertyName);
}

class AActor;
class UWorld;

// Label, name or path (case-insensitive); when !bExactMatchOnly, also a unique
// label substring. A path-restricted caller finds only actors in a level inside its prefixes.
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
		// A path-restricted caller finds only actors in a level inside its prefixes.
		if ((It->GetActorLabel().Equals(Target, ESearchCase::IgnoreCase) ||
			It->GetName().Equals(Target, ESearchCase::IgnoreCase) ||
			It->GetPathName().Equals(Target, ESearchCase::IgnoreCase)) &&
			McpCapabilityPrincipal::ActiveRequestPermitsObject(*It))
		{
			return *It;
		}
	}
	return nullptr;
}

// A name compared loosely: lower case, spaces and underscores dropped ("Path Tracing", "path_tracing" and
// "PathTracing" all read "pathtracing").
inline FString McpPlainKey(const FString& Text)
{
	return Text.Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT("")).ToLower();
}

// "/Game/A/B" names the asset B inside package /Game/A/B (its object path is /Game/A/B.B); an object path passes
// through. A loader takes either; the asset registry and FindObject take only the object path.
inline FString McpToObjectPath(const FString& Path)
{
	return Path.Contains(TEXT(".")) ? Path : Path + TEXT(".") + FPackageName::GetShortName(Path);
}
