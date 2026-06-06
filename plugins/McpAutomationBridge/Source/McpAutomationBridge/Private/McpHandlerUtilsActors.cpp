#include "McpHandlerUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpSafeOperations.h"
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
AActor* FindActorByName(const FString& ActorName, bool bExactMatch)
{
    UWorld* World = nullptr;
    if (GEditor)
    {
        World = GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World();
    }
    if (!World)
    {
        return nullptr;
    }

    FString SearchName = ActorName;
    if (bExactMatch)
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            if (Actor && Actor->GetName().Equals(SearchName, ESearchCase::IgnoreCase))
            {
                return Actor;
            }
        }
    }
    else
    {
        // Partial match - actors starting with the name
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            if (Actor && Actor->GetName().StartsWith(SearchName, ESearchCase::IgnoreCase))
            {
                return Actor;
            }
        }
    }

    return nullptr;
}

UActorComponent* FindActorComponentByName(AActor* Actor, const FString& ComponentName)
{
    if (!Actor || ComponentName.IsEmpty())
    {
        return nullptr;
    }

    const FString Needle = ComponentName.ToLower();
    UActorComponent* ExactMatch = nullptr;
    UActorComponent* StartsWithMatch = nullptr;

    TArray<UActorComponent*> Components;
    Actor->GetComponents(Components);

    for (UActorComponent* Comp : Components)
    {
        if (!Comp)
        {
            continue;
        }

        const FString CompName = Comp->GetName().ToLower();
        const FString CompPath = Comp->GetPathName().ToLower();

        // Exact name match (highest priority)
        if (CompName.Equals(Needle))
        {
            return Comp;
        }

        // Exact path match
        if (CompPath.Equals(Needle))
        {
            return Comp;
        }

        // Path ends with .ComponentName
        if (CompPath.EndsWith(FString::Printf(TEXT(".%s"), *Needle)))
        {
            return Comp;
        }

        // Path ends with :ComponentName (subobject format)
        if (CompPath.EndsWith(FString::Printf(TEXT(":%s"), *Needle)))
        {
            return Comp;
        }

        // Partial match: ComponentName starts with needle
        if (CompName.StartsWith(Needle) && !StartsWithMatch)
        {
            StartsWithMatch = Comp;
        }

        // Path contains the component name
        if (!ExactMatch && CompPath.Contains(Needle))
        {
            ExactMatch = Comp;
        }
    }

    // Return in priority order
    if (StartsWithMatch)
    {
        return StartsWithMatch;
    }

    return ExactMatch;
}
#endif
}
