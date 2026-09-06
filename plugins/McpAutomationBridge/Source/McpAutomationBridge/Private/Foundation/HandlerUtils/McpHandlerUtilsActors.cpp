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
    return FindComponentByName(Actor, ComponentName);
}
#endif
}
