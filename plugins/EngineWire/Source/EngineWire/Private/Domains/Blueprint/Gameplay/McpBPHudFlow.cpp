#include "Domains/Blueprint/Gameplay/McpBPHudFlow.h"

#include "Domains/Blueprint/McpBPActionContext.h"
#include "Domains/BlueprintGraph/Behaviour/McpBPBehaviour.h"
#include "Core/Requests/McpResponseCaptureRegistry.h"
#include "Domains/WidgetAuthoring/Support/McpWidgetBPLoading.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPCompileHelpers.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Editor.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Pawn.h"
#include "Misc/PackageName.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace McpBlueprintHandlers {
namespace {
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
} // namespace

UBlueprint *LoadScreenBlueprint(const FBlueprintActionContext &Context, const TCHAR *What, FString &OutPath,
                                const UClass *Base) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  const FString Path = ResolveBlueprintRequestedPath();
  FString Normalized, Error;
  UBlueprint *Blueprint = Path.IsEmpty() ? nullptr : LoadBlueprintAsset(Path, Normalized, Error);
  OutPath = Normalized.IsEmpty() ? Path : Normalized;
  if (Blueprint && (Blueprint->Status == BS_Dirty || Blueprint->Status == BS_Unknown)) {
    McpSafeCompileBlueprint(Blueprint); // a variable added since the last compile is not on the class yet
  }
  const UClass *Class = Blueprint ? Blueprint->GeneratedClass.Get() : nullptr;
  if (!Class || !Class->IsChildOf(Base ? Base : APawn::StaticClass())) {
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, false,
        !Blueprint ? (Error.IsEmpty() ? FString(TEXT("blueprintPath must name a Blueprint")) : Error)
        : Base     ? FString::Printf(TEXT("%s needs an Actor Blueprint"), What)
                   : FString::Printf(TEXT("%s on screen needs a Pawn or Character Blueprint (the player controlling it sees it)"), What),
        nullptr, Blueprint ? TEXT("INVALID_ARGUMENT") : TEXT("BLUEPRINT_NOT_FOUND"));
    return nullptr;
  }
  return Blueprint;
}

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
                                Result.bApplied ? FString(Message).Replace(TEXT("{widget}"), *WidgetPackage) : Result.Message,
                                Result.Report, Result.ErrorCode);
}
} // namespace McpBlueprintHandlers
