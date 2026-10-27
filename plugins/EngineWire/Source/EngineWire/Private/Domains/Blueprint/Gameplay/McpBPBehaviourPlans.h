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

// The AI and route behaviours (McpBPBehaviourPlansAI.cpp): a pawn that chases the player (recipe Gameplay/Chase), one
// that patrols a route and may chase as well (Gameplay/Patrol), which takes over a chase the pawn already has, an actor
// that turns to the player and fires at them (Gameplay/Turret), an enemy the player stomps (Gameplay/Stomp), which takes
// over a damage on touch, and a platform that travels a route (MovingPlatform).
bool PlanChase(const TSharedPtr<FJsonObject> &Payload, const UBlueprint *Blueprint, FBehaviourPlan &Plan, FString &OutError);
bool PlanPatrol(const TSharedPtr<FJsonObject> &Payload, const UBlueprint *Blueprint, FBehaviourPlan &Plan, FString &OutError);
bool PlanMovingPlatform(const TSharedPtr<FJsonObject> &Payload, FBehaviourPlan &Plan, FString &OutError);
bool PlanExplosive(const TSharedPtr<FJsonObject> &Payload, const UBlueprint *Blueprint, FBehaviourPlan &Plan,
                   FString &OutError);
bool PlanStomp(const TSharedPtr<FJsonObject> &Payload, const UBlueprint *Blueprint, FBehaviourPlan &Plan, FString &OutError);
bool PlanTurret(const TSharedPtr<FJsonObject> &Payload, const UBlueprint *Blueprint, FBehaviourPlan &Plan, FString &OutError);

// projectileClass, projectileSpeed and fireRate (default DefaultRate shots a minute) as the values ProjectileType,
// ProjectileClass, ProjectileSpeed and FireRate that the Shoot and Turret recipes take; false with OutError.
bool ReadProjectile(const TSharedPtr<FJsonObject> &Payload, const UBlueprint *Blueprint, double DefaultRate,
                    FBehaviourPlan &Plan, FString &OutError);

// soundPath as the Sound value the recipes take (an object path, or None when it is not given); false with OutError
// when it names no sound asset.
bool ReadSound(const TSharedPtr<FJsonObject> &Payload, FBehaviourPlan &Plan, FString &OutError);

// The class has the members the Health recipe (Resources/Recipes/Gameplay/Health.json) gives it.
bool HasHealthBehaviour(const UClass *Class);
} // namespace McpBlueprintHandlers
