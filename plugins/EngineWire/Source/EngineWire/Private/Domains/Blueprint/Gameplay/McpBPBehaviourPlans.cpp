#include "Domains/Blueprint/Gameplay/McpBPBehaviourPlans.h"

// The add_behaviour plans: which recipe each behaviour writes, its placeholder values, and the checks a call must pass
// before anything is authored (McpBPAddBehaviour.cpp runs the plan).
#include "Domains/BlueprintGraph/Behaviour/McpBPBehaviour.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Foundation/BridgeHelpers/Reflection/McpClassResolve.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"

#include "Engine/Blueprint.h"
#include "GameFramework/Character.h"
#include "Sound/SoundBase.h"
#include "UObject/UnrealType.h"

namespace McpBlueprintHandlers {
namespace {
void SetNumber(FBehaviourPlan &Plan, const TCHAR *Name, double Value) {
  Plan.Values.Add(Name, MakeShared<FJsonValueNumber>(Value));
}

void SetString(FBehaviourPlan &Plan, const TCHAR *Name, const FString &Value) {
  Plan.Values.Add(Name, MakeShared<FJsonValueString>(Value));
}

} // namespace

bool ReadSound(const TSharedPtr<FJsonObject> &Payload, FBehaviourPlan &Plan, FString &OutError) {
  const FString SoundPath = GetJsonStringField(Payload, TEXT("soundPath")).TrimStartAndEnd();
  USoundBase *Sound = SoundPath.IsEmpty() ? nullptr : Cast<USoundBase>(McpLoadAsset(SoundPath));
  if (!SoundPath.IsEmpty() && !Sound) {
    OutError = FString::Printf(TEXT("soundPath %s is not a sound asset"), *SoundPath);
    return false;
  }
  SetString(Plan, TEXT("Sound"), Sound ? Sound->GetPathName() : FString(TEXT("None")));
  return true;
}

namespace {

// A counting pickup: the collector class, its Int or Float variable, and the pin the Cast node gives that class.
bool PlanCountingPickup(const TSharedPtr<FJsonObject> &Payload, FBehaviourPlan &Plan, FString &OutError) {
  const FString ClassText = GetJsonStringField(Payload, TEXT("collectorClass")).TrimStartAndEnd();
  const FString Variable = GetJsonStringField(Payload, TEXT("variableName")).TrimStartAndEnd();
  UClass *Collector = ResolveClassByName(ClassText);
  const FProperty *Property = Collector && !Variable.IsEmpty() ? FindFProperty<FProperty>(Collector, *Variable) : nullptr;
  const bool bInt = Property && Property->IsA<FIntProperty>();
  if (!Collector || !Collector->IsChildOf(AActor::StaticClass())) {
    OutError = FString::Printf(TEXT("collectorClass %s is not an actor class or Blueprint"), *ClassText);
  } else if (!bInt && !(Property && (Property->IsA<FFloatProperty>() || Property->IsA<FDoubleProperty>()))) {
    OutError = FString::Printf(TEXT("%s has no Int or Float variable named '%s' (a variable added since its last "
                                    "compile counts once it compiles)"), *Collector->GetName(), *Variable);
  }
  if (!OutError.IsEmpty()) {
    return false;
  }
  Plan.Recipe = TEXT("PickupCount");
  SetString(Plan, TEXT("CollectorClass"), Collector->GetPathName());
  SetString(Plan, TEXT("CastPin"), TEXT("As") + Collector->GetDisplayNameText().ToString());
  SetString(Plan, TEXT("VariableName"), Property->GetName());
  SetString(Plan, TEXT("AddFunction"), bInt ? TEXT("Add_IntInt") : TEXT("Add_DoubleDouble"));
  SetNumber(Plan, TEXT("Amount"), GetJsonNumberField(Payload, TEXT("amount"), 1.0));
  return true;
}

// A pawn that shoots the projectileClass actor on a key press (recipe Character/Shoot).
bool PlanShoot(const TSharedPtr<FJsonObject> &Payload, const UBlueprint *Blueprint, FBehaviourPlan &Plan, FString &OutError) {
  const UClass *Parent = Blueprint->GeneratedClass ? Blueprint->GeneratedClass.Get() : Blueprint->ParentClass.Get();
  OutError = !Parent || !Parent->IsChildOf(APawn::StaticClass()) ? FString(TEXT("Shooting needs a Pawn or Character Blueprint"))
                                                                 : FString();
  if (!OutError.IsEmpty() || !ReadProjectile(Payload, Blueprint, 300.0, Plan, OutError)) {
    return false;
  }
  Plan.Domain = TEXT("Character");
  Plan.Recipe = TEXT("Shoot");
  SetString(Plan, TEXT("Key"), GetJsonStringField(Payload, TEXT("key"), TEXT("LeftMouseButton")));
  SetString(Plan, TEXT("InputAction"), GetJsonStringField(Payload, TEXT("inputActionPath")));
  SetString(Plan, TEXT("MappingContext"), GetJsonStringField(Payload, TEXT("mappingContextPath")));
  return true;
}

// A pad that launches a Character touching it (recipe Gameplay/LaunchPad).
bool PlanLaunchPad(const TSharedPtr<FJsonObject> &Payload, FBehaviourPlan &Plan, FString &OutError) {
  const FVector Launch = ExtractVectorField(Payload, TEXT("launchVelocity"), FVector(0.0, 0.0, 1200.0));
  if (!ReadSound(Payload, Plan, OutError)) {
    return false;
  }
  Plan.Recipe = TEXT("LaunchPad");
  SetString(Plan, TEXT("Velocity"), FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), Launch.X, Launch.Y, Launch.Z));
  Plan.Values.Add(TEXT("OverrideXY"), MakeShared<FJsonValueBoolean>(GetJsonBoolField(Payload, TEXT("overrideXY"), false)));
  OutError = Launch.IsNearlyZero() ? FString(TEXT("launchVelocity must not be zero")) : FString();
  return OutError.IsEmpty();
}

