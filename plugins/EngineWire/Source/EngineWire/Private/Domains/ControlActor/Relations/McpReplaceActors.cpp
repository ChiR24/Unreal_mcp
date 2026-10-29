// control_actor replace_actors: placed actors swapped for a static mesh, a Blueprint or an actor class where they stand,
// as the editor's Replace Selected Actors with does, through the engine's own ReplaceActors (UEditorEngine's to 5.4,
// UEditorActorSubsystem's from 5.5, where the first became deprecated; the same work in every 5.x): each new actor
// takes the old one's transform, scale, mobility, layers, object name, attachments and the level's references to it, and
// a Blueprint related to the old class by a native parent takes its property values. The label, tags and outliner
// folder are set from the old actor here, which the engine itself does only from 5.1 (EditorReplacedActor). Every name,
// the asset and a factory that places it (the editor's own question) are checked before anything changes; one undo step.

#include "Domains/ControlActor/Relations/McpRelations.h"

#include "ActorEditorUtils.h"
#include "ActorFactories/ActorFactory.h"
#include "AssetRegistry/AssetData.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Foundation/BridgeHelpers/Reflection/McpClassResolve.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/BridgeHelpers/Responses/McpResponseVerify.h"
#include "Foundation/BridgeHelpers/Security/McpProjectPaths.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "GameFramework/Actor.h"
#include "Subsystems/EditorActorSubsystem.h"

