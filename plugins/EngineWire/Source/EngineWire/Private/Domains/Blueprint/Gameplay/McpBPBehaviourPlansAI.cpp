// The plans of the behaviours that act on other actors or travel a route: chase (recipe Gameplay/Chase), patrol
// (Gameplay/Patrol), turret (Gameplay/Turret), stomp (Gameplay/Stomp), explosive (Gameplay/Explosive) and moving
// platform (Gameplay/MovingPlatform).
// Chase and patrol run a timer that moves the pawn through its AI controller, so they would fight over one pawn: patrol
// chases too (chaseRange) and takes over a chase the pawn has, and chase refuses a pawn that patrols.
// McpBPAddBehaviour.cpp runs the plans.
#include "Domains/Blueprint/Gameplay/McpBPBehaviourPlans.h"

#include "Domains/BlueprintGraph/Behaviour/McpBPBehaviour.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Foundation/BridgeHelpers/Reflection/McpClassResolve.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Pawn.h"
#include "NiagaraSystem.h"
#include "UObject/UnrealType.h"

namespace McpBlueprintHandlers {
namespace {
// Whether a behaviour's function (ChaseUpdate, PatrolUpdate) is in the Blueprint, so the behaviour is installed.
bool HasBehaviourFunction(const UBlueprint *Blueprint, const TCHAR *Name) {
  for (const UEdGraph *Graph : Blueprint->FunctionGraphs) {
    if (Graph && Graph->GetFName() == FName(Name)) {
      return true;
    }
  }
  return false;
}

bool IsPawnBlueprint(const UBlueprint *Blueprint) {
  const UClass *Parent = Blueprint->GeneratedClass ? Blueprint->GeneratedClass.Get() : Blueprint->ParentClass.Get();
  return Parent && Parent->IsChildOf(APawn::StaticClass());
}

// points ([[x, y, z], ...]) as the vector export texts an Array:Vector default takes; false with OutError on a bad one.
// Patrol and moving platform share it.
bool ReadRoute(const TSharedPtr<FJsonObject> &Payload, TArray<TSharedPtr<FJsonValue>> &OutPoints, FString &OutError) {
  const TArray<TSharedPtr<FJsonValue>> *Points = nullptr;
  const TArray<TSharedPtr<FJsonValue>> Back = {
      MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(500.0),
                                                                  MakeShared<FJsonValueNumber>(0.0),
                                                                  MakeShared<FJsonValueNumber>(0.0)}),
      MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(0.0),
                                                                  MakeShared<FJsonValueNumber>(0.0),
                                                                  MakeShared<FJsonValueNumber>(0.0)})};
  if (!Payload->TryGetArrayField(TEXT("points"), Points)) {
    Points = &Back; // to 500 units along X and back
  }
  for (const TSharedPtr<FJsonValue> &Point : *Points) {
    static const TCHAR *const Keys[3] = {TEXT("x"), TEXT("y"), TEXT("z")};
    double Xyz[3] = {0.0, 0.0, 0.0};
    if (!ReadJsonTriple(Point, Keys, Xyz)) {
      OutError = TEXT("points must be [x, y, z] offsets from where the actor starts, such as [[500, 0, 0], [0, 0, 0]]");
      return false;
    }
    OutPoints.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), Xyz[0], Xyz[1], Xyz[2])));
  }
  OutError = OutPoints.Num() > 0 ? FString() : FString(TEXT("points must hold at least one point"));
  return OutError.IsEmpty();
}
} // namespace

bool PlanChase(const TSharedPtr<FJsonObject> &Payload, const UBlueprint *Blueprint, FBehaviourPlan &Plan, FString &OutError) {
  const double Range = GetJsonNumberField(Payload, TEXT("chaseRange"), 2000.0);
  const double Stop = GetJsonNumberField(Payload, TEXT("stopDistance"), 100.0);
  OutError = !IsPawnBlueprint(Blueprint) ? FString(TEXT("Chasing needs a Pawn or Character Blueprint"))
             : HasBehaviourFunction(Blueprint, TEXT("PatrolUpdate"))
                 ? FString(TEXT("This pawn patrols, and its patrol chases too: add_behaviour patrol with chaseRange makes it "
                                "chase the player who comes that close and go back to its route after"))
             : Range <= 0.0 || Stop < 0.0 ? FString(TEXT("chaseRange must be above 0 and stopDistance 0 or more")) : FString();
  if (!OutError.IsEmpty()) {
    return false;
  }
  Plan.Recipe = TEXT("Chase");
  Plan.Values.Add(TEXT("ChaseRange"), MakeShared<FJsonValueNumber>(Range));
  Plan.Values.Add(TEXT("StopDistance"), MakeShared<FJsonValueNumber>(Stop));
  Plan.Values.Add(TEXT("UsePathfinding"), MakeShared<FJsonValueBoolean>(GetJsonBoolField(Payload, TEXT("usePathfinding"), true)));
  return true;
}

