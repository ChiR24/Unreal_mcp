// add_health, add_damage_on_touch, add_pickup, add_sprint, add_shoot and add_chase (blueprint.add_behaviour): gameplay logic written into an
// Actor Blueprint from a recipe (Resources/Recipes/Gameplay and Character) through McpBlueprintBehaviour::Author, so
// the Blueprint holds plain engine nodes. Each was a hand-built graph of ten to forty nodes and pins to get right.
#include "Domains/Blueprint/McpBPActionContext.h"
#include "Domains/BlueprintGraph/Behaviour/McpBPBehaviour.h"
#include "Foundation/BridgeHelpers/Reflection/McpClassResolve.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"

#include "AIController.h"
#include "Engine/Blueprint.h"
#include "Safety/McpSafeOperations.h"
#include "GameFramework/Character.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Sound/SoundBase.h"
#include "UObject/UnrealType.h"

namespace McpBlueprintHandlers {
namespace {
struct FBehaviourPlan {
  FString Domain = TEXT("Gameplay");
  FString Recipe;
  TMap<FString, TSharedPtr<FJsonValue>> Values;
};

void SetNumber(FBehaviourPlan &Plan, const TCHAR *Name, double Value) {
  Plan.Values.Add(Name, MakeShared<FJsonValueNumber>(Value));
}

void SetString(FBehaviourPlan &Plan, const TCHAR *Name, const FString &Value) {
  Plan.Values.Add(Name, MakeShared<FJsonValueString>(Value));
}

// soundPath as the object path a variable default takes, or None when it is not given.
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
  const FString ClassText = GetJsonStringField(Payload, TEXT("projectileClass")).TrimStartAndEnd();
  UClass *Fired = ResolveClassByName(ClassText);
  const double Speed = GetJsonNumberField(Payload, TEXT("projectileSpeed"), 3000.0);
  const double Rate = GetJsonNumberField(Payload, TEXT("fireRate"), 300.0);
  OutError = !Parent || !Parent->IsChildOf(APawn::StaticClass()) ? FString(TEXT("Shooting needs a Pawn or Character Blueprint"))
             : !Fired || !Fired->IsChildOf(AActor::StaticClass())
                 ? FString::Printf(TEXT("projectileClass %s is not an actor class or Blueprint (make one with manage_combat "
                                        "create_combat_asset kind projectile_blueprint)"), *ClassText)
             : Speed <= 0.0 || Rate <= 0.0 ? FString(TEXT("projectileSpeed and fireRate must be above 0")) : FString();
  if (!OutError.IsEmpty()) {
    return false;
  }
  Plan.Domain = TEXT("Character");
  Plan.Recipe = TEXT("Shoot");
  SetString(Plan, TEXT("Key"), GetJsonStringField(Payload, TEXT("key"), TEXT("LeftMouseButton")));
  SetString(Plan, TEXT("ProjectileType"), TEXT("Class:") + Fired->GetPathName());
  SetString(Plan, TEXT("ProjectileClass"), Fired->GetPathName());
  SetNumber(Plan, TEXT("ProjectileSpeed"), Speed);
  SetNumber(Plan, TEXT("FireRate"), Rate);
  SetString(Plan, TEXT("InputAction"), GetJsonStringField(Payload, TEXT("inputActionPath")));
  SetString(Plan, TEXT("MappingContext"), GetJsonStringField(Payload, TEXT("mappingContextPath")));
  return true;
}

// A pawn its AI controller moves toward the player (recipe Gameplay/Chase).
bool PlanChase(const TSharedPtr<FJsonObject> &Payload, const UBlueprint *Blueprint, FBehaviourPlan &Plan, FString &OutError) {
  const UClass *Parent = Blueprint->GeneratedClass ? Blueprint->GeneratedClass.Get() : Blueprint->ParentClass.Get();
  const double Range = GetJsonNumberField(Payload, TEXT("chaseRange"), 2000.0);
  const double Stop = GetJsonNumberField(Payload, TEXT("stopDistance"), 100.0);
  OutError = !Parent || !Parent->IsChildOf(APawn::StaticClass()) ? FString(TEXT("Chasing needs a Pawn or Character Blueprint"))
             : Range <= 0.0 || Stop < 0.0 ? FString(TEXT("chaseRange must be above 0 and stopDistance 0 or more")) : FString();
  if (!OutError.IsEmpty()) {
    return false;
  }
  Plan.Recipe = TEXT("Chase");
  SetNumber(Plan, TEXT("ChaseRange"), Range);
  SetNumber(Plan, TEXT("StopDistance"), Stop);
  Plan.Values.Add(TEXT("UsePathfinding"), MakeShared<FJsonValueBoolean>(GetJsonBoolField(Payload, TEXT("usePathfinding"), true)));
  return true;
}

// A chasing pawn moves only under an AI controller: placed or spawned, it gets one.
void PossessByAI(UBlueprint *Blueprint, const TSharedPtr<FJsonObject> &Report) {
  APawn *Pawn = Blueprint->GeneratedClass ? Cast<APawn>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
  if (!Pawn) {
    return;
  }
  if (!Pawn->AIControllerClass || !Pawn->AIControllerClass->IsChildOf(AAIController::StaticClass())) {
    Pawn->AIControllerClass = AAIController::StaticClass();
  }
  if (Pawn->AutoPossessAI == EAutoPossessAI::Disabled || Pawn->AutoPossessAI == EAutoPossessAI::PlacedInWorld) {
    Pawn->AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
  }
  Pawn->MarkPackageDirty();
  Report->SetBoolField(TEXT("saved"), McpSafeAssetSave(Blueprint));
  Report->SetStringField(TEXT("aiControllerClass"), Pawn->AIControllerClass->GetPathName());
  Report->SetStringField(TEXT("autoPossessAI"),
                         StaticEnum<EAutoPossessAI>()->GetNameStringByValue(static_cast<int64>(Pawn->AutoPossessAI)));
}

// The recipe and its values for the action, checked against what the Blueprint is; false with OutError otherwise.
bool PlanBehaviour(const FString &Action, const TSharedPtr<FJsonObject> &Payload, const UBlueprint *Blueprint,
                   FBehaviourPlan &Plan, FString &OutError) {
  if (Action == TEXT("add_health")) {
    const double MaxHealth = GetJsonNumberField(Payload, TEXT("maxHealth"), 100.0);
    Plan.Recipe = TEXT("Health");
    SetNumber(Plan, TEXT("MaxHealth"), MaxHealth);
    Plan.Values.Add(TEXT("DestroyOnDeath"), MakeShared<FJsonValueBoolean>(GetJsonBoolField(Payload, TEXT("destroyOnDeath"))));
    OutError = MaxHealth > 0.0 ? FString() : FString(TEXT("maxHealth must be above 0"));
  } else if (Action == TEXT("add_damage_on_touch")) {
    const double Damage = GetJsonNumberField(Payload, TEXT("damage"), 10.0);
    Plan.Recipe = TEXT("DamageOnTouch");
    SetNumber(Plan, TEXT("Damage"), Damage);
    OutError = Damage != 0.0 ? FString() : FString(TEXT("damage must not be 0 (a negative amount heals)"));
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
} // namespace

bool HandleBlueprintAddBehaviour(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  static const TCHAR *const Actions[] = {TEXT("add_health"), TEXT("add_damage_on_touch"), TEXT("add_pickup"),
                                         TEXT("add_sprint"), TEXT("add_shoot"), TEXT("add_chase")};
  const TCHAR *Matched = nullptr;
  for (const TCHAR *Each : Actions) {
    Matched = Matched ? Matched : (ActionMatchesPattern(Each) ? Each : nullptr);
  }
  if (!Matched) {
    return false;
  }
  const FString Path = ResolveBlueprintRequestedPath();
  FString Normalized, Error;
  UBlueprint *Blueprint = Path.IsEmpty() ? nullptr : LoadBlueprintAsset(Path, Normalized, Error);
  if (!Blueprint || !FBlueprintEditorUtils::IsActorBased(Blueprint)) {
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                  Blueprint ? TEXT("A behaviour goes on an Actor Blueprint (an actor, pawn or character)")
                                            : (Error.IsEmpty() ? TEXT("blueprintPath must name a Blueprint") : *Error),
                                  nullptr, Blueprint ? TEXT("NOT_AN_ACTOR_BLUEPRINT") : TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }
  FBehaviourPlan Plan;
  if (!PlanBehaviour(Matched, LocalPayload, Blueprint, Plan, Error)) {
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false, Error, nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  const TSharedPtr<FJsonObject> Recipe = McpBlueprintBehaviour::LoadRecipe(Plan.Domain, Plan.Recipe, Plan.Values, Error);
  if (!Recipe) {
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false, Error, nullptr, TEXT("INVALID_RECIPE"));
    return true;
  }
  const McpBlueprintBehaviour::FAuthorResult Result =
      McpBlueprintBehaviour::Author(Bridge, RequestId, RequestingSocket, Normalized.IsEmpty() ? Path : Normalized, Recipe);
  // Applied, the per-step rows and node ids of the recipe's batch (several KB for health) tell the caller nothing;
  // a failure keeps them beside failedStep.
  if (Result.bApplied && Result.Report.IsValid()) {
    Result.Report->RemoveField(TEXT("results"));
    Result.Report->RemoveField(TEXT("nodeIds"));
    if (FCString::Strcmp(Matched, TEXT("add_chase")) == 0) {
      PossessByAI(Blueprint, Result.Report);
    }
  }
  Bridge.SendAutomationResponse(RequestingSocket, RequestId, Result.bApplied, Result.Message, Result.Report,
                                Result.ErrorCode);
  return true;
}
} // namespace McpBlueprintHandlers
