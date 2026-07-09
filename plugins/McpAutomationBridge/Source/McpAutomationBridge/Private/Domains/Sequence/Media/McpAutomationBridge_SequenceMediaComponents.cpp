#include "Domains/Sequence/Media/McpAutomationBridge_SequenceMedia.h"

#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "GameFramework/Actor.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"

namespace McpSequenceMedia {
namespace {

AActor *FindMediaActorByName(const FString &ActorName) {
  if (ActorName.IsEmpty() || !GEditor) {
    return nullptr;
  }
  if (GEditor->PlayWorld) {
    for (TActorIterator<AActor> It(GEditor->PlayWorld.Get()); It; ++It) {
      AActor *Actor = *It;
      if (Actor && (Actor->GetName().Equals(ActorName, ESearchCase::IgnoreCase) ||
                    Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase) ||
                    Actor->GetPathName().Equals(ActorName, ESearchCase::IgnoreCase))) {
        return Actor;
      }
    }
  }
  if (UEditorActorSubsystem *ActorSubsystem =
          GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
    for (AActor *Actor : ActorSubsystem->GetAllLevelActors()) {
      if (Actor && (Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase) ||
                    Actor->GetName().Equals(ActorName, ESearchCase::IgnoreCase) ||
                    Actor->GetPathName().Equals(ActorName, ESearchCase::IgnoreCase))) {
        return Actor;
      }
    }
  }
  return McpHandlerUtils::FindActorByName(ActorName);
}

}

bool HandleCreateMediaSoundComponent(UMcpAutomationBridgeSubsystem *Subsystem,
                                     const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString ActorName =
      GetStringAny(Payload, {TEXT("actorName"), TEXT("targetActor")});
  if (ActorName.IsEmpty()) {
    SendMediaError(Subsystem, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                   TEXT("actorName is required"));
    return true;
  }
  AActor *Actor = FindMediaActorByName(ActorName);
  if (!Actor) {
    SendMediaError(
        Subsystem, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
        FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    return true;
  }

  FString Error;
  UClass *PlayerClass = ResolveMediaClass(TEXT("MediaPlayer"), Error);
  FString ResolvedPlayerPath;
  UObject *Player =
      PlayerClass
          ? LoadMediaObject(GetStringAny(Payload, {TEXT("mediaPlayerPath"),
                                                   TEXT("playerPath")}),
                            PlayerClass, ResolvedPlayerPath, Error)
          : nullptr;
  if (!Player) {
    SendMediaError(Subsystem, Socket, RequestId,
                   TEXT("MEDIA_PLAYER_NOT_FOUND"),
                   Error.IsEmpty() ? TEXT("mediaPlayerPath is required")
                                   : Error);
    return true;
  }

  UClass *ComponentClass = ResolveMediaClass(TEXT("MediaSoundComponent"), Error);
  if (!ComponentClass ||
      !ComponentClass->IsChildOf(UActorComponent::StaticClass())) {
    SendMediaError(Subsystem, Socket, RequestId,
                   TEXT("MEDIA_FRAMEWORK_UNAVAILABLE"),
                   Error.IsEmpty()
                       ? TEXT("MediaSoundComponent class is unavailable")
                       : Error);
    return true;
  }

  FString ComponentName =
      GetStringAny(Payload, {TEXT("componentName"), TEXT("name")});
  if (ComponentName.IsEmpty()) {
    ComponentName =
        FString::Printf(TEXT("MediaSound_%s"), *Player->GetName());
  }
  Actor->Modify();
  UActorComponent *Component = NewObject<UActorComponent>(
      Actor, ComponentClass, FName(*ComponentName), RF_Transactional);
  if (!Component) {
    SendMediaError(Subsystem, Socket, RequestId,
                   TEXT("COMPONENT_CREATE_FAILED"),
                   TEXT("Failed to create media sound component"));
    return true;
  }
  Component->SetFlags(RF_Transactional);
  Actor->AddInstanceComponent(Component);
  Component->OnComponentCreated();
  if (USceneComponent *SceneComponent = Cast<USceneComponent>(Component)) {
    if (Actor->GetRootComponent() && !SceneComponent->GetAttachParent()) {
      SceneComponent->SetupAttachment(Actor->GetRootComponent());
    }
  }
  if (!SetObjectProperty(Component, TEXT("MediaPlayer"), Player)) {
    SendMediaError(Subsystem, Socket, RequestId, TEXT("MEDIA_BIND_FAILED"),
                   TEXT("Failed to bind media player to media sound component"));
    Component->DestroyComponent();
    return true;
  }
  CallVoidObjectFunction(Component, TEXT("SetMediaPlayer"), Player);
  Component->RegisterComponent();
  if (GetBoolAny(Payload, {TEXT("activate")}, true)) {
    Component->Activate(true);
  }
  Component->MarkPackageDirty();
  Actor->MarkPackageDirty();

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("actorName"), ActorName);
  Result->SetStringField(TEXT("componentName"), Component->GetName());
  Result->SetStringField(TEXT("componentPath"), Component->GetPathName());
  Result->SetStringField(TEXT("mediaPlayerPath"), ResolvedPlayerPath);
  Result->SetBoolField(TEXT("registered"), Component->IsRegistered());
  Result->SetBoolField(TEXT("active"), Component->IsActive());
  McpHandlerUtils::AddVerification(Result, Component);
  Subsystem->SendAutomationResponse(
      Socket, RequestId, true, TEXT("Media sound component created"), Result);
  return true;
}

}
