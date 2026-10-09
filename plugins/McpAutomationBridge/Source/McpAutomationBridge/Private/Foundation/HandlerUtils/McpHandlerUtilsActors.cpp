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
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetRegistryHelpers.h"
#include "EditorAssetLibrary.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "EdGraphSchema_K2.h"

namespace McpHandlerUtils
{

UWorld* GetEditorWorld()
{
    return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

AActor* FindActorByName(const FString& ActorName)
{
    UWorld* World = GEditor ? (GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World()) : nullptr;
    if (AActor* Actor = FindActorByNameInWorldForMcp(World, ActorName, true))
    {
        return Actor;
    }
    // While PIE runs, PlayerPawn, PlayerController, GameMode ... name the running game's actors by role, as control_actor
    // takes them: raycast_world ignoreActors refused PlayerPawn.
    return Cast<AActor>(ResolveRuntimeRole(ActorName));
}

void AddMeshAssetFields(const UActorComponent* Component, const TSharedPtr<FJsonObject>& Entry)
{
    if (const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Component))
    {
        Entry->SetStringField(TEXT("staticMesh"), Mesh->GetStaticMesh() ? Mesh->GetStaticMesh()->GetPathName() : TEXT(""));
    }
    if (const UMeshComponent* MeshComponent = Cast<UMeshComponent>(Component))
    {
        TArray<TSharedPtr<FJsonValue>> Materials;
        for (UMaterialInterface* Material : MeshComponent->GetMaterials())
        {
            Materials.Add(MakeShared<FJsonValueString>(Material ? Material->GetPathName() : TEXT("")));
        }
        Entry->SetArrayField(TEXT("materials"), Materials);
    }
}
}