// A teleporter (recipe Gameplay/Teleporter); its own class and Cast pin tell a target that is a teleporter too.
bool PlanTeleporter(const TSharedPtr<FJsonObject> &Payload, const UBlueprint *Blueprint, FBehaviourPlan &Plan,
                    FString &OutError) {
  const UClass *Self = Blueprint->GeneratedClass.Get();
  const double Cooldown = GetJsonNumberField(Payload, TEXT("cooldown"), 1.0);
  if (!ReadSound(Payload, Plan, OutError)) {
    return false;
  }
  OutError = !Self ? FString(TEXT("The Blueprint has no compiled class yet: compile it, then add the teleporter"))
             : Cooldown < 0.0 ? FString(TEXT("cooldown must be 0 or more")) : FString();
  if (!OutError.IsEmpty()) {
    return false;
  }
  Plan.Recipe = TEXT("Teleporter");
  SetNumber(Plan, TEXT("Cooldown"), Cooldown);
  SetString(Plan, TEXT("SelfClass"), Self->GetPathName());
  SetString(Plan, TEXT("CastPin"), TEXT("As") + Self->GetDisplayNameText().ToString());
  return true;
}

// The player reaching the actor (recipe Gameplay/Goal); a nextLevel given as a path must be an asset that exists.
bool PlanGoal(const TSharedPtr<FJsonObject> &Payload, FBehaviourPlan &Plan, FString &OutError) {
  const FString Level = GetJsonStringField(Payload, TEXT("nextLevel")).TrimStartAndEnd();
  const double Delay = GetJsonNumberField(Payload, TEXT("delay"), 2.0);
  if (!ReadSound(Payload, Plan, OutError)) {
    return false;
  }
  OutError = Delay < 0.0 ? FString(TEXT("delay must be 0 or more"))
             : Level.StartsWith(TEXT("/")) && !McpAssetExists(Level)
                 ? FString::Printf(TEXT("nextLevel %s is not a level in the project"), *Level)
                 : FString();
  Plan.Recipe = TEXT("Goal");
  SetString(Plan, TEXT("NextLevel"), Level.IsEmpty() ? FString(TEXT("None")) : Level);
  SetNumber(Plan, TEXT("Delay"), Delay);
  return OutError.IsEmpty();
}

// An actor that spawns spawnClass on a timer (recipe Gameplay/Spawner); never its own class, which would spawn spawners.
bool PlanSpawner(const TSharedPtr<FJsonObject> &Payload, const UBlueprint *Blueprint, FBehaviourPlan &Plan, FString &OutError) {
  const FString ClassText = GetJsonStringField(Payload, TEXT("spawnClass")).TrimStartAndEnd();
  UClass *Spawned = ResolveClassByName(ClassText);
  const double Interval = GetJsonNumberField(Payload, TEXT("interval"), 3.0);
  const double MaxAlive = GetJsonNumberField(Payload, TEXT("maxAlive"), 5.0);
  const double Radius = GetJsonNumberField(Payload, TEXT("radius"), 0.0);
  OutError = !Spawned || !Spawned->IsChildOf(AActor::StaticClass())
                 ? FString::Printf(TEXT("spawnClass %s is not an actor class or Blueprint"), *ClassText)
             : Spawned == Blueprint->GeneratedClass ? FString(TEXT("A spawner cannot spawn its own class"))
             : Interval <= 0.0 || MaxAlive < 1.0 || Radius < 0.0
                 ? FString(TEXT("interval must be above 0, maxAlive 1 or more and radius 0 or more"))
                 : FString();
  if (!OutError.IsEmpty()) {
    return false;
  }
  Plan.Recipe = TEXT("Spawner");
  SetString(Plan, TEXT("SpawnType"),
            McpBlueprintBehaviour::ClassVariableType(Blueprint, TEXT("SpawnClass"), Spawned, AActor::StaticClass()));
  SetString(Plan, TEXT("SpawnClass"), Spawned->GetPathName());
  SetNumber(Plan, TEXT("Interval"), Interval);
  SetNumber(Plan, TEXT("MaxAlive"), FMath::FloorToDouble(MaxAlive));
  SetNumber(Plan, TEXT("Radius"), Radius);
  return true;
}