bool PlanPatrol(const TSharedPtr<FJsonObject> &Payload, const UBlueprint *Blueprint, FBehaviourPlan &Plan, FString &OutError) {
  TArray<TSharedPtr<FJsonValue>> Points;
  const double Wait = GetJsonNumberField(Payload, TEXT("waitTime"), 1.0);
  const double Range = GetJsonNumberField(Payload, TEXT("chaseRange"), 0.0);
  const double Stop = GetJsonNumberField(Payload, TEXT("stopDistance"), 100.0);
  if (!IsPawnBlueprint(Blueprint)) {
    OutError = TEXT("Patrolling needs a Pawn or Character Blueprint");
  } else if (ReadRoute(Payload, Points, OutError) && (Wait < 0.0 || Range < 0.0 || Stop < 0.0)) {
    OutError = TEXT("waitTime, chaseRange and stopDistance must be 0 or more");
  }
  if (!OutError.IsEmpty()) {
    return false;
  }
  Plan.Recipe = TEXT("Patrol");
  Plan.Values.Add(TEXT("Points"), MakeShared<FJsonValueArray>(Points));
  Plan.Values.Add(TEXT("WaitTime"), MakeShared<FJsonValueNumber>(Wait));
  // A setting the caller left out keeps what the Blueprint has (a chase it had keeps its range); new, it is the default.
  Plan.Values.Add(TEXT("ChaseRange"), MakeShared<FJsonValueNumber>(Range));
  Plan.Values.Add(TEXT("StopDistance"), MakeShared<FJsonValueNumber>(Stop));
  Plan.Values.Add(TEXT("UsePathfinding"), MakeShared<FJsonValueBoolean>(GetJsonBoolField(Payload, TEXT("usePathfinding"), true)));
  Plan.Values.Add(TEXT("KeepChaseRange"), MakeShared<FJsonValueBoolean>(!Payload->HasField(TEXT("chaseRange"))));
  Plan.Values.Add(TEXT("KeepStopDistance"), MakeShared<FJsonValueBoolean>(!Payload->HasField(TEXT("stopDistance"))));
  Plan.Values.Add(TEXT("KeepPathfinding"), MakeShared<FJsonValueBoolean>(!Payload->HasField(TEXT("usePathfinding"))));
  if (HasBehaviourFunction(Blueprint, TEXT("ChaseUpdate"))) {
    Plan.Replaces.Add(TEXT("Chase"));
  }
  return true;
}

bool PlanMovingPlatform(const TSharedPtr<FJsonObject> &Payload, FBehaviourPlan &Plan, FString &OutError) {
  TArray<TSharedPtr<FJsonValue>> Points;
  const double Speed = GetJsonNumberField(Payload, TEXT("speed"), 200.0);
  const double Wait = GetJsonNumberField(Payload, TEXT("waitTime"), 1.0);
  if (ReadRoute(Payload, Points, OutError) && (Speed <= 0.0 || Wait < 0.0)) {
    OutError = TEXT("speed must be above 0 and waitTime 0 or more");
  }
  if (!OutError.IsEmpty()) {
    return false;
  }
  Plan.Recipe = TEXT("MovingPlatform");
  Plan.Values.Add(TEXT("Points"), MakeShared<FJsonValueArray>(Points));
  Plan.Values.Add(TEXT("Speed"), MakeShared<FJsonValueNumber>(Speed));
  Plan.Values.Add(TEXT("Wait"), MakeShared<FJsonValueNumber>(Wait));
  return true;
}

bool ReadProjectile(const TSharedPtr<FJsonObject> &Payload, const UBlueprint *Blueprint, double DefaultRate,
                    FBehaviourPlan &Plan, FString &OutError) {
  const FString ClassText = GetJsonStringField(Payload, TEXT("projectileClass")).TrimStartAndEnd();
  UClass *Fired = ResolveClassByName(ClassText);
  const double Speed = GetJsonNumberField(Payload, TEXT("projectileSpeed"), 3000.0);
  const double Rate = GetJsonNumberField(Payload, TEXT("fireRate"), DefaultRate);
  OutError = !Fired || !Fired->IsChildOf(AActor::StaticClass())
                 ? FString::Printf(TEXT("projectileClass %s is not an actor class or Blueprint (make one with manage_combat "
                                        "create_combat_asset kind projectile_blueprint)"), *ClassText)
             : Speed <= 0.0 || Rate <= 0.0 ? FString(TEXT("projectileSpeed and fireRate must be above 0")) : FString();
  if (!OutError.IsEmpty()) {
    return false;
  }
  Plan.Values.Add(TEXT("ProjectileType"), MakeShared<FJsonValueString>(McpBlueprintBehaviour::ClassVariableType(
                                              Blueprint, TEXT("ProjectileClass"), Fired, AActor::StaticClass())));
  Plan.Values.Add(TEXT("ProjectileClass"), MakeShared<FJsonValueString>(Fired->GetPathName()));
  Plan.Values.Add(TEXT("ProjectileSpeed"), MakeShared<FJsonValueNumber>(Speed));
  Plan.Values.Add(TEXT("FireRate"), MakeShared<FJsonValueNumber>(Rate));
  return true;
}

