// add_health_hud and add_counter_hud (blueprint.add_behaviour behaviour=health_hud, counter_hud): a pawn's health bar,
// or one of its numbers (Coins, Score), on the screen of the local player who controls it. The HealthHud and CounterHud
// recipes (Resources/Recipes/Gameplay) create the widget when a local player's controller takes the pawn, keep it
// current (the bar on every OnHealthChanged, the number every 0.1 s) and take it off when the pawn loses that controller
// or is destroyed. With no widgetPath the widget is made next to the Blueprint under UI/: the health bar HUD piece, or
// a text at the top right. A pawn's health and counts were shown by nothing: each meant a hand-wired widget.
#include "Domains/Blueprint/McpBPActionContext.h"
#include "Domains/Blueprint/Gameplay/McpBPBehaviourPlans.h"
#include "Domains/BlueprintGraph/Behaviour/McpBPBehaviour.h"
#include "Core/Requests/McpResponseCaptureRegistry.h"
#include "Domains/WidgetAuthoring/McpWidgetActions.h"
#include "Domains/WidgetAuthoring/Support/McpWidgetBPLoading.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPCompileHelpers.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Pawn.h"
#include "Misc/PackageName.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace McpBlueprintHandlers {
namespace {
constexpr const TCHAR *PieceBar = TEXT("HealthBar_Bar"); // the bar of the health bar HUD piece (add_health_bar)

// One widget authoring sub-action run in process; its refusal message, empty when it succeeded.
FString RunWidgetStep(UEngineWireSubsystem &Bridge, const FString &Id,
                      WidgetAuthoringHandlers::FWidgetAuthoringActionHandler Handler, const TCHAR *SubAction,
                      const TSharedPtr<FJsonObject> &Payload) {
  FMcpResponseCaptureRegistry::Get().Begin(Id);
  Handler(Bridge, Id, SubAction, Payload, nullptr, McpHandlerUtils::CreateResultObject());
  const FMcpCapturedResponse Reply = FMcpResponseCaptureRegistry::Get().End(Id);
  return Reply.bSuccess ? FString() : FString::Printf(TEXT("%s failed: %s"), SubAction, *Reply.Message);
}

// The widget's element Name of ElementClass, read off its generated class: a widget flagged as a variable is a property.
bool HasElement(const UWidgetBlueprint *Widget, const FString &Name, const UClass *ElementClass) {
  const FObjectPropertyBase *Element =
      Widget && Widget->GeneratedClass ? FindFProperty<FObjectPropertyBase>(Widget->GeneratedClass, *Name) : nullptr;
  return Element && Element->PropertyClass && Element->PropertyClass->IsChildOf(ElementClass);
}

// One HUD behaviour: its recipe and values, the widget made when no widgetPath is given, the element the pawn writes,
// and the widget step that adds that element to a widget without it. An element the caller named must be there already.
struct FHud {
  const TCHAR *Recipe = nullptr;
  FString WidgetName;
  FString Element;
  const UClass *ElementClass = nullptr;
  bool bElementNamed = false;
  WidgetAuthoringHandlers::FWidgetAuthoringActionHandler AddHandler = nullptr;
  const TCHAR *AddSubAction = nullptr;
  TSharedPtr<FJsonObject> AddPayload = MakeShared<FJsonObject>();
  TMap<FString, TSharedPtr<FJsonValue>> Values;
};

// The flow both share once the caller's own checks passed: the widget made or loaded and given its element, the
// recipe authored, and the reply sent (Message is the applied reply's text, where {widget} is the widget's path).
void AuthorHud(const FBlueprintActionContext &Context, UBlueprint *Blueprint, const FString &BlueprintPath, FHud &Hud,
               const TCHAR *Message) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  // Author refuses during PIE too, but only after the widget below would have been made.
  if (GEditor && GEditor->IsPlaySessionInProgress()) {
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                  TEXT("Stop Play In Editor first: compiling a Blueprint during PIE reinstances the running game."),
                                  nullptr, TEXT("PIE_ACTIVE"));
    return;
  }
  FString WidgetPath = GetJsonStringField(LocalPayload, TEXT("widgetPath")).TrimStartAndEnd();
  const bool bOwnWidget = WidgetPath.IsEmpty();
  if (bOwnWidget) {
    WidgetPath = FPackageName::GetLongPackagePath(Blueprint->GetPackage()->GetName()) + TEXT("/UI/") + Hud.WidgetName;
  } else if (!WidgetPath.StartsWith(TEXT("/"))) {
    WidgetPath = TEXT("/Game/") + WidgetPath;
  }
  // Asked of the asset registry first: loading a path that holds no asset logs "Failed to find object" warnings.
  UWidgetBlueprint *Widget = McpAssetExists(WidgetPath) ? WidgetAuthoringHelpers::LoadWidgetBlueprint(WidgetPath) : nullptr;
  TArray<TSharedPtr<FJsonValue>> Created;
  bool bWidgetChanged = false;
  FString Error;
  Hud.AddPayload->SetStringField(TEXT("widgetPath"), WidgetPath);
  if (!Widget && bOwnWidget) {
    Error = RunWidgetStep(Bridge, RequestId + TEXT("#widget"), WidgetAuthoringHandlers::HandleWidgetAuthoringCreation,
                          TEXT("create_widget_blueprint"), Hud.AddPayload);
    Widget = Error.IsEmpty() ? WidgetAuthoringHelpers::LoadWidgetBlueprint(WidgetPath) : nullptr;
    Created.Add(MakeShared<FJsonValueString>(WidgetPath));
    bWidgetChanged = true;
  }
  if (Widget && !HasElement(Widget, Hud.Element, Hud.ElementClass) && !Hud.bElementNamed) {
    Error = RunWidgetStep(Bridge, RequestId + TEXT("#element"), Hud.AddHandler, Hud.AddSubAction, Hud.AddPayload);
    bWidgetChanged = true;
  }
  if (!Widget || !HasElement(Widget, Hud.Element, Hud.ElementClass)) {
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, false,
        !Error.IsEmpty() ? Error
        : !Widget        ? FString::Printf(TEXT("No Widget Blueprint at %s"), *WidgetPath)
                         : FString::Printf(TEXT("%s has no %s variable named %s"), *WidgetPath,
                                           *Hud.ElementClass->GetName(), *Hud.Element),
        nullptr, !Widget ? TEXT("NOT_FOUND") : TEXT("WIDGET_NOT_FOUND"));
    return;
  }
  const FString WidgetClass = Widget->GeneratedClass->GetPathName();
  Hud.Values.Add(TEXT("WidgetClass"), MakeShared<FJsonValueString>(WidgetClass));
  Hud.Values.Add(TEXT("WidgetType"), MakeShared<FJsonValueString>(TEXT("Object:") + WidgetClass));
  const TSharedPtr<FJsonObject> Recipe = McpBlueprintBehaviour::LoadRecipe(TEXT("Gameplay"), Hud.Recipe, Hud.Values, Error);
  if (!Recipe) {
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false, Error, nullptr, TEXT("INVALID_RECIPE"));
    return;
  }
  const McpBlueprintBehaviour::FAuthorResult Result =
      McpBlueprintBehaviour::Author(Bridge, RequestId, RequestingSocket, BlueprintPath, Recipe);
  for (const TCHAR *Field : {TEXT("results"), TEXT("nodeIds"), TEXT("tag"), TEXT("stepCount"), TEXT("succeeded")}) {
    Result.Report->RemoveField(Field);
  }
  const FString WidgetPackage = WidgetAuthoringHelpers::WidgetBlueprintPackagePath(Widget);
  Result.Report->SetStringField(TEXT("widgetPath"), WidgetPackage);
  if (Created.Num() > 0) {
    Result.Report->SetArrayField(TEXT("createdAssets"), Created); // kept when the Blueprint is rolled back
  }
  // The widget made or given its element is a changed asset too, even when the Blueprint was put back.
  const TArray<TSharedPtr<FJsonValue>> *Listed = nullptr;
  TArray<TSharedPtr<FJsonValue>> Changed;
  if (Result.Report->TryGetArrayField(TEXT("changedAssets"), Listed)) {
    Changed = *Listed;
  }
  if (bWidgetChanged) {
    Changed.Add(MakeShared<FJsonValueString>(WidgetPackage));
  }
  Result.Report->SetArrayField(TEXT("changedAssets"), Changed);
  Bridge.SendAutomationResponse(RequestingSocket, RequestId, Result.bApplied,
                                Result.bApplied ? FString(Message).Replace(TEXT("{widget}"), *WidgetPackage) : Result.Message, Result.Report,
                                Result.ErrorCode);
}

