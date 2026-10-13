// add_health_hud (blueprint.add_behaviour behaviour=health_hud): a pawn's health on screen. The HealthHud recipe
// (Resources/Recipes/Gameplay) creates a Widget Blueprint for the local player whose controller takes the pawn, fills
// its ProgressBar with Health / MaxHealth on every OnHealthChanged, and takes it off the screen when the pawn loses that
// controller or is destroyed. With no widgetPath it makes UI/WBP_HealthBar next to the Blueprint from the health bar
// HUD piece. The health behaviour broadcast OnHealthChanged, but nothing showed it: a health bar meant hand-wiring.
#include "Domains/Blueprint/McpBPActionContext.h"
#include "Domains/Blueprint/Gameplay/McpBPBehaviourPlans.h"
#include "Domains/BlueprintGraph/Behaviour/McpBPBehaviour.h"
#include "Core/Requests/McpResponseCaptureRegistry.h"
#include "Domains/WidgetAuthoring/McpWidgetActions.h"
#include "Domains/WidgetAuthoring/Support/McpWidgetBPLoading.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Components/ProgressBar.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Pawn.h"
#include "Misc/PackageName.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace McpBlueprintHandlers {
namespace {
constexpr const TCHAR *PieceBar = TEXT("HealthBar_Bar"); // the bar of the health bar HUD piece (add_health_bar)

// One manage_widget_authoring sub-action run in process; its refusal message, empty when it succeeded.
FString RunWidgetStep(UEngineWireSubsystem &Bridge, const FString &Id,
                      WidgetAuthoringHandlers::FWidgetAuthoringActionHandler Handler, const TCHAR *SubAction,
                      const FString &WidgetPath) {
  TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
  Payload->SetStringField(TEXT("widgetPath"), WidgetPath);
  FMcpResponseCaptureRegistry::Get().Begin(Id);
  Handler(Bridge, Id, SubAction, Payload, nullptr, McpHandlerUtils::CreateResultObject());
  const FMcpCapturedResponse Reply = FMcpResponseCaptureRegistry::Get().End(Id);
  return Reply.bSuccess ? FString() : FString::Printf(TEXT("%s on %s failed: %s"), SubAction, *WidgetPath, *Reply.Message);
}

// The widget's ProgressBar called BarName, read off its generated class: a widget flagged as a variable is a property.
bool HasBar(const UWidgetBlueprint *Widget, const FString &BarName) {
  const FObjectPropertyBase *Bar =
      Widget && Widget->GeneratedClass ? FindFProperty<FObjectPropertyBase>(Widget->GeneratedClass, *BarName) : nullptr;
  return Bar && Bar->PropertyClass && Bar->PropertyClass->IsChildOf(UProgressBar::StaticClass());
}
} // namespace

