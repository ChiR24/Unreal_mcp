#include "Domains/Sequence/Cinematics/McpSeqCine.h"

#include "GameFramework/Actor.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieScenePossessable.h"

namespace McpSequenceCinematics {
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