// The Blueprint at blueprintPath as a pawn class, or the refusal (sent) and null.
UBlueprint *LoadPawnBlueprint(const FBlueprintActionContext &Context, const TCHAR *What, FString &OutPath) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  const FString Path = ResolveBlueprintRequestedPath();
  FString Normalized, Error;
  UBlueprint *Blueprint = Path.IsEmpty() ? nullptr : LoadBlueprintAsset(Path, Normalized, Error);
  OutPath = Normalized.IsEmpty() ? Path : Normalized;
  if (Blueprint && (Blueprint->Status == BS_Dirty || Blueprint->Status == BS_Unknown)) {
    McpSafeCompileBlueprint(Blueprint); // a variable added since the last compile is not on the class yet
  }
  const UClass *Class = Blueprint ? Blueprint->GeneratedClass.Get() : nullptr;
  if (!Class || !Class->IsChildOf(APawn::StaticClass())) {
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, false,
        !Blueprint ? (Error.IsEmpty() ? FString(TEXT("blueprintPath must name a Blueprint")) : Error)
                   : FString::Printf(TEXT("%s on screen needs a Pawn or Character Blueprint (the player controlling it sees it)"), What),
        nullptr, Blueprint ? TEXT("INVALID_ARGUMENT") : TEXT("BLUEPRINT_NOT_FOUND"));
    return nullptr;
  }
  return Blueprint;
}