bool HandleBlueprintAddHealthHud(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (!ActionMatchesPattern(TEXT("add_health_hud"))) {
    return false;
  }
  const FString Path = ResolveBlueprintRequestedPath();
  FString Normalized, Error;
  UBlueprint *Blueprint = Path.IsEmpty() ? nullptr : LoadBlueprintAsset(Path, Normalized, Error);
  const UClass *Class = Blueprint ? Blueprint->GeneratedClass.Get() : nullptr;
  const FString BarName = GetJsonStringField(LocalPayload, TEXT("barName"), PieceBar).TrimStartAndEnd();
  const FString Refusal =
      !Blueprint ? (Error.IsEmpty() ? FString(TEXT("blueprintPath must name a Blueprint")) : Error)
      : !Class || !Class->IsChildOf(APawn::StaticClass())
          ? FString(TEXT("A health bar on screen needs a Pawn or Character Blueprint (the player controlling it sees it)"))
      : !HasHealthBehaviour(Class) ? FString(TEXT("The Blueprint has no health to show: add_behaviour behaviour=health first"))
      : BarName.IsEmpty() ? FString(TEXT("barName must not be empty")) : FString();
  if (!Refusal.IsEmpty()) {
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false, Refusal, nullptr,
                                  Blueprint ? TEXT("INVALID_ARGUMENT") : TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }
  // Author refuses during PIE too, but only after the widget below would have been made.
  if (GEditor && GEditor->IsPlaySessionInProgress()) {
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                  TEXT("Stop Play In Editor first: compiling a Blueprint during PIE reinstances the running game."),
                                  nullptr, TEXT("PIE_ACTIVE"));
    return true;
  }
  FString WidgetPath = GetJsonStringField(LocalPayload, TEXT("widgetPath")).TrimStartAndEnd();
  const bool bOwnWidget = WidgetPath.IsEmpty();
  if (bOwnWidget) {
    WidgetPath = FPackageName::GetLongPackagePath(Blueprint->GetPackage()->GetName()) + TEXT("/UI/WBP_HealthBar");
  } else if (!WidgetPath.StartsWith(TEXT("/"))) {
    WidgetPath = TEXT("/Game/") + WidgetPath;
  }
  // Asked of the asset registry first: loading a path that holds no asset logs "Failed to find object" warnings.
  UWidgetBlueprint *Widget = McpAssetExists(WidgetPath) ? WidgetAuthoringHelpers::LoadWidgetBlueprint(WidgetPath) : nullptr;
  TArray<TSharedPtr<FJsonValue>> Created;
  bool bWidgetChanged = false;
  if (!Widget && bOwnWidget) {
    Error = RunWidgetStep(Bridge, RequestId + TEXT("#widget"), WidgetAuthoringHandlers::HandleWidgetAuthoringCreation,
                          TEXT("create_widget_blueprint"), WidgetPath);
    Widget = Error.IsEmpty() ? WidgetAuthoringHelpers::LoadWidgetBlueprint(WidgetPath) : nullptr;
    Created.Add(MakeShared<FJsonValueString>(WidgetPath));
    bWidgetChanged = true;
  }
  // A widget without the bar gets the health bar piece; a bar named by the caller must already be there.
  if (Widget && !HasBar(Widget, BarName) && BarName == PieceBar) {
    Error = RunWidgetStep(Bridge, RequestId + TEXT("#bar"), WidgetAuthoringHandlers::HandleWidgetAuthoringHudElements,
                          TEXT("add_health_bar"), WidgetPath);
    bWidgetChanged = true;
  }
  if (!Widget || !HasBar(Widget, BarName)) {
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, false,
        !Error.IsEmpty() ? Error
        : !Widget        ? FString::Printf(TEXT("No Widget Blueprint at %s"), *WidgetPath)
                         : FString::Printf(TEXT("%s has no ProgressBar variable named %s"), *WidgetPath, *BarName),
        nullptr, !Widget ? TEXT("NOT_FOUND") : TEXT("WIDGET_NOT_FOUND"));
    return true;
  }
  const FString WidgetClass = Widget->GeneratedClass->GetPathName();
  TMap<FString, TSharedPtr<FJsonValue>> Values;
  Values.Add(TEXT("WidgetClass"), MakeShared<FJsonValueString>(WidgetClass));
  Values.Add(TEXT("WidgetType"), MakeShared<FJsonValueString>(TEXT("Object:") + WidgetClass));
  Values.Add(TEXT("BarName"), MakeShared<FJsonValueString>(BarName));
  Values.Add(TEXT("SelfClass"), MakeShared<FJsonValueString>(Class->GetPathName()));
  const TSharedPtr<FJsonObject> Recipe = McpBlueprintBehaviour::LoadRecipe(TEXT("Gameplay"), TEXT("HealthHud"), Values, Error);
  if (!Recipe) {
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false, Error, nullptr, TEXT("INVALID_RECIPE"));
    return true;
  }
  const McpBlueprintBehaviour::FAuthorResult Result =
      McpBlueprintBehaviour::Author(Bridge, RequestId, RequestingSocket, Normalized.IsEmpty() ? Path : Normalized, Recipe);
  for (const TCHAR *Field : {TEXT("results"), TEXT("nodeIds"), TEXT("tag"), TEXT("stepCount"), TEXT("succeeded")}) {
    Result.Report->RemoveField(Field);
  }
  const FString WidgetPackage = WidgetAuthoringHelpers::WidgetBlueprintPackagePath(Widget);
  Result.Report->SetStringField(TEXT("widgetPath"), WidgetPackage);
  if (Created.Num() > 0) {
    Result.Report->SetArrayField(TEXT("createdAssets"), Created); // kept when the Blueprint is rolled back
  }
  // The widget made or given its bar is a changed asset too, even when the Blueprint was put back.
  const TArray<TSharedPtr<FJsonValue>> *Listed = nullptr;
  TArray<TSharedPtr<FJsonValue>> Changed;
  if (Result.Report->TryGetArrayField(TEXT("changedAssets"), Listed)) {
    Changed = *Listed;
  }
  if (bWidgetChanged) {
    Changed.Add(MakeShared<FJsonValueString>(WidgetPackage));
  }
  Result.Report->SetArrayField(TEXT("changedAssets"), Changed);
  const FString Message = !Result.bApplied ? Result.Message
                          : FString::Printf(TEXT("Health bar written: while a local player controls the pawn, %s is on "
                                                 "screen with %s filled to Health / MaxHealth, refilled whenever "
                                                 "OnHealthChanged fires."),
                                            *WidgetPackage, *BarName);
  Bridge.SendAutomationResponse(RequestingSocket, RequestId, Result.bApplied, Message, Result.Report, Result.ErrorCode);
  return true;
}
} // namespace McpBlueprintHandlers
