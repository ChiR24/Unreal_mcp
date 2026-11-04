// The plans of the behaviours that set up a place rather than a pawn: a sound that plays when a player walks into the
// actor's area (recipe Gameplay/SoundTrigger), and a cutscene that plays there (Gameplay/SequenceTrigger). A sound on
// entering an area had no gameplay action: the nearest answers were a sound cue edit and an ambient sound that plays
// whoever is there; a cutscene had only the editor preview. McpBPAddBehaviour.cpp runs the plans.
#include "Domains/Blueprint/Gameplay/McpBPBehaviourPlans.h"

#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"

#include "LevelSequence.h"

namespace McpBlueprintHandlers {
bool PlanSoundTrigger(const TSharedPtr<FJsonObject> &Payload, FBehaviourPlan &Plan, FString &OutError) {
  if (GetJsonStringField(Payload, TEXT("soundPath")).TrimStartAndEnd().IsEmpty()) {
    OutError = TEXT("soundPath must name the sound to play");
    return false;
  }
  if (!ReadSound(Payload, Plan, OutError)) {
    return false;
  }
  Plan.Recipe = TEXT("SoundTrigger");
  Plan.Values.Add(TEXT("Once"), MakeShared<FJsonValueBoolean>(GetJsonBoolField(Payload, TEXT("once"), false)));
  Plan.Values.Add(TEXT("Spatial"), MakeShared<FJsonValueBoolean>(GetJsonBoolField(Payload, TEXT("spatial"), false)));
  return true;
}

bool PlanSequenceTrigger(const TSharedPtr<FJsonObject> &Payload, FBehaviourPlan &Plan, FString &OutError) {
  const FString Path = GetJsonStringField(Payload, TEXT("sequencePath")).TrimStartAndEnd();
  const UObject *Asset = Path.IsEmpty() ? nullptr : McpLoadAsset(Path);
  const ULevelSequence *Sequence = Cast<ULevelSequence>(Asset);
  if (!Sequence) {
    // The code manage_sequence answers for a path that is not a Level Sequence.
    Plan.ErrorCode = TEXT("SEQUENCE_NOT_FOUND");
    OutError = Path.IsEmpty() ? FString(TEXT("sequencePath must name the Level Sequence to play"))
                              : FString::Printf(TEXT("sequencePath %s is not a Level Sequence (%s)"), *Path,
                                                Asset ? *Asset->GetClass()->GetName() : TEXT("no asset there"));
    return false;
  }
  Plan.Recipe = TEXT("SequenceTrigger");
  Plan.Values.Add(TEXT("Sequence"), MakeShared<FJsonValueString>(Sequence->GetPathName()));
  Plan.Values.Add(TEXT("Once"), MakeShared<FJsonValueBoolean>(GetJsonBoolField(Payload, TEXT("once"), true)));
  Plan.Values.Add(TEXT("Cinematic"), MakeShared<FJsonValueBoolean>(GetJsonBoolField(Payload, TEXT("cinematic"), true)));
  return true;
}
} // namespace McpBlueprintHandlers
