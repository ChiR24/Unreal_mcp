#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

class UBlueprint;
class UClass;

namespace McpBlueprintHandlers {
// One behaviour: its recipe (Resources/Recipes/<Domain>/<Recipe>.json) and the values of its placeholders.
struct FBehaviourPlan {
  FString Domain = TEXT("Gameplay");
  FString Recipe;
  TMap<FString, TSharedPtr<FJsonValue>> Values;
  TArray<FString> Replaces; // tags of behaviours this one takes over: removed first, under the same snapshot
};

// The recipe and its values for an add_<behaviour> action, checked against what the Blueprint is; false with
// OutError otherwise.
bool PlanBehaviour(const FString &Action, const TSharedPtr<FJsonObject> &Payload, const UBlueprint *Blueprint,
                   FBehaviourPlan &Plan, FString &OutError);

// The AI behaviours (McpBPBehaviourPlansAI.cpp): a pawn that chases the player (recipe Gameplay/Chase), and one that
// patrols a route and may chase as well (Gameplay/Patrol), which takes over a chase the pawn already has.
bool PlanChase(const TSharedPtr<FJsonObject> &Payload, const UBlueprint *Blueprint, FBehaviourPlan &Plan, FString &OutError);
bool PlanPatrol(const TSharedPtr<FJsonObject> &Payload, const UBlueprint *Blueprint, FBehaviourPlan &Plan, FString &OutError);

// The class has the members the Health recipe (Resources/Recipes/Gameplay/Health.json) gives it.
bool HasHealthBehaviour(const UClass *Class);
} // namespace McpBlueprintHandlers
