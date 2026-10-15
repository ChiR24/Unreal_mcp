// add_respawn (blueprint.add_behaviour behaviour=respawn): a pawn that comes back. The Respawn recipe (Resources/Recipes/
// Gameplay) keeps a respawn point, its start and then where it touched an actor tagged checkpointTag, and teleports it
// there when it falls fallHeight below that point; on a pawn with the Health behaviour, RespawnOnDeath also brings it
// back at full health when it dies. A pawn that fell off a level or died stayed gone: nothing put it back.
#include "Domains/Blueprint/McpBPActionContext.h"
#include "Domains/Blueprint/Gameplay/McpBPBehaviourPlans.h"
#include "Domains/BlueprintGraph/Behaviour/McpBPBehaviour.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"

#include "Engine/Blueprint.h"
#include "GameFramework/Pawn.h"
#include "UObject/UnrealType.h"

namespace McpBlueprintHandlers {
bool HandleBlueprintAddRespawn(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (!ActionMatchesPattern(TEXT("add_respawn"))) {
    return false;
  }
  const FString Path = ResolveBlueprintRequestedPath();
  FString Normalized, Error;
  UBlueprint *Blueprint = Path.IsEmpty() ? nullptr : LoadBlueprintAsset(Path, Normalized, Error);
  const UClass *Class = Blueprint ? Blueprint->GeneratedClass.Get() : nullptr;
  const double FallHeight = GetJsonNumberField(LocalPayload, TEXT("fallHeight"), 2000.0);
  const FString Tag = GetJsonStringField(LocalPayload, TEXT("checkpointTag"), TEXT("Checkpoint")).TrimStartAndEnd();
  const FString Refusal = !Blueprint ? (Error.IsEmpty() ? FString(TEXT("blueprintPath must name a Blueprint")) : Error)
                          : !Class || !Class->IsChildOf(APawn::StaticClass())
                              ? FString(TEXT("Respawning needs a Pawn or Character Blueprint (it teleports a pawn back)"))
                          : FallHeight <= 0.0 ? FString(TEXT("fallHeight must be above 0"))
                          : Tag.IsEmpty() ? FString(TEXT("checkpointTag must not be empty")) : FString();
  if (!Refusal.IsEmpty()) {
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false, Refusal, nullptr,
                                  Blueprint ? TEXT("INVALID_ARGUMENT") : TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }
  // With the Health behaviour, death respawns it too. One snapshot for both, so a failure leaves the Blueprint as it was.
  // A pawn with a game over restarts the level when it dies, so only falling respawns it.
  const bool bGameOver = McpBlueprintBehaviour::FindOwned(Blueprint, TEXT("GameOver")).Num() > 0;
  const bool bOnDeath = HasHealthBehaviour(Class) && !bGameOver;
  TMap<FString, TSharedPtr<FJsonValue>> Values;
  Values.Add(TEXT("FallHeight"), MakeShared<FJsonValueNumber>(FallHeight));
  Values.Add(TEXT("CheckpointTag"), MakeShared<FJsonValueString>(Tag));
  Values.Add(TEXT("SelfClass"), MakeShared<FJsonValueString>(Class->GetPathName()));
  const McpBlueprintBehaviour::FSnapshot Before = McpBlueprintBehaviour::TakeSnapshot(Blueprint);
  TSharedPtr<FJsonObject> Report;
  double PreviousRemoved = 0;
  // The members both parts made, not the last part's alone (the death part adds none, and read as if nothing was added).
  TMap<FString, TArray<TSharedPtr<FJsonValue>>> Members;
  for (const TCHAR *Name : {TEXT("Respawn"), TEXT("RespawnOnDeath")}) {
    if (!bOnDeath && FCString::Strcmp(Name, TEXT("RespawnOnDeath")) == 0) {
      continue;
    }
    const TSharedPtr<FJsonObject> Recipe = McpBlueprintBehaviour::LoadRecipe(TEXT("Gameplay"), Name, Values, Error);
    if (!Recipe) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false, Error, nullptr, TEXT("INVALID_RECIPE"));
      return true;
    }
    const McpBlueprintBehaviour::FAuthorResult Result = McpBlueprintBehaviour::Author(
        Bridge, RequestId, RequestingSocket, Normalized.IsEmpty() ? Path : Normalized, Recipe, &Before);
    if (!Result.bApplied) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false, Result.Message, Result.Report, Result.ErrorCode);
      return true;
    }
    Report = Result.Report;
    PreviousRemoved += GetJsonNumberField(Report, TEXT("previousRemoved"));
    for (const TCHAR *Field : {TEXT("variables"), TEXT("functions"), TEXT("dispatchers")}) {
      const TArray<TSharedPtr<FJsonValue>> *Made = nullptr;
      if (Report->TryGetArrayField(Field, Made)) {
        Members.FindOrAdd(Field).Append(*Made);
      }
    }
  }
  for (const TPair<FString, TArray<TSharedPtr<FJsonValue>>> &Made : Members) {
    Report->SetArrayField(Made.Key, Made.Value);
  }
  for (const TCHAR *Field : {TEXT("results"), TEXT("nodeIds"), TEXT("tag"), TEXT("stepCount"), TEXT("succeeded")}) {
    Report->RemoveField(Field);
  }
  Report->SetNumberField(TEXT("previousRemoved"), PreviousRemoved);
  Report->SetBoolField(TEXT("respawnOnDeath"), bOnDeath);
  const TCHAR *Death = bOnDeath    ? TEXT(" or dies (back at full health).")
                       : bGameOver ? TEXT(". When it dies, its game over restarts the level instead.")
                                   : TEXT(". Add the health behaviour, then this again, to respawn it when it dies too.");
  const FString Message = FString::Printf(
      TEXT("Respawn written: the pawn comes back to its start, or to where it last touched an actor tagged %s, when it "
           "falls %.0f units below that point%s"),
      *Tag, FallHeight, Death);
  Bridge.SendAutomationResponse(RequestingSocket, RequestId, true, Message, Report, FString());
  return true;
}
} // namespace McpBlueprintHandlers
