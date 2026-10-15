// add_health_hud and add_counter_hud (blueprint.add_behaviour behaviour=health_hud, counter_hud): a pawn's health bar,
// or one of its numbers (Coins, Score), on the screen of the local player who controls it. The HealthHud and CounterHud
// recipes (Resources/Recipes/Gameplay) create the widget when a local player's controller takes the pawn, keep it
// current (the bar on every OnHealthChanged, the number every 0.1 s) and take it off when the pawn loses that controller
// or is destroyed. With no widgetPath the widget is made next to the Blueprint under UI/: the health bar HUD piece, or
// a text at the top right. A pawn's health and counts were shown by nothing: each meant a hand-wired widget.
#include "Domains/Blueprint/McpBPActionContext.h"
#include "Domains/Blueprint/Gameplay/McpBPBehaviourPlans.h"
#include "Domains/Blueprint/Gameplay/McpBPHudFlow.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Blueprint.h"
#include "UObject/UnrealType.h"

namespace McpBlueprintHandlers {
namespace {
constexpr const TCHAR *PieceBar = TEXT("HealthBar_Bar"); // the bar of the health bar HUD piece (add_health_bar)
} // namespace

bool HandleBlueprintAddHealthHud(const FBlueprintActionContext &Context) {
  if (!ActionMatchesPattern(Context, TEXT("add_health_hud"))) {
    return false;
  }
  FString Path;
  UBlueprint *Blueprint = LoadScreenBlueprint(Context, TEXT("A health bar"), Path);
  if (!Blueprint) {
    return true;
  }
  FHud Hud;
  Hud.Recipe = TEXT("HealthHud");
  Hud.WidgetName = TEXT("WBP_HealthBar");
  Hud.Element = GetJsonStringField(Context.LocalPayload, TEXT("barName"), PieceBar).TrimStartAndEnd();
  Hud.ElementClass = UProgressBar::StaticClass();
  Hud.bElementNamed = Hud.Element != PieceBar;
  Hud.AddHandler = WidgetAuthoringHandlers::HandleWidgetAuthoringHudElements;
  Hud.AddSubAction = TEXT("add_health_bar");
  const FString Refusal = !HasHealthBehaviour(Blueprint->GeneratedClass)
                              ? FString(TEXT("The Blueprint has no health to show: add_behaviour behaviour=health first"))
                          : Hud.Element.IsEmpty() ? FString(TEXT("barName must not be empty")) : FString();
  if (!Refusal.IsEmpty()) {
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, false, Refusal, nullptr,
                                          TEXT("INVALID_ARGUMENT"));
    return true;
  }
  SetHudString(Hud, TEXT("BarName"), Hud.Element);
  SetHudString(Hud, TEXT("SelfClass"), Blueprint->GeneratedClass->GetPathName());
  AuthorHud(Context, Blueprint, Path, Hud,
            TEXT("Health bar written: while a local player controls the pawn, {widget} is on screen with its bar filled to "
                 "Health / MaxHealth, refilled whenever OnHealthChanged fires."));
  return true;
}