// A Character launched along its facing on a key press (recipe Character/Dash), at most once per cooldown.
bool PlanDash(const TSharedPtr<FJsonObject> &Payload, const UBlueprint *Blueprint, FBehaviourPlan &Plan, FString &OutError) {
  const UClass *Parent = Blueprint->GeneratedClass ? Blueprint->GeneratedClass.Get() : Blueprint->ParentClass.Get();
  const double Speed = GetJsonNumberField(Payload, TEXT("dashSpeed"), 1500.0);
  const double Cooldown = GetJsonNumberField(Payload, TEXT("cooldown"), 1.0);
  OutError = !Parent || !Parent->IsChildOf(ACharacter::StaticClass()) ? FString(TEXT("Dashing needs a Character Blueprint (it is launched)"))
             : Speed <= 0.0 || Cooldown < 0.0 ? FString(TEXT("dashSpeed must be above 0 and cooldown 0 or more")) : FString();
  if (!OutError.IsEmpty()) {
    return false;
  }
  Plan.Domain = TEXT("Character");
  Plan.Recipe = TEXT("Dash");
  SetString(Plan, TEXT("Key"), GetJsonStringField(Payload, TEXT("key"), TEXT("Q")));
  SetNumber(Plan, TEXT("DashSpeed"), Speed);
  SetNumber(Plan, TEXT("Cooldown"), Cooldown);
  SetString(Plan, TEXT("InputAction"), GetJsonStringField(Payload, TEXT("inputActionPath")));
  SetString(Plan, TEXT("MappingContext"), GetJsonStringField(Payload, TEXT("mappingContextPath")));
  return true;
}
} // namespace

