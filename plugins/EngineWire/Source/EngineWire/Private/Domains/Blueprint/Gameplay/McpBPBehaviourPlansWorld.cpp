// The plans of the behaviours that set up a place rather than a pawn: a sound that plays when a player walks into the
// actor's area (recipe Gameplay/SoundTrigger). A sound on entering an area had no gameplay action: the nearest answers
// were a sound cue edit and an ambient sound that plays whoever is there. McpBPAddBehaviour.cpp runs the plans.
#include "Domains/Blueprint/Gameplay/McpBPBehaviourPlans.h"

#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"

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
} // namespace McpBlueprintHandlers
