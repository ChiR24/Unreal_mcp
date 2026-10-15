// add_controls (blueprint.add_behaviour behaviour=controls): a Character the player can steer, in one call. It makes
// IA_Move and IA_Look (Axis2D) next to the Blueprint, writes the Jump, Move and Look recipes (Resources/Recipes/
// Character) against them, and maps WASD and the left stick (Swizzle and Negate shape the keys into one 2D value),
// the mouse and the right stick, and the jump key. A Character made over MCP had no input at all: it could not move.
#include "Domains/Blueprint/McpBPActionContext.h"
#include "Domains/BlueprintGraph/Behaviour/McpBPBehaviour.h"
#include "Core/Requests/McpResponseCaptureRegistry.h"
#include "Domains/Input/McpInputActionRouting.h"
#include "Domains/Input/McpInputAssetResolve.h"
#include "Domains/Input/McpInputKeyResolve.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"

#include "Engine/Blueprint.h"
#include "GameFramework/Character.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Misc/PackageName.h"

namespace McpBlueprintHandlers {
namespace {
// One input-domain request run in process; true when it succeeded.
bool RunInputStep(UEngineWireSubsystem &Bridge, const FString &Id, const TSharedPtr<FJsonObject> &Payload,
                  bool (*Handler)(UEngineWireSubsystem &, const FString &, const TSharedPtr<FJsonObject> &,
                                  TSharedPtr<FMcpBridgeWebSocket>)) {
  FMcpResponseCaptureRegistry::Get().Begin(Id);
  Handler(Bridge, Id, Payload, nullptr);
  return FMcpResponseCaptureRegistry::Get().End(Id).bSuccess;
}

// Maps Key to Action with Modifiers in order, unless that pair is mapped already (a re-run keeps it as it is).
bool MapKey(UEngineWireSubsystem &Bridge, const FString &Id, const FString &Context, const FString &Action,
            const TCHAR *Key, std::initializer_list<const TCHAR *> Modifiers) {
  FString Normalized;
  UInputMappingContext *Imc = McpInputHandlers::LoadInputMappingContextAsset(Context, Normalized);
  UInputAction *Ia = McpInputHandlers::LoadInputActionAsset(Action, Normalized);
  if (Imc && Ia && McpInputHandlers::FindInputMapping(Imc, Ia, McpInputHandlers::InputKeyFromName(Key))) {
    return true;
  }
  TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
  Payload->SetStringField(TEXT("contextPath"), Context);
  Payload->SetStringField(TEXT("actionPath"), Action);
  Payload->SetStringField(TEXT("key"), Key);
  FMcpResponseCaptureRegistry::Get().Begin(Id);
  McpInputHandlers::HandleAddInputMapping(Bridge, Id, TEXT("add_mapping"), Payload, nullptr);
  bool bOk = FMcpResponseCaptureRegistry::Get().End(Id).bSuccess;
  for (const TCHAR *Modifier : Modifiers) {
    Payload->SetStringField(TEXT("modifierType"), Modifier);
    bOk = bOk && RunInputStep(Bridge, Id + TEXT(".") + Modifier, Payload, McpInputHandlers::HandleSetInputModifier);
  }
  return bOk;
}
} // namespace

bool HandleBlueprintAddControls(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (!ActionMatchesPattern(TEXT("add_controls"))) {
    return false;
  }
  const FString Path = ResolveBlueprintRequestedPath();
  FString Normalized, Error;
  UBlueprint *Blueprint = Path.IsEmpty() ? nullptr : LoadBlueprintAsset(Path, Normalized, Error);
  const UClass *Parent = Blueprint ? Blueprint->ParentClass.Get() : nullptr;
  if (!Parent || !Parent->IsChildOf(ACharacter::StaticClass())) {
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                  Blueprint ? TEXT("Controls need a Character Blueprint (it jumps and walks)")
                                            : (Error.IsEmpty() ? TEXT("blueprintPath must name a Blueprint") : *Error),
                                  nullptr, Blueprint ? TEXT("INVALID_ARGUMENT") : TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }
  // IA_Move and IA_Look next to the Blueprint, Axis2D (an existing one is turned Axis2D).
  const FString Folder = FPackageName::GetLongPackagePath(Blueprint->GetPackage()->GetName()) + TEXT("/Input");
  for (const TCHAR *Name : {TEXT("IA_Move"), TEXT("IA_Look")}) {
    TSharedPtr<FJsonObject> ActionPayload = MakeShared<FJsonObject>();
    ActionPayload->SetStringField(TEXT("name"), Name);
    ActionPayload->SetStringField(TEXT("path"), Folder);
    ActionPayload->SetStringField(TEXT("valueType"), TEXT("axis2d"));
    if (!RunInputStep(Bridge, RequestId + TEXT("#") + Name, ActionPayload, McpInputHandlers::HandleCreateInputAction)) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    FString::Printf(TEXT("Could not make %s/%s"), *Folder, Name), nullptr,
                                    TEXT("ASSET_CREATE_FAILED"));
      return true;
    }
  }
  const FString MoveAction = Folder + TEXT("/IA_Move.IA_Move");
  const FString LookAction = Folder + TEXT("/IA_Look.IA_Look");
  const bool bLook = GetJsonBoolField(LocalPayload, TEXT("look"), true);
  // Jump first: its input section settles the mapping context the others share. One snapshot for all three, so any
  // failure leaves the Blueprint as it was.
  const McpBlueprintBehaviour::FSnapshot Before = McpBlueprintBehaviour::TakeSnapshot(Blueprint);
  FString ContextPath = GetJsonStringField(LocalPayload, TEXT("mappingContextPath"));
  TSharedPtr<FJsonObject> Report;
  TArray<FString> Changed; // every part's changed assets, not the last part's alone
  double PreviousRemoved = 0; // a re-run's nodes of all three parts, not the last part's alone
  struct FPart { const TCHAR *Recipe; FString Action; FString Key; };
  for (const FPart &Part : {FPart{TEXT("Jump"), FString(), GetJsonStringField(LocalPayload, TEXT("jumpKey"), TEXT("SpaceBar"))},
                            FPart{TEXT("Move"), MoveAction, FString()}, FPart{TEXT("Look"), LookAction, FString()}}) {
    if (!bLook && Part.Action == LookAction) {
      continue;
    }
    TMap<FString, TSharedPtr<FJsonValue>> Values;
    Values.Add(TEXT("InputAction"), MakeShared<FJsonValueString>(Part.Action));
    Values.Add(TEXT("MappingContext"), MakeShared<FJsonValueString>(ContextPath));
    if (!Part.Key.IsEmpty() || Part.Action.IsEmpty()) {
      Values.Add(TEXT("Key"), MakeShared<FJsonValueString>(Part.Key));
    }
    const TSharedPtr<FJsonObject> Recipe = McpBlueprintBehaviour::LoadRecipe(TEXT("Character"), Part.Recipe, Values, Error);
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
    const TArray<TSharedPtr<FJsonValue>> *PartChanged = nullptr;
    if (Report->TryGetArrayField(TEXT("changedAssets"), PartChanged)) {
      for (const TSharedPtr<FJsonValue> &Asset : *PartChanged) {
        Changed.AddUnique(Asset->AsString());
      }
    }
    PreviousRemoved += GetJsonNumberField(Report, TEXT("previousRemoved"));
    const TSharedPtr<FJsonObject> *Input = nullptr;
    if (ContextPath.IsEmpty() && Report->TryGetObjectField(TEXT("input"), Input)) {
      ContextPath = GetJsonStringField(*Input, TEXT("mappingContextPath"));
    }
  }
  // WASD as one 2D value: W and S on Y (SwizzleAxis), S and A negative (Negate); sticks and the mouse are 2D already.
  const FString Id = RequestId + TEXT("#keys");
  const bool bMapped =
      MapKey(Bridge, Id + TEXT("W"), ContextPath, MoveAction, TEXT("W"), {TEXT("SwizzleAxis")}) &&
      MapKey(Bridge, Id + TEXT("S"), ContextPath, MoveAction, TEXT("S"), {TEXT("SwizzleAxis"), TEXT("Negate")}) &&
      MapKey(Bridge, Id + TEXT("A"), ContextPath, MoveAction, TEXT("A"), {TEXT("Negate")}) &&
      MapKey(Bridge, Id + TEXT("D"), ContextPath, MoveAction, TEXT("D"), {}) &&
      MapKey(Bridge, Id + TEXT("L"), ContextPath, MoveAction, TEXT("Gamepad_Left2D"), {}) &&
      (!bLook || (MapKey(Bridge, Id + TEXT("M"), ContextPath, LookAction, TEXT("Mouse2D"), {}) &&
                  MapKey(Bridge, Id + TEXT("R"), ContextPath, LookAction, TEXT("Gamepad_Right2D"), {})));
  // The last part's own fields (its tag, input, step counts) would describe Look alone.
  for (const TCHAR *Field : {TEXT("results"), TEXT("nodeIds"), TEXT("tag"), TEXT("input"), TEXT("stepCount"), TEXT("succeeded")}) {
    Report->RemoveField(Field);
  }
  Report->SetNumberField(TEXT("previousRemoved"), PreviousRemoved);
  // Asset paths in one form: the context read back from Jump's input section was an object path, a given one not.
  Report->SetStringField(TEXT("moveAction"), FPackageName::ObjectPathToPackageName(MoveAction));
  if (bLook) {
    Report->SetStringField(TEXT("lookAction"), FPackageName::ObjectPathToPackageName(LookAction));
  }
  Report->SetStringField(TEXT("mappingContextPath"), FPackageName::ObjectPathToPackageName(ContextPath));
  Report->SetBoolField(TEXT("keysMapped"), bMapped);
  // The actions made or turned Axis2D and the context the keys went into changed as well.
  for (const FString &Asset : {MoveAction, bLook ? LookAction : FString(), ContextPath}) {
    if (!Asset.IsEmpty()) {
      Changed.AddUnique(FPackageName::ObjectPathToPackageName(Asset));
    }
  }
  TArray<TSharedPtr<FJsonValue>> ChangedValues;
  for (const FString &Asset : Changed) {
    ChangedValues.Add(MakeShared<FJsonValueString>(Asset));
  }
  Report->SetArrayField(TEXT("changedAssets"), ChangedValues);
  Bridge.SendAutomationResponse(RequestingSocket, RequestId, bMapped,
                                bMapped ? FString(TEXT("Controls written: WASD or the left stick walk, the mouse or the right "
                                                       "stick look, and the jump key jumps."))
                                        : FString::Printf(TEXT("The controls were written but a key could not be mapped into "
                                                               "%s; map it with manage_networking configure_input."), *ContextPath),
                                Report, bMapped ? FString() : FString(TEXT("KEY_MAPPING_FAILED")));
  return true;
}
} // namespace McpBlueprintHandlers
