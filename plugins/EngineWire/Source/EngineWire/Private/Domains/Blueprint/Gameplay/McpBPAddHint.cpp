// add_hint (blueprint.add_behaviour behaviour=hint): a message on the player's screen while the player's pawn is in the
// actor's area (a sign, a tutorial zone). The Hint recipe (Resources/Recipes/Gameplay) puts the widget on the view of a
// locally controlled pawn that starts to overlap the actor and takes it off when that pawn leaves, or hideAfter seconds
// later. With no widgetPath it makes UI/WBP_Hint next to the Blueprint: the message in a dark box at the bottom of the
// view. A message on screen had no gameplay action: the viewport settings were the nearest answer.
#include "Domains/Blueprint/McpBPActionContext.h"
#include "Domains/Blueprint/Gameplay/McpBPHudFlow.h"
#include "Domains/WidgetAuthoring/Templates/McpWidgetSpec.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"

#include "Components/TextBlock.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"

namespace McpBlueprintHandlers {
namespace {
constexpr const TCHAR *HintText = TEXT("Hint_Text"); // the message text of the default widget
} // namespace

bool HandleBlueprintAddHint(const FBlueprintActionContext &Context) {
  if (!ActionMatchesPattern(Context, TEXT("add_hint"))) {
    return false;
  }
  FString Path;
  UBlueprint *Blueprint = LoadScreenBlueprint(Context, TEXT("A hint"), Path, AActor::StaticClass());
  if (!Blueprint) {
    return true;
  }
  const TSharedPtr<FJsonObject> &Payload = Context.LocalPayload;
  const FString Message = GetJsonStringField(Payload, TEXT("message")).TrimStartAndEnd();
  const double HideAfter = GetJsonNumberField(Payload, TEXT("hideAfter"), 0.0);
  FHud Hud;
  Hud.Recipe = TEXT("Hint");
  Hud.WidgetName = TEXT("WBP_Hint");
  Hud.Element = GetJsonStringField(Payload, TEXT("textName"), HintText).TrimStartAndEnd();
  Hud.ElementClass = UTextBlock::StaticClass();
  Hud.bElementNamed = Payload->HasField(TEXT("textName"));
  Hud.AddHandler = WidgetAuthoringHandlers::HandleWidgetAuthoringTreeBuild;
  Hud.AddSubAction = TEXT("add_widget_tree");
  Hud.AddPayload->SetObjectField(TEXT("tree"), WidgetAuthoringHelpers::McpParseWidgetSpec(TEXT(R"({"type":"Overlay","name":"Hint",
      "slot":{"anchors":[0,0,1,1],"offsets":[0,0,0,0]},"children":[
      {"type":"Border","name":"Hint_Box","color":[0,0,0,0.55],"padding":[28,14,28,14],
       "slot":{"hAlign":"center","vAlign":"bottom","padding":[0,0,0,96]},"children":[
       {"type":"TextBlock","name":"Hint_Text","text":"Hint","fontSize":28,"justify":"center"}]}]})")));
  const FString Refusal = Message.IsEmpty()        ? FString(TEXT("message must say what the hint shows"))
                          : HideAfter < 0.0         ? FString(TEXT("hideAfter must be 0 or more"))
                          : Hud.Element.IsEmpty()   ? FString(TEXT("textName must not be empty"))
                                                    : FString();
  if (!Refusal.IsEmpty()) {
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, false, Refusal, nullptr,
                                          TEXT("INVALID_ARGUMENT"));
    return true;
  }
  SetHudString(Hud, TEXT("TextName"), Hud.Element);
  SetHudString(Hud, TEXT("Message"), Message);
  Hud.Values.Add(TEXT("HideAfter"), MakeShared<FJsonValueNumber>(HideAfter));
  const FString Gone = HideAfter > 0.0 ? FString::Printf(TEXT("%.1f seconds later"), HideAfter)
                                       : FString(TEXT("when that pawn leaves"));
  AuthorHud(Context, Blueprint, Path, Hud,
            *(TEXT("Hint written: when a locally controlled pawn starts to overlap the actor, {widget} shows the message "
                   "on that player's view, and it goes ") + Gone + TEXT(". The actor needs a component that overlaps pawns.")));
  return true;
}
} // namespace McpBlueprintHandlers