bool PlanBehaviour(const FString &Action, const TSharedPtr<FJsonObject> &Payload, const UBlueprint *Blueprint,
                   FBehaviourPlan &Plan, FString &OutError) {
  if (Action == TEXT("add_health")) {
    const double MaxHealth = GetJsonNumberField(Payload, TEXT("maxHealth"), 100.0);
    const double Invulnerable = GetJsonNumberField(Payload, TEXT("invulnerableTime"), 0.0);
    Plan.Recipe = TEXT("Health");
    SetNumber(Plan, TEXT("MaxHealth"), MaxHealth);
    SetNumber(Plan, TEXT("Invulnerable"), Invulnerable);
    Plan.Values.Add(TEXT("DestroyOnDeath"), MakeShared<FJsonValueBoolean>(GetJsonBoolField(Payload, TEXT("destroyOnDeath"))));
    OutError = MaxHealth > 0.0 && Invulnerable >= 0.0 ? FString()
                                                      : FString(TEXT("maxHealth must be above 0 and invulnerableTime 0 or more"));
  } else if (Action == TEXT("add_damage_on_touch")) {
    const double Damage = GetJsonNumberField(Payload, TEXT("damage"), 10.0);
    const double Knockback = GetJsonNumberField(Payload, TEXT("knockback"), 0.0);
    Plan.Recipe = TEXT("DamageOnTouch");
    SetNumber(Plan, TEXT("Damage"), Damage);
    SetNumber(Plan, TEXT("Knockback"), Knockback);
    OutError = Damage == 0.0     ? FString(TEXT("damage must not be 0 (a negative amount heals)"))
               : Knockback < 0.0 ? FString(TEXT("knockback must be 0 or more")) : FString();
  } else if (Action == TEXT("add_countdown")) {
    const double Seconds = FMath::FloorToDouble(GetJsonNumberField(Payload, TEXT("seconds"), 60.0));
    Plan.Recipe = TEXT("Countdown");
    SetNumber(Plan, TEXT("Seconds"), Seconds);
    Plan.Values.Add(TEXT("Kill"), MakeShared<FJsonValueBoolean>(GetJsonBoolField(Payload, TEXT("killAtZero"), true)));
    OutError = Seconds >= 1.0 ? FString() : FString(TEXT("seconds must be 1 or more"));
  } else if (Action == TEXT("add_falling_platform")) {
    const double Delay = GetJsonNumberField(Payload, TEXT("fallDelay"), 0.5);
    const double Respawn = GetJsonNumberField(Payload, TEXT("respawnTime"), 3.0);
    Plan.Recipe = TEXT("FallingPlatform");
    SetNumber(Plan, TEXT("Delay"), Delay);
    SetNumber(Plan, TEXT("Respawn"), Respawn);
    OutError = Delay >= 0.0 && Respawn >= 0.0 ? FString() : FString(TEXT("fallDelay and respawnTime must be 0 or more"));
  } else if (Action == TEXT("add_pickup")) {
    const double Heal = GetJsonNumberField(Payload, TEXT("heal"), 0.0);
    if (!ReadSound(Payload, Plan, OutError)) {
      return false;
    }
    if (Payload->HasField(TEXT("collectorClass"))) {
      OutError = Heal > 0.0 ? FString(TEXT("heal and collectorClass make two kinds of pickup: give one")) : FString();
      return OutError.IsEmpty() && PlanCountingPickup(Payload, Plan, OutError);
    }
    Plan.Recipe = TEXT("Pickup");
    SetNumber(Plan, TEXT("Heal"), Heal);
    OutError = Heal >= 0.0 ? FString() : FString(TEXT("heal must be 0 or more"));
  } else if (Action == TEXT("add_shoot")) {
    return PlanShoot(Payload, Blueprint, Plan, OutError);
  } else if (Action == TEXT("add_chase")) {
    return PlanChase(Payload, Blueprint, Plan, OutError);
  } else if (Action == TEXT("add_patrol")) {
    return PlanPatrol(Payload, Blueprint, Plan, OutError);
  } else if (Action == TEXT("add_moving_platform")) {
    return PlanMovingPlatform(Payload, Plan, OutError);
  } else if (Action == TEXT("add_explosive")) {
    return PlanExplosive(Payload, Blueprint, Plan, OutError);
  } else if (Action == TEXT("add_stomp")) {
    return PlanStomp(Payload, Blueprint, Plan, OutError);
  } else if (Action == TEXT("add_turret")) {
    return PlanTurret(Payload, Blueprint, Plan, OutError);
  } else if (Action == TEXT("add_launch_pad")) {
    return PlanLaunchPad(Payload, Plan, OutError);
  } else if (Action == TEXT("add_teleporter")) {
    return PlanTeleporter(Payload, Blueprint, Plan, OutError);
  } else if (Action == TEXT("add_goal")) {
    return PlanGoal(Payload, Plan, OutError);
  } else if (Action == TEXT("add_spawner")) {
    return PlanSpawner(Payload, Blueprint, Plan, OutError);
  } else if (Action == TEXT("add_dash")) {
    return PlanDash(Payload, Blueprint, Plan, OutError);
  } else {
    const UClass *Parent = Blueprint->GeneratedClass ? Blueprint->GeneratedClass.Get() : Blueprint->ParentClass.Get();
    const double SprintSpeed = GetJsonNumberField(Payload, TEXT("sprintSpeed"), 1000.0);
    Plan.Domain = TEXT("Character");
    Plan.Recipe = TEXT("Sprint");
    SetString(Plan, TEXT("Key"), GetJsonStringField(Payload, TEXT("key"), TEXT("LeftShift")));
    SetNumber(Plan, TEXT("SprintSpeed"), SprintSpeed);
    // Empty: the recipe's input section makes IA_Sprint and uses the context the character registers.
    SetString(Plan, TEXT("InputAction"), GetJsonStringField(Payload, TEXT("inputActionPath")));
    SetString(Plan, TEXT("MappingContext"), GetJsonStringField(Payload, TEXT("mappingContextPath")));
    OutError = !Parent || !Parent->IsChildOf(ACharacter::StaticClass()) ? FString(TEXT("Sprint needs a Character Blueprint"))
               : SprintSpeed <= 0.0 ? FString(TEXT("sprintSpeed must be above 0")) : FString();
  }
  return OutError.IsEmpty();
}

bool HasHealthBehaviour(const UClass *Class) {
  for (const TCHAR *Name : {TEXT("Health"), TEXT("MaxHealth"), TEXT("bIsDead"), TEXT("bDestroyOnDeath"),
                            TEXT("OnDied"), TEXT("OnHealthChanged")}) {
    if (!Class || !FindFProperty<FProperty>(Class, Name)) {
      return false;
    }
  }
  return true;
}
} // namespace McpBlueprintHandlers
