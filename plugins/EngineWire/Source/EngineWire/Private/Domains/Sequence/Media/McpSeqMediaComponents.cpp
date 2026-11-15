#include "Domains/Sequence/Media/McpSeqMedia.h"

#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "GameFramework/Actor.h"
#include "EngineWireSubsystem.h"
#include "MediaPlayer.h"
#include "MediaSoundComponent.h"
#include "Subsystems/EditorActorSubsystem.h"

namespace McpSequenceMedia {
namespace {

AActor *FindMediaActorByName(const FString &ActorName) {
  if (ActorName.IsEmpty() || !GEditor) {
    return nullptr;
  }
  // The editor level, while Play runs too: a component added to the running game's actor ended with the session.
  return FindActorByNameInWorldForMcp(McpHandlerUtils::GetEditorWorld(), ActorName, true);
}

}

bool HandleCreateMediaSoundComponent(UEngineWireSubsystem *Subsystem,
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
  // NewObject over a component of that name (a second call for the same player) collided with it; refuse instead.
  if (FindObject<UObject>(Actor, *ComponentName)) {
    SendMediaError(Subsystem, Socket, RequestId, TEXT("NAME_TAKEN"),
                   FString::Printf(TEXT("%s already has a component named %s; pick another componentName. Nothing was added."),
                                   *Actor->GetActorLabel(), *ComponentName));
    return true;
  }
  // One undo step for the add: Modify ran with no transaction open, so undo left the component in place.
  FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Add Media Sound Component")),
                                          EMcpMutationDurability::EditorStateOnly, {Actor});
  UActorComponent *Component = NewObject<UActorComponent>(
      Actor, ComponentClass, FName(*ComponentName), RF_Transactional);
  if (!Component) {
    Transaction.Cancel();
    SendMediaError(Subsystem, Socket, RequestId,
                   TEXT("COMPONENT_CREATE_FAILED"),
                   TEXT("Failed to create media sound component"));
    return true;
  }
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
    Transaction.Cancel();
    return true;
  }
  if (UMediaSoundComponent *Sound = Cast<UMediaSoundComponent>(Component)) {
    Sound->SetMediaPlayer(Cast<UMediaPlayer>(Player));
  }
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
  Transaction.DescribeInto(Result);
  Subsystem->SendAutomationResponse(
      Socket, RequestId, true, TEXT("Media sound component created"), Result);
  return true;
}

}
