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
#include "Engine/GameInstance.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Blueprint/UserWidget.h"
#include "UObject/UObjectIterator.h"

namespace McpHandlerUtils
{

FString DescribeObjectNotFound(const FString& ObjectPath)
{
    return FString::Printf(
        TEXT("Unable to find object at path %s. Actors are found by name or label; while PIE runs, GameInstance, "
             "GameMode, GameState, PlayerController, PlayerPawn, PlayerState and HUD name those runtime objects, "
             "and a live widget goes by the name simulate_input widget_list reports (WBP_HUD_C_0 or WBP_HUD)."),
        *ObjectPath);
}

// The running game's objects by role. Their paths are transient
// (/Engine/Transient.UnrealEdEngine_0:BP_MyGI_C_3), so no caller could guess one.
UObject* ResolveRuntimeRole(const FString& Role)
{
    UWorld* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
    if (!World)
    {
        return nullptr;
    }
    APlayerController* PC = World->GetFirstPlayerController();
    const auto Is = [&Role](const TCHAR* Name) { return Role.Equals(Name, ESearchCase::IgnoreCase); };
    if (Is(TEXT("GameInstance"))) return World->GetGameInstance();
    if (Is(TEXT("GameMode"))) return World->GetAuthGameMode();
    if (Is(TEXT("GameState"))) return World->GetGameState();
    if (Is(TEXT("PlayerController"))) return PC;
    if (Is(TEXT("PlayerPawn"))) return PC ? PC->GetPawn() : nullptr;
    if (Is(TEXT("PlayerState"))) return PC ? PC->GetPlayerState<APlayerState>() : nullptr;
    if (Is(TEXT("HUD"))) return PC ? PC->GetHUD() : nullptr;
    // A live UMG widget by the object or Widget Blueprint name widget_list reports (WBP_HUD_C_0, WBP_HUD):
    // widget_list named it, then get_property could not reach it. Its tree widgets are properties
    // (CoinBox.RenderTransform).
    for (TObjectIterator<UUserWidget> It; It; ++It)
    {
        FString ClassName = It->GetClass()->GetName();
        ClassName.RemoveFromEnd(TEXT("_C"));
        if (It->GetWorld() == World && (It->GetName().Equals(Role, ESearchCase::IgnoreCase) ||
                                        ClassName.Equals(Role, ESearchCase::IgnoreCase)))
        {
            return *It;
        }
    }
    return nullptr;
}

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

    if (UObject* RuntimeObject = ResolveRuntimeRole(Path))
    {
        return Resolved(RuntimeObject);
    }

    // Try to load as asset (whitelist known roots + engine-registered mount points). The package part is what must be
    // valid: an object path (/Temp/X.X, /Niagara/...) is never a valid package name, so those mounts only took /Temp/X.
    if (Path.StartsWith(TEXT("/Game/")) || Path.StartsWith(TEXT("/Engine/")) || Path.StartsWith(TEXT("/Script/")) ||
        FPackageName::IsValidLongPackageName(FPackageName::ObjectPathToPackageName(Path), true))
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
            if (UObject* Found = FindObject<UObject>(LoadedPackage, *Path))
            {
                return Resolved(Found);
            }
            // Only a caller who named the package itself gets the package. A
            // missing object inside it answered as the package, so the next
            // error blamed a property "not found on Package".
            if (PackagePath == Path)
            {
                return Resolved(LoadedPackage);
            }
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

    // The shared resolver set_property and the component actions use: dotted
    // paths at any depth, and a bare name that lives in one struct member.
    FString ResolvedPath;
    Result.Property = McpResolvePropertyPath(Object, PropertyName, Result.Container, ResolvedPath, Result.Error);
    if (!Result.Property && !PropertyName.Contains(TEXT(".")))
    {
        // Most misses are a component property looked up on the actor
        // (Intensity lives on LightComponent0, not the light actor). The
        // bare "not found" sent callers hunting; name the way through.
        Result.Error = FString::Printf(
            TEXT("Property '%s' not found on %s (%s). If it belongs to a component, use get_component_property or set_component_property with that component's name (control_actor get_components lists them)."),
            *PropertyName, *Object->GetClass()->GetName(), *Result.Error);
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