bool HandleBlueprintAddCounterHud(const FBlueprintActionContext &Context) {
  if (!ActionMatchesPattern(Context, TEXT("add_counter_hud"))) {
    return false;
  }
  FString Path;
  UBlueprint *Blueprint = LoadScreenBlueprint(Context, TEXT("A counter"), Path);
  if (!Blueprint) {
    return true;
  }
  const TSharedPtr<FJsonObject> &Payload = Context.LocalPayload;
  const FString Variable = GetJsonStringField(Payload, TEXT("variableName")).TrimStartAndEnd();
  const FProperty *Number = Variable.IsEmpty() ? nullptr : FindFProperty<FProperty>(Blueprint->GeneratedClass, *Variable);
  if (!Number || !(Number->IsA<FIntProperty>() || Number->IsA<FInt64Property>() || Number->IsA<FFloatProperty>() ||
                   Number->IsA<FDoubleProperty>())) {
    Context.Bridge.SendAutomationResponse(
        Context.RequestingSocket, Context.RequestId, false,
        FString::Printf(TEXT("variableName must name an Int or Float variable of the Blueprint (Coins, Score); '%s' %s"),
                        *Variable, Number ? TEXT("is not a number") : TEXT("is not one of its variables")),
        nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  const FString Label = Payload->HasField(TEXT("label")) ? GetJsonStringField(Payload, TEXT("label")) : Variable;
  const FString Prefix = Label.IsEmpty() ? FString() : Label + TEXT(": ");
  FHud Hud;
  Hud.Recipe = TEXT("CounterHud");
  Hud.WidgetName = TEXT("WBP_") + Variable + TEXT("Counter");
  Hud.Element = GetJsonStringField(Payload, TEXT("textName"), Variable + TEXT("Counter")).TrimStartAndEnd();
  Hud.ElementClass = UTextBlock::StaticClass();
  Hud.bElementNamed = Payload->HasField(TEXT("textName"));
  Hud.AddHandler = WidgetAuthoringHandlers::HandleWidgetAuthoringTreeBuild;
  Hud.AddSubAction = TEXT("add_widget_tree");
  // The text at the top right, a row lower for each other counter this pawn already shows (CoinsCounterHud...).
  const FString HudVariable = Variable + TEXT("CounterHud");
  int32 Row = 0;
  for (const FBPVariableDescription &Existing : Blueprint->NewVariables) {
    const FString Name = Existing.VarName.ToString();
    Row += Name.EndsWith(TEXT("CounterHud")) && Name != HudVariable ? 1 : 0;
  }
  const TSharedPtr<FJsonObject> Tree = MakeShared<FJsonObject>();
  Tree->SetStringField(TEXT("type"), TEXT("TextBlock"));
  Tree->SetStringField(TEXT("name"), Hud.Element);
  Tree->SetStringField(TEXT("text"), Prefix + TEXT("0"));
  Tree->SetNumberField(TEXT("fontSize"), 28);
  Tree->SetStringField(TEXT("justify"), TEXT("right"));
  const TSharedPtr<FJsonObject> Slot = MakeShared<FJsonObject>();
  const auto Numbers = [](std::initializer_list<double> Items) {
    TArray<TSharedPtr<FJsonValue>> Out;
    for (const double Item : Items) {
      Out.Add(MakeShared<FJsonValueNumber>(Item));
    }
    return Out;
  };
  Slot->SetArrayField(TEXT("anchors"), Numbers({1, 0, 1, 0}));
  Slot->SetArrayField(TEXT("alignment"), Numbers({1, 0}));
  Slot->SetArrayField(TEXT("position"), Numbers({-32, 24.0 + 44.0 * Row}));
  Slot->SetBoolField(TEXT("autoSize"), true);
  Tree->SetObjectField(TEXT("slot"), Slot);
  Hud.AddPayload->SetObjectField(TEXT("tree"), Tree);
  if (Hud.Element.IsEmpty()) {
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, false,
                                          TEXT("textName must not be empty"), nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  SetHudString(Hud, TEXT("Tag"), TEXT("CounterHud_") + Variable);
  SetHudString(Hud, TEXT("HudVariable"), HudVariable);
  SetHudString(Hud, TEXT("TextName"), Hud.Element);
  SetHudString(Hud, TEXT("VariableName"), Variable);
  SetHudString(Hud, TEXT("Prefix"), Prefix);
  SetHudString(Hud, TEXT("FillFunction"), TEXT("Update") + Variable + TEXT("Counter"));
  SetHudString(Hud, TEXT("DropFunction"), TEXT("Remove") + Variable + TEXT("Counter"));
  AuthorHud(Context, Blueprint, Path, Hud,
            TEXT("Counter written: while a local player controls the pawn, {widget} is on screen showing the number, "
                 "refreshed every 0.1 seconds."));
  return true;
}
} // namespace McpBlueprintHandlers
