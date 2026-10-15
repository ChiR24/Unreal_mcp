// add_game_over (blueprint.add_behaviour behaviour=game_over): a game over for the player. The GameOver recipe
// (Resources/Recipes/Gameplay) binds the Health behaviour's OnDied: when the local player's pawn dies, its input stops,
// a widget shows the message, and the level opens again after restartDelay seconds. With no widgetPath it makes
// UI/WBP_GameOver next to the Blueprint: a dark shade over the whole view with the message in the middle. A player who
// died saw nothing happen: the pawn stood there or vanished, and the level went on.
#include "Domains/Blueprint/McpBPActionContext.h"
#include "Domains/Blueprint/Gameplay/McpBPBehaviourPlans.h"
#include "Domains/Blueprint/Gameplay/McpBPHudFlow.h"
#include "Domains/BlueprintGraph/Behaviour/McpBPBehaviour.h"
#include "Domains/WidgetAuthoring/Templates/McpWidgetSpec.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"

#include "Components/TextBlock.h"
#include "Engine/Blueprint.h"

namespace McpBlueprintHandlers {
namespace {
constexpr const TCHAR *GameOverText = TEXT("GameOver_Text"); // the message text of the default screen
} // namespace

bool HandleBlueprintAddGameOver(const FBlueprintActionContext &Context) {
  if (!ActionMatchesPattern(Context, TEXT("add_game_over"))) {
    return false;
  }
  FString Path;
  UBlueprint *Blueprint = LoadScreenBlueprint(Context, TEXT("A game over"), Path);
  if (!Blueprint) {
    return true;
  }
  const TSharedPtr<FJsonObject> &Payload = Context.LocalPayload;
  const double Delay = GetJsonNumberField(Payload, TEXT("restartDelay"), 3.0);
  FHud Hud;
  Hud.Recipe = TEXT("GameOver");
  Hud.WidgetName = TEXT("WBP_GameOver");
  Hud.Element = GetJsonStringField(Payload, TEXT("textName"), GameOverText).TrimStartAndEnd();
  Hud.ElementClass = UTextBlock::StaticClass();
  Hud.bElementNamed = Payload->HasField(TEXT("textName"));
  Hud.AddHandler = WidgetAuthoringHandlers::HandleWidgetAuthoringTreeBuild;
  Hud.AddSubAction = TEXT("add_widget_tree");
  Hud.AddPayload->SetObjectField(TEXT("tree"), WidgetAuthoringHelpers::McpParseWidgetSpec(TEXT(R"({"type":"Overlay","name":"GameOver",
      "slot":{"anchors":[0,0,1,1],"offsets":[0,0,0,0]},"children":[
      {"type":"Border","name":"GameOver_Shade","color":[0,0,0,0.6],"slot":{"hAlign":"fill","vAlign":"fill"}},
      {"type":"TextBlock","name":"GameOver_Text","text":"GAME OVER","fontSize":72,"justify":"center",
       "slot":{"hAlign":"center","vAlign":"center"}}]})")));
  const FString Refusal =
      !HasHealthBehaviour(Blueprint->GeneratedClass)
          ? FString(TEXT("The pawn has no health, so it never dies: add_behaviour behaviour=health first"))
      : McpBlueprintBehaviour::FindOwned(Blueprint, TEXT("RespawnOnDeath")).Num() > 0
          ? FString(TEXT("The pawn respawns when it dies (the respawn behaviour), so a game over would never show"))
      : Delay < 0.0          ? FString(TEXT("restartDelay must be 0 or more"))
      : Hud.Element.IsEmpty() ? FString(TEXT("textName must not be empty"))
                             : FString();
  if (!Refusal.IsEmpty()) {
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, false, Refusal, nullptr,
                                          TEXT("INVALID_ARGUMENT"));
    return true;
  }
  SetHudString(Hud, TEXT("TextName"), Hud.Element);
  SetHudString(Hud, TEXT("Message"), GetJsonStringField(Payload, TEXT("message"), TEXT("GAME OVER")));
  Hud.Values.Add(TEXT("RestartDelay"), MakeShared<FJsonValueNumber>(Delay));
  SetHudString(Hud, TEXT("SelfClass"), Blueprint->GeneratedClass->GetPathName());
  const FString Restart = Delay > 0.0 ? FString::Printf(TEXT("the level opens again %.1f seconds later"), Delay)
                                      : FString(TEXT("the level does not restart (restartDelay 0)"));
  AuthorHud(Context, Blueprint, Path, Hud,
            *(TEXT("Game over written: when the pawn dies while a local player controls it, that player's input stops, "
                   "{widget} shows the message, and ") + Restart + TEXT(". The pawn now stays when it dies.")));
  return true;
}
} // namespace McpBlueprintHandlers
