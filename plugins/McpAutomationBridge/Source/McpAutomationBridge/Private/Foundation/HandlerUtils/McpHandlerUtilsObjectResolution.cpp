#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Safety/McpSafeOperations.h"
#include "EngineUtils.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetRegistryHelpers.h"
#if __has_include("EditorAssetLibrary.h")
#include "EditorAssetLibrary.h"
#else
#include "Editor/EditorAssetLibrary.h"
#endif
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "EdGraphSchema_K2.h"
#endif

namespace McpHandlerUtils
{

#if WITH_EDITOR
UObject* ResolveObjectFromPath(const FString& ObjectPath, FString* OutResolvedPath)
{
    if (ObjectPath.IsEmpty())
    {
        return nullptr;
    }

    FString Path = ObjectPath;

    // Handle component paths in "ActorName.ComponentName" format
    if (Path.Contains(TEXT(".")) && !Path.StartsWith(TEXT("/")))
    {
        FString ActorName = Path.Left(Path.Find(TEXT(".")));
        FString ComponentName = Path.Right(Path.Len() - ActorName.Len() - 1);

        if (!ActorName.IsEmpty() && !ComponentName.IsEmpty())
        {
            if (AActor* Actor = FindActorByName(ActorName))
            {
                if (UActorComponent* Comp = FindActorComponentByName(Actor, ComponentName))
                {
                    if (OutResolvedPath)
                    {
                        *OutResolvedPath = Comp->GetPathName();
                    }
                    return Comp;
                }
            }
        }
    }

    // Try to find as actor by name
    if (AActor* FoundActor = FindActorByName(Path))
    {
        if (OutResolvedPath)
        {
            *OutResolvedPath = FoundActor->GetPathName();
        }
        return FoundActor;
    }

    // Try to find by actor label (display name) as fallback
    if (GEditor)
    {
        UWorld* World = GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World();
        if (World)
        {
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                AActor* Actor = *It;
                if (Actor && (Actor->GetActorLabel().Equals(Path, ESearchCase::IgnoreCase) ||
                              Actor->GetName().Equals(Path, ESearchCase::IgnoreCase)))
                {
                    if (OutResolvedPath)
                    {
                        *OutResolvedPath = Actor->GetPathName();
                    }
                    return Actor;
                }
            }
        }
    }

    // Try to load as asset (whitelist known roots + engine-registered mount points)
    if (Path.StartsWith(TEXT("/Game/")) || Path.StartsWith(TEXT("/Engine/")) || Path.StartsWith(TEXT("/Script/")) ||
        FPackageName::IsValidLongPackageName(Path, true))
    {
        // Canonical asset resolution FIRST: LoadObject auto-resolves PackageName ->
        // PackageName.AssetName, returning a real asset (DataAsset/GE/...) as the object instead of
        // falling through to the UPackage fallback below. Additive: pure-package callers still reach
        // the old path. Guard against a bare path resolving to its own UPackage.
        if (UObject* DirectObj = LoadObject<UObject>(nullptr, *Path))
        {
            if (!DirectObj->IsA<UPackage>())
            {
                if (OutResolvedPath)
                {
                    *OutResolvedPath = DirectObj->GetPathName();
                }
                return DirectObj;
            }
        }
        if (!Path.Contains(TEXT(".")))
        {
            const FString DottedPath = Path + TEXT(".") + FPackageName::GetShortName(Path);
            if (UObject* DottedObj = LoadObject<UObject>(nullptr, *DottedPath))
            {
                // Mirror the DirectObj guard above: the dotted path can also resolve to a
                // UPackage (the exact case this fix avoids) — don't return it, fall through
                // to the package path below so genuine package callers still work.
                if (!DottedObj->IsA<UPackage>())
                {
                    if (OutResolvedPath)
                    {
                        *OutResolvedPath = DottedObj->GetPathName();
                    }
                    return DottedObj;
                }
            }
        }
        FString PackagePath = Path;
        if (PackagePath.Contains(TEXT(".")))
        {
            PackagePath = PackagePath.Left(PackagePath.Find(TEXT(".")));
        }
        UPackage* LoadedPackage = LoadPackage(nullptr, *PackagePath, LOAD_None);
        if (LoadedPackage)
        {
            if (UObject* Found = FindObject<UObject>(LoadedPackage, *Path))
            {
                if (OutResolvedPath)
                {
                    *OutResolvedPath = Found->GetPathName();
                }
                return Found;
            }
            if (OutResolvedPath)
            {
                *OutResolvedPath = LoadedPackage->GetPathName();
            }
            return LoadedPackage;
        }

        // Try StaticFindObject for engine assets that may not need package loading
        if (UObject* Found = FindObject<UObject>(nullptr, *Path))
        {
            if (OutResolvedPath)
            {
                *OutResolvedPath = Found->GetPathName();
            }
            return Found;
        }
    }

    return nullptr;
}
#endif

FPropertyResolveResult ResolveProperty(UObject* Object, const FString& PropertyName)
{
    FPropertyResolveResult Result;

    if (!Object)
    {
        Result.Error = TEXT("Object is null");
        return Result;
    }

    if (PropertyName.IsEmpty())
    {
        Result.Error = TEXT("Property name is empty");
        return Result;
    }

    // Handle nested property paths
    if (PropertyName.Contains(TEXT(".")))
    {
        Result.Property = ResolveNestedPropertyPath(Object, PropertyName, Result.Container, Result.Error);
    }
    else
    {
        // Simple property name
        Result.Container = Object;
        Result.Property = Object->GetClass()->FindPropertyByName(*PropertyName);

        if (!Result.Property)
        {
            Result.Error = FString::Printf(TEXT("Property '%s' not found on object"), *PropertyName);
        }
    }

    return Result;
}

void AddVerification(TSharedPtr<FJsonObject>& Result, UObject* Object)
{
    if (!Result.IsValid() || !Object)
    {
        return;
    }

#if WITH_EDITOR
    if (AActor* AsActor = Cast<AActor>(Object))
    {
        AddActorVerification(Result, AsActor);
    }
    else
    {
        AddAssetVerification(Result, Object);
    }
#endif
}
}
