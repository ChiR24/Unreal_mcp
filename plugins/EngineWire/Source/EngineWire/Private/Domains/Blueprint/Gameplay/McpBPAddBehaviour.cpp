// add_health, add_damage_on_touch, add_pickup, add_sprint, add_shoot, add_chase, add_patrol, add_turret, add_stomp,
// add_explosive,
// add_launch_pad, add_teleporter, add_falling_platform, add_moving_platform, add_countdown, add_goal, add_spawner and
// add_dash
// (blueprint.add_behaviour): gameplay logic written into an Actor Blueprint from a recipe (Resources/Recipes/Gameplay and
// Character, chosen by McpBPBehaviourPlans.cpp) through McpBlueprintBehaviour::Author, so the Blueprint holds plain engine
// nodes. Each was a hand-built graph of ten to forty nodes and pins to get right.
#include "Domains/Blueprint/McpBPActionContext.h"
#include "Domains/Blueprint/Gameplay/McpBPBehaviourPlans.h"
#include "Domains/BlueprintGraph/Behaviour/McpBPBehaviour.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"

#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "Engine/Blueprint.h"
#include "Safety/McpSafeOperations.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpBlueprintHandlers {
namespace {
// A chasing or patrolling pawn moves only under an AI controller: placed or spawned, it gets one.
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
} // namespace

bool HandleBlueprintAddBehaviour(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  static const TCHAR *const Actions[] = {TEXT("add_health"), TEXT("add_damage_on_touch"), TEXT("add_pickup"),
                                         TEXT("add_sprint"), TEXT("add_shoot"), TEXT("add_chase"), TEXT("add_patrol"),
                                         TEXT("add_turret"), TEXT("add_stomp"), TEXT("add_explosive"), TEXT("add_launch_pad"),
                                         TEXT("add_teleporter"),
                                         TEXT("add_falling_platform"), TEXT("add_moving_platform"), TEXT("add_countdown"),
                                         TEXT("add_goal"), TEXT("add_spawner"), TEXT("add_dash")};
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
  // A behaviour this one takes over (patrol over chase) is removed first: a recipe holding only its tag removes its nodes
  // and functions. One snapshot for both, so a failure leaves the Blueprint as it was.
  const FString Target = Normalized.IsEmpty() ? Path : Normalized;
  McpBlueprintBehaviour::FSnapshot Before;
  if (Plan.Replaces.Num() > 0) {
    Before = McpBlueprintBehaviour::TakeSnapshot(Blueprint);
  }
  double Removed = 0;
  for (const FString &Tag : Plan.Replaces) {
    const TSharedPtr<FJsonObject> Removal = MakeShared<FJsonObject>();
    Removal->SetStringField(TEXT("tag"), Tag);
    const McpBlueprintBehaviour::FAuthorResult Gone =
        McpBlueprintBehaviour::Author(Bridge, RequestId, RequestingSocket, Target, Removal, &Before);
    if (!Gone.bApplied) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false, Gone.Message, Gone.Report, Gone.ErrorCode);
      return true;
    }
    Removed += GetJsonNumberField(Gone.Report, TEXT("previousRemoved"));
  }
  const McpBlueprintBehaviour::FAuthorResult Result = McpBlueprintBehaviour::Author(
      Bridge, RequestId, RequestingSocket, Target, Recipe, Plan.Replaces.Num() > 0 ? &Before : nullptr);
  // Applied, the per-step rows and node ids of the recipe's batch (several KB for health) tell the caller nothing;
  // a failure keeps them beside failedStep.
  if (Result.bApplied && Result.Report.IsValid()) {
    Result.Report->RemoveField(TEXT("results"));
    Result.Report->RemoveField(TEXT("nodeIds"));
    if (FCString::Strcmp(Matched, TEXT("add_chase")) == 0 || FCString::Strcmp(Matched, TEXT("add_patrol")) == 0) {
      PossessByAI(Blueprint, Result.Report);
    }
    if (Plan.Replaces.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> Replaced;
      for (const FString &Tag : Plan.Replaces) {
        Replaced.Add(MakeShared<FJsonValueString>(Tag));
      }
      Result.Report->SetArrayField(TEXT("replaced"), Replaced);
      Result.Report->SetNumberField(TEXT("previousRemoved"), GetJsonNumberField(Result.Report, TEXT("previousRemoved")) + Removed);
    }
  }
  Bridge.SendAutomationResponse(RequestingSocket, RequestId, Result.bApplied, Result.Message, Result.Report,
                                Result.ErrorCode);
  return true;
}
} // namespace McpBlueprintHandlers
