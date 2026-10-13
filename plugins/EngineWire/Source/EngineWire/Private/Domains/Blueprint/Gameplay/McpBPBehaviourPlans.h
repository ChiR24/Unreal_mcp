#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

class UBlueprint;

namespace McpBlueprintHandlers {
// One behaviour: its recipe (Resources/Recipes/<Domain>/<Recipe>.json) and the values of its placeholders.
struct FBehaviourPlan {
  FString Domain = TEXT("Gameplay");
  FString Recipe;
  TMap<FString, TSharedPtr<FJsonValue>> Values;
};

// The recipe and its values for an add_<behaviour> action, checked against what the Blueprint is; false with
// OutError otherwise.
bool PlanBehaviour(const FString &Action, const TSharedPtr<FJsonObject> &Payload, const UBlueprint *Blueprint,
                   FBehaviourPlan &Plan, FString &OutError);
} // namespace McpBlueprintHandlers
