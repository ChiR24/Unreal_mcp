#include "Domains/Sequence/Cinematics/McpAutomationBridge_SequenceCinematics.h"

#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"

#if WITH_EDITOR
#include "GameFramework/Actor.h"
#include "MovieScene.h"
#include "MovieScenePossessable.h"
#include "MovieSceneSpawnable.h"
#include "Particles/ParticleSystemComponent.h"
#include "UObject/UObjectHash.h"
#endif

namespace McpSequenceCinematics {
#if WITH_EDITOR
namespace {
// The class default / spawnable template owns its native FX component as a default
// subobject (ANiagaraActor's NiagaraComponent0, AEmitter's ParticleSystemComponent0).
UFXSystemComponent *FindFxComponentOnTemplate(UObject *Template) {
  if (!Cast<AActor>(Template)) return nullptr;
  TArray<UObject *> Subobjects;
  GetObjectsWithOuter(Template, Subobjects, false);
  for (UObject *Subobject : Subobjects) {
    if (UFXSystemComponent *Fx = Cast<UFXSystemComponent>(Subobject)) return Fx;
  }
  return nullptr;
}

FGuid FindExistingFxComponentBinding(UMovieScene *MovieScene,
                                     const FGuid &ParentGuid) {
  for (int32 Index = 0; Index < MovieScene->GetPossessableCount(); ++Index) {
    const FMovieScenePossessable &Possessable = MovieScene->GetPossessable(Index);
    const UClass *Class = Possessable.GetPossessedObjectClass();
    if (Possessable.GetParent() == ParentGuid && Class &&
        Class->IsChildOf(UFXSystemComponent::StaticClass())) {
      return Possessable.GetGuid();
    }
  }
  return FGuid();
}
}
#endif

// The particle evaluator only resolves AEmitter or UFXSystemComponent objects, so an
// actor that merely owns a Niagara/particle component (ANiagaraActor, a Blueprint
// actor, ...) gets its track on a child component binding, which is exactly what the
// Sequencer UI creates for "Add Track > Particle" on such actors (dogfood #123).
FGuid ResolveParticleComponentBinding(ULevelSequence *Sequence,
                                      const FGuid &ActorGuid,
                                      TSharedPtr<FJsonObject> &OutDetails) {
#if WITH_EDITOR
  OutDetails = McpHandlerUtils::CreateResultObject();
  UMovieScene *MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
  if (!MovieScene || !ActorGuid.IsValid()) return FGuid();
  const FGuid Existing = FindExistingFxComponentBinding(MovieScene, ActorGuid);
  if (Existing.IsValid()) {
    OutDetails->SetStringField(TEXT("bindingResolvedBy"),
                               TEXT("existing-component-binding"));
    return Existing;
  }

  // Live actor first (possessables placed in the editor world), then the class
  // default / spawnable template so unplaced and spawnable actors resolve too.
  UFXSystemComponent *Component = nullptr;
  UObject *Context = nullptr;
  FString ResolvedBy;
  UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  if (World) {
    TArray<UObject *, TInlineAllocator<1>> BoundObjects;
    LocateBindingObjects(Sequence, ActorGuid, World, BoundObjects);
    for (UObject *Object : BoundObjects) {
      AActor *Actor = Cast<AActor>(Object);
      UActorComponent *Found =
          Actor ? Actor->FindComponentByClass(UFXSystemComponent::StaticClass())
                : nullptr;
      if (Found) {
        Component = Cast<UFXSystemComponent>(Found);
        Context = Actor;
        ResolvedBy = TEXT("live-actor-component");
        break;
      }
    }
  }
  if (!Component) {
    const FMovieScenePossessable *Possessable = MovieScene->FindPossessable(ActorGuid);
    FMovieSceneSpawnable *Spawnable = MovieScene->FindSpawnable(ActorGuid);
    const UClass *BoundClass =
        Possessable ? Possessable->GetPossessedObjectClass() : nullptr;
    UObject *Template = BoundClass ? BoundClass->GetDefaultObject()
                                   : (Spawnable ? Spawnable->GetObjectTemplate() : nullptr);
    Component = FindFxComponentOnTemplate(Template);
    if (Component) {
      Context = Template;
      ResolvedBy = Spawnable ? TEXT("spawnable-template-component")
                             : TEXT("class-default-component");
    }
  }
  if (!Component) return FGuid();

  const FGuid ComponentGuid =
      MovieScene->AddPossessable(Component->GetName(), Component->GetClass());
  if (!ComponentGuid.IsValid()) return FGuid();
  if (FMovieScenePossessable *ComponentPossessable =
          MovieScene->FindPossessable(ComponentGuid)) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    ComponentPossessable->SetParent(ActorGuid, MovieScene);
#else
    ComponentPossessable->SetParent(ActorGuid);
#endif
  }
  // The reference is relative to the owning actor, so a default-subobject name
  // ("NiagaraComponent0") resolves on the placed or spawned instance as well.
  Sequence->BindPossessableObject(ComponentGuid, *Component, Context);
  MovieScene->Modify();
  OutDetails->SetStringField(TEXT("bindingResolvedBy"), ResolvedBy);
  OutDetails->SetStringField(TEXT("boundComponent"), Component->GetName());
  OutDetails->SetStringField(TEXT("boundComponentClass"),
                             Component->GetClass()->GetName());
  OutDetails->SetBoolField(TEXT("componentBindingCreated"), true);
  return ComponentGuid;
#else
  OutDetails = McpHandlerUtils::CreateResultObject();
  return FGuid();
#endif
}
}
