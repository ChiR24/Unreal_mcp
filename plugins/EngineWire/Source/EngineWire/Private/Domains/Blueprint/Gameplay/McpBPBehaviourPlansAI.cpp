// The AI-moved behaviours' plans: chase (recipe Gameplay/Chase) and patrol (Gameplay/Patrol). Both run a timer that
// moves the pawn through its AI controller, so they would fight over one pawn: patrol chases too (chaseRange) and
// takes over a chase the pawn has, and chase refuses a pawn that patrols. McpBPAddBehaviour.cpp runs the plans.
#include "Domains/Blueprint/Gameplay/McpBPBehaviourPlans.h"

#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Pawn.h"

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
      OutError = TEXT("points must be [x, y, z] offsets from where the pawn starts, such as [[500, 0, 0], [0, 0, 0]]");
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
} // namespace McpBlueprintHandlers
