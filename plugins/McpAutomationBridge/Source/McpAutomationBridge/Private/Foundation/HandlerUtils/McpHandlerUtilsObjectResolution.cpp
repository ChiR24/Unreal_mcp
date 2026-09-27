#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Safety/McpSafeOperations.h"
#include "EngineUtils.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetRegistryHelpers.h"
#include "EditorAssetLibrary.h"
#include "EdGraphSchema_K2.h"

namespace McpHandlerUtils
{

UObject* ResolveObjectFromPath(const FString& ObjectPath, FString* OutResolvedPath)
{
    if (ObjectPath.IsEmpty())
    {
        return nullptr;
    }

    const FString Path = ObjectPath;
    const auto Resolved = [OutResolvedPath](UObject* Object) -> UObject*
    {
        if (OutResolvedPath)
        {
            *OutResolvedPath = Object->GetPathName();
        }
        return Object;
    };

    // Handle component paths in "ActorName.ComponentName" format
    if (Path.Contains(TEXT(".")) && !Path.StartsWith(TEXT("/")))
    {
        FString ActorName = Path.Left(Path.Find(TEXT(".")));
        FString ComponentName = Path.Right(Path.Len() - ActorName.Len() - 1);

        if (!ActorName.IsEmpty() && !ComponentName.IsEmpty())
        {
            AActor* Actor = FindActorByName(ActorName);
            if (UActorComponent* Comp = Actor ? FindComponentByName(Actor, ComponentName) : nullptr)
            {
                return Resolved(Comp);
            }
        }
    }

    // An actor by label, name or path.
    if (AActor* FoundActor = FindActorByName(Path))
    {
        return Resolved(FoundActor);
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
                return Resolved(DirectObj);
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
                    return Resolved(DottedObj);
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
            UObject* Found = FindObject<UObject>(LoadedPackage, *Path);
            return Resolved(Found ? Found : LoadedPackage);
        }

        // Try StaticFindObject for engine assets that may not need package loading
        if (UObject* Found = FindObject<UObject>(nullptr, *Path))
        {
            return Resolved(Found);
        }
    }

    return nullptr;
}

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
            // Most misses are a component property looked up on the actor
            // (Intensity lives on LightComponent0, not the light actor). The
            // bare "not found" sent callers hunting; name the way through.
            Result.Error = FString::Printf(
                TEXT("Property '%s' not found on %s. If it belongs to a component, use get_component_property/set_component_property with that component's name (control_actor get_components lists them)."),
                *PropertyName, *Object->GetClass()->GetName());
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

    if (AActor* AsActor = Cast<AActor>(Object))
    {
        AddActorVerification(Result, AsActor);
    }
    else
    {
        AddAssetVerification(Result, Object);
    }
}
}
