#include "Domains/Sequence/Cinematics/McpSeqCine.h"

#include "GameFramework/Actor.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieScenePossessable.h"

namespace McpSequenceCinematics {
FNewBinding::~FNewBinding() {
  if (Sequence && Guid.IsValid()) {
    Sequence->UnbindPossessableObjects(Guid);
    Sequence->GetMovieScene()->RemovePossessable(Guid);
  }
}

FGuid BindActor(ULevelSequence *Sequence, AActor *Actor, FNewBinding &New) {
  const FGuid Existing = Actor ? FindExistingBinding(Sequence, Actor, Actor->GetWorld()) : FGuid();
  if (Existing.IsValid()) {
    return Existing;
  }
  const FGuid Made = ResolveOrCreateBinding(Sequence, Actor);
  if (Made.IsValid()) {
    New.Sequence = Sequence;
    New.Guid = Made;
  }
  return Made;
}

bool CheckRequestedBinding(const TSharedPtr<FJsonObject> &Params, UMovieScene *MovieScene, const TCHAR *Action,
                           TSharedPtr<FJsonObject> &OutResult) {
  FGuid Guid;
  if (!ReadBindingGuid(Params, Guid) || MovieScene->FindBinding(Guid)) {
    return true;
  }
  OutResult = MakeResult(false, Action,
                         FString::Printf(TEXT("bindingGuid %s names no binding of this sequence (get_bindings lists them)."),
                                         *Guid.ToString()),
                         TEXT("BINDING_NOT_FOUND"));
  return false;
}

FGuid RebindPossessableByName(ULevelSequence *Sequence, AActor *Actor) {
  UMovieScene *MovieScene = Sequence && Actor ? Sequence->GetMovieScene() : nullptr;
  if (!MovieScene) {
    return FGuid();
  }
  const FString Label = Actor->GetActorLabel();
  for (int32 Index = 0; Index < MovieScene->GetPossessableCount(); ++Index) {
    const FMovieScenePossessable &Possessable = MovieScene->GetPossessable(Index);
    // A component binding sits under its actor's binding and is found through it, never by name.
    if (Possessable.GetParent().IsValid() || Possessable.GetName() != Label) {
      continue;
    }
    const FGuid Guid = Possessable.GetGuid();
    Sequence->Modify();
    Sequence->UnbindPossessableObjects(Guid);
    Sequence->BindPossessableObject(Guid, *Actor, Actor->GetWorld());
    return Guid;
  }
  return FGuid();
}
} // namespace McpSequenceCinematics