// An actor with health that blows up when it dies (recipe Gameplay/Explosive); effectPath must name a Niagara system.
bool PlanExplosive(const TSharedPtr<FJsonObject> &Payload, const UBlueprint *Blueprint, FBehaviourPlan &Plan,
                   FString &OutError) {
  const double Damage = GetJsonNumberField(Payload, TEXT("explosionDamage"), 50.0);
  const double Radius = GetJsonNumberField(Payload, TEXT("explosionRadius"), 400.0);
  const FString EffectPath = GetJsonStringField(Payload, TEXT("effectPath")).TrimStartAndEnd();
  const UNiagaraSystem *Effect = EffectPath.IsEmpty() ? nullptr : Cast<UNiagaraSystem>(McpLoadAsset(EffectPath));
  const UClass *Self = Blueprint->GeneratedClass;
  OutError = !HasHealthBehaviour(Self)
                 ? FString(TEXT("An explosive blows up when it dies: add_behaviour behaviour=health first"))
             : Damage <= 0.0 || Radius <= 0.0 ? FString(TEXT("explosionDamage and explosionRadius must be above 0"))
             : !EffectPath.IsEmpty() && !Effect ? FString::Printf(TEXT("effectPath %s is not a Niagara system"), *EffectPath)
                                                : FString();
  if (!OutError.IsEmpty() || !ReadSound(Payload, Plan, OutError)) {
    return false;
  }
  Plan.Recipe = TEXT("Explosive");
  Plan.Values.Add(TEXT("Damage"), MakeShared<FJsonValueNumber>(Damage));
  Plan.Values.Add(TEXT("Radius"), MakeShared<FJsonValueNumber>(Radius));
  Plan.Values.Add(TEXT("Effect"), MakeShared<FJsonValueString>(Effect ? Effect->GetPathName() : FString(TEXT("None"))));
  Plan.Values.Add(TEXT("SelfClass"), MakeShared<FJsonValueString>(Self->GetPathName()));
  return true;
}

bool PlanStomp(const TSharedPtr<FJsonObject> &Payload, const UBlueprint *Blueprint, FBehaviourPlan &Plan, FString &OutError) {
  // A damage_on_touch would hurt the player landing on the actor too: stomp takes it over, its damage the side damage.
  // Its default lives on the class defaults; the variable description's DefaultValue is not kept up to date.
  const UClass *Class = Blueprint->GeneratedClass;
  const FNumericProperty *Touch = Class ? FindFProperty<FNumericProperty>(Class, TEXT("TouchDamage")) : nullptr;
  const double TouchDamage =
      Touch ? Touch->GetFloatingPointPropertyValue(Touch->ContainerPtrToValuePtr<void>(Class->GetDefaultObject())) : 0.0;
  const double Bounce = GetJsonNumberField(Payload, TEXT("bounceSpeed"), 600.0);
  const double Side = GetJsonNumberField(Payload, TEXT("sideDamage"), FMath::Max(TouchDamage, 0.0));
  OutError = Bounce > 0.0 && Side >= 0.0 ? FString() : FString(TEXT("bounceSpeed must be above 0 and sideDamage 0 or more"));
  if (!OutError.IsEmpty()) {
    return false;
  }
  Plan.Recipe = TEXT("Stomp");
  Plan.Values.Add(TEXT("Bounce"), MakeShared<FJsonValueNumber>(Bounce));
  Plan.Values.Add(TEXT("SideDamage"), MakeShared<FJsonValueNumber>(Side));
  Plan.Values.Add(TEXT("Destroys"), MakeShared<FJsonValueBoolean>(!HasHealthBehaviour(Blueprint->GeneratedClass)));
  if (McpBlueprintBehaviour::FindOwned(Blueprint, TEXT("DamageOnTouch")).Num() > 0) {
    Plan.Replaces.Add(TEXT("DamageOnTouch"));
  }
  return true;
}

bool PlanTurret(const TSharedPtr<FJsonObject> &Payload, const UBlueprint *Blueprint, FBehaviourPlan &Plan, FString &OutError) {
  const double Range = GetJsonNumberField(Payload, TEXT("range"), 2000.0);
  OutError = Range > 0.0 ? FString() : FString(TEXT("range must be above 0"));
  if (!OutError.IsEmpty() || !ReadProjectile(Payload, Blueprint, 60.0, Plan, OutError)) {
    return false;
  }
  Plan.Recipe = TEXT("Turret");
  Plan.Values.Add(TEXT("Range"), MakeShared<FJsonValueNumber>(Range));
  return true;
}
} // namespace McpBlueprintHandlers