void SetString(FHud &Hud, const TCHAR *Name, const FString &Value) {
  Hud.Values.Add(Name, MakeShared<FJsonValueString>(Value));
}
} // namespace

bool HandleBlueprintAddHealthHud(const FBlueprintActionContext &Context) {
  if (!ActionMatchesPattern(Context, TEXT("add_health_hud"))) {
    return false;
  }
  FString Path;
  UBlueprint *Blueprint = LoadPawnBlueprint(Context, TEXT("A health bar"), Path);
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
  SetString(Hud, TEXT("BarName"), Hud.Element);
  SetString(Hud, TEXT("SelfClass"), Blueprint->GeneratedClass->GetPathName());
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
  UBlueprint *Blueprint = LoadPawnBlueprint(Context, TEXT("A counter"), Path);
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
  SetString(Hud, TEXT("Tag"), TEXT("CounterHud_") + Variable);
  SetString(Hud, TEXT("HudVariable"), HudVariable);
  SetString(Hud, TEXT("TextName"), Hud.Element);
  SetString(Hud, TEXT("VariableName"), Variable);
  SetString(Hud, TEXT("Prefix"), Prefix);
  SetString(Hud, TEXT("FillFunction"), TEXT("Update") + Variable + TEXT("Counter"));
  SetString(Hud, TEXT("DropFunction"), TEXT("Remove") + Variable + TEXT("Counter"));
  AuthorHud(Context, Blueprint, Path, Hud,
            TEXT("Counter written: while a local player controls the pawn, {widget} is on screen showing the number, "
                 "refreshed every 0.1 seconds."));
  return true;
}
} // namespace McpBlueprintHandlers