namespace McpRelations
{
namespace
{
/** What the old actor keeps across the swap: the engine names the new actor after it, in its level. */
struct FOld
{
    AActor* Actor = nullptr;
    ULevel* Level = nullptr;
    FName Name;
    FString Label;
    TArray<FName> Tags;
    FName Folder;
};

/** A static mesh (meshPath), a Blueprint (blueprintPath) or an actor class (classPath); null when it does not load. */
UObject* LoadReplacement(const FString& Mesh, const FString& Blueprint, const FString& Class)
{
    if (!Class.IsEmpty())
    {
        UClass* Resolved = ResolveClassByName(Class);
        return Resolved && Resolved->IsChildOf(AActor::StaticClass()) ? Resolved : nullptr;
    }
    const FString Safe = SanitizeProjectRelativePath(Mesh.IsEmpty() ? Blueprint : Mesh);
    UObject* Loaded = Safe.IsEmpty() ? nullptr : McpLoadAsset(Safe);
    return Mesh.IsEmpty() ? static_cast<UObject*>(Cast<UBlueprint>(Loaded)) : static_cast<UObject*>(Cast<UStaticMesh>(Loaded));
}

/** The factory the editor places Asset with: the first of its actor factories that can (FActorFactoryAssetProxy's loop). */
UActorFactory* FactoryFor(const FAssetData& Asset)
{
    FText Unused;
    for (UActorFactory* Factory : GEditor->ActorFactories)
    {
        if (Factory && Factory->CanCreateActorFrom(Asset, Unused))
        {
            return Factory;
        }
    }
    return nullptr;
}
} // namespace

bool HandleReplaceActors(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                         TSharedPtr<FMcpBridgeWebSocket> Socket, TFunctionRef<AActor*(const FString&)> Find)
{
    const auto Refuse = [&](const FString& Message, const TCHAR* Code)
    {
        Bridge->SendAutomationError(Socket, RequestId, Message + TEXT(" Nothing was replaced."), Code);
        return true;
    };
    const FString Mesh = GetJsonStringField(Payload, TEXT("meshPath"));
    const FString Blueprint = GetJsonStringField(Payload, TEXT("blueprintPath"));
    const FString Class = GetJsonStringField(Payload, TEXT("classPath"));
    const int32 Named = (Mesh.IsEmpty() ? 0 : 1) + (Blueprint.IsEmpty() ? 0 : 1) + (Class.IsEmpty() ? 0 : 1);
    if (Named != 1)
    {
        return Refuse(TEXT("Name what replaces the actors with exactly one of meshPath, blueprintPath or classPath."), TEXT("INVALID_ARGUMENT"));
    }
    UObject* Asset = LoadReplacement(Mesh, Blueprint, Class);
    if (!Asset)
    {
        return Refuse(Mesh.IsEmpty() ? FString::Printf(TEXT("No %s at %s."), Class.IsEmpty() ? TEXT("Blueprint") : TEXT("actor class"), Class.IsEmpty() ? *Blueprint : *Class)
                                     : FString::Printf(TEXT("No static mesh at %s."), *Mesh),
                      Mesh.IsEmpty() ? TEXT("CLASS_NOT_FOUND") : TEXT("MESH_NOT_FOUND"));
    }
    const FAssetData AssetData(Asset);
    // An abstract, deprecated or not-placeable class passes the class factory's test and then fails to spawn.
    const UClass* AsClass = Cast<UClass>(Asset);
    UActorFactory* Factory = AsClass && AsClass->HasAnyClassFlags(CLASS_NotPlaceable | CLASS_Deprecated | CLASS_Abstract) ? nullptr : FactoryFor(AssetData);
    if (!Factory)
    {
        return Refuse(FString::Printf(TEXT("The editor has no way to place %s as an actor."), *Asset->GetPathName()), TEXT("CANNOT_PLACE"));
    }
    TArray<FOld> Olds;
    TArray<AActor*> Actors;
    for (const FString& Name : McpHandlerUtils::GetStringArrayField(Payload, TEXT("actorNames")))
    {
        AActor* Actor = Find(Name);
        if (!Actor)
        {
            return Refuse(FString::Printf(TEXT("No actor is labelled or named %s."), *Name), TEXT("ACTOR_NOT_FOUND"));
        }
        if (!Actor->IsUserManaged() || FActorEditorUtils::IsABuilderBrush(Actor))
        {
            return Refuse(FString::Printf(TEXT("%s is managed by the editor and cannot be replaced."), *Actor->GetActorLabel()), TEXT("ACTOR_NOT_REPLACEABLE"));
        }
        if (!Actors.Contains(Actor))
        {
            Actors.Add(Actor);
            Olds.Add({Actor, Actor->GetLevel(), Actor->GetFName(), Actor->GetActorLabel(), Actor->Tags, Actor->GetFolderPath()});
        }
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> Rows;
    TArray<FString> NotReplaced;
    {
        FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Replace Actors")), EMcpMutationDurability::EditorStateOnly,
            TArray<UObject*>(Actors));
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 5
        UEditorActorSubsystem::ReplaceActors(Factory, AssetData, Actors);
#else
        GEditor->ReplaceActors(Factory, AssetData, Actors);
#endif
        for (const FOld& Old : Olds)
        {
            // The engine spawns the new actor under the old one's object name, in its level; a failed one keeps the old.
            AActor* New = Cast<AActor>(StaticFindObjectFast(AActor::StaticClass(), Old.Level, Old.Name));
            if (!New || New == Old.Actor)
            {
                NotReplaced.Add(Old.Label);
                continue;
            }
            New->SetActorLabel(Old.Label);
            New->Tags = Old.Tags;
            New->SetFolderPath(Old.Folder);
            TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("from"), Old.Label);
            Row->SetStringField(TEXT("actorName"), McpActorRef(New));
            Row->SetStringField(TEXT("class"), New->GetClass()->GetName());
            Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
        Transaction.DescribeInto(Result);
    }
    if (Rows.Num() == 0)
    {
        Bridge->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("The editor placed none of the %d replacement(s) (see the log)."), Olds.Num()), TEXT("REPLACE_FAILED"));
        return true;
    }
    Result->SetArrayField(TEXT("replaced"), Rows);
    if (NotReplaced.Num() > 0)
    {
        Result->SetArrayField(TEXT("notReplaced"), McpHandlerUtils::ToJsonStringArray(NotReplaced));
    }
    Bridge->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Replaced %d of %d actor(s) with %s."), Rows.Num(), Olds.Num(), *Asset->GetName()), Result);
    return true;
}
} // namespace McpRelations
