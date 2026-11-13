#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/SCS/McpSCSHandlers.h"
#include "Domains/SCS/McpSCSSupport.h"

#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Components/SceneComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"

using namespace McpSCSHandlers;

namespace {
// Actor components (no transform) are SCS root nodes too, and so is a scene component hung on an
// inherited parent: only a scene node with no parent is the actor's root.
bool IsSceneRootNode(const USCS_Node *Node) {
  return Node && Node->ComponentTemplate && Node->ComponentTemplate->IsA<USceneComponent>() &&
         Node->ParentComponentOrVariableName == NAME_None;
}
}

TSharedPtr<FJsonObject>
FSCSHandlers::RemoveSCSComponent(const FString &BlueprintPath,
                                 const FString &ComponentName) {
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

  UBlueprint *Blueprint = LoadScsBlueprint(BlueprintPath, Result);
  if (!Blueprint) {
    return Result;
  }

  USimpleConstructionScript *SCS = Blueprint->SimpleConstructionScript;

  USCS_Node *NodeToRemove = FindScsNodeByName(SCS, ComponentName);

  if (!NodeToRemove) {
    return SCSFail(Result, FString::Printf(TEXT("Component not found: %s"), *ComponentName), TEXT("SCS_COMPONENT_NOT_FOUND"));
  }

  // The default root with no child to take its place comes straight back on compile: removing it
  // answered success and changed nothing.
  const bool bDefaultRoot = NodeToRemove == SCS->GetDefaultSceneRootNode();
  const bool bWasRoot = IsSceneRootNode(NodeToRemove) && SCS->GetRootNodes().Contains(NodeToRemove);
  if (bDefaultRoot && NodeToRemove->GetChildNodes().Num() == 0) {
    return SCSFail(Result, TEXT("DefaultSceneRoot is the only scene root and the engine re-creates it; add a scene component "
                                "first, then remove DefaultSceneRoot and that component takes its place."),
                   TEXT("SCS_ROOT_REQUIRED"));
  }
  TArray<TSharedPtr<FJsonValue>> Promoted;
  for (const USCS_Node *Child : NodeToRemove->GetChildNodes()) {
    Promoted.Add(MakeShared<FJsonValueString>(Child->GetVariableName().ToString()));
  }
  // As the editor deletes: the children move up (the first scene child becomes the root), never
  // vanish with their parent; a plain RemoveNode put the default root back with the mesh still under it.
  SCS->RemoveNodeAndPromoteChildren(NodeToRemove);

  bool bCompiled = false;
  bool bSaved = false;
  FinalizeBlueprintSCSChange(Blueprint, bCompiled, bSaved);
  if (FindScsNodeByName(SCS, ComponentName)) {
    return SCSFail(Result, FString::Printf(TEXT("Component '%s' is still in the Blueprint after the compile"), *ComponentName),
                   TEXT("SCS_REMOVE_REVERTED"));
  }

  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(
      TEXT("message"),
      FString::Printf(TEXT("Component '%s' removed from SCS"), *ComponentName));
  if (Promoted.Num() > 0) {
    Result->SetArrayField(TEXT("promotedChildren"), Promoted);
  }
  const USCS_Node *const *NewRoot = SCS->GetRootNodes().FindByPredicate(IsSceneRootNode);
  if (bWasRoot && NewRoot) {
    Result->SetStringField(TEXT("newRoot"), (*NewRoot)->GetVariableName().ToString());
  }
  Result->SetBoolField(TEXT("compiled"), bCompiled);
  Result->SetBoolField(TEXT("saved"), bSaved);
  McpHandlerUtils::AddVerification(Result, Blueprint);

  return Result;
}

// Several components, one compile and one save: removing seven compiled and saved
// the Blueprint seven times.
TSharedPtr<FJsonObject>
FSCSHandlers::RemoveSCSComponents(const FString &BlueprintPath,
                                  const TArray<FString> &ComponentNames) {
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  UBlueprint *Blueprint = LoadScsBlueprint(BlueprintPath, Result);
  if (!Blueprint) {
    return Result;
  }
  USimpleConstructionScript *SCS = Blueprint->SimpleConstructionScript;
  TArray<TSharedPtr<FJsonObject>> Rows;
  TArray<FString> Missing;
  int32 Removed = 0;
  for (const FString &Name : ComponentNames) {
    USCS_Node *Node = FindScsNodeByName(SCS, Name);
    // The single removal's root rule, per name: the default root with no child comes straight back on compile.
    const bool bLoneDefaultRoot = Node && Node == SCS->GetDefaultSceneRootNode() && Node->GetChildNodes().Num() == 0;
    TSharedPtr<FJsonObject> One = McpHandlerUtils::CreateResultObject();
    One->SetStringField(TEXT("componentName"), Name);
    One->SetBoolField(TEXT("success"), Node && !bLoneDefaultRoot);
    if (Node && !bLoneDefaultRoot) {
      SCS->RemoveNodeAndPromoteChildren(Node);
      ++Removed;
    } else {
      One->SetStringField(TEXT("error"), Node ? TEXT("DefaultSceneRoot is the only scene root and the engine re-creates it")
                                              : TEXT("Component not found"));
      Missing.Add(Name);
    }
    Rows.Add(One);
  }
  bool bCompiled = false;
  bool bSaved = false;
  if (Removed > 0) {
    FinalizeBlueprintSCSChange(Blueprint, bCompiled, bSaved);
  }
  // As the single removal looks after its compile: a node that is back was not removed.
  TArray<TSharedPtr<FJsonValue>> Results;
  for (const TSharedPtr<FJsonObject> &One : Rows) {
    const FString Name = One->GetStringField(TEXT("componentName"));
    if (One->GetBoolField(TEXT("success")) && FindScsNodeByName(SCS, Name)) {
      One->SetBoolField(TEXT("success"), false);
      One->SetStringField(TEXT("error"), TEXT("still in the Blueprint after the compile"));
      Missing.Add(Name);
      --Removed;
    }
    Results.Add(MakeShared<FJsonValueObject>(One));
  }
  Result->SetArrayField(TEXT("results"), Results);
  Result->SetNumberField(TEXT("removed"), Removed);
  Result->SetBoolField(TEXT("compiled"), bCompiled);
  Result->SetBoolField(TEXT("saved"), bSaved);
  Result->SetBoolField(TEXT("success"), Missing.Num() == 0);
  Result->SetStringField(TEXT("message"), Missing.Num() == 0
      ? FString::Printf(TEXT("Removed %d SCS components"), Removed)
      : FString::Printf(TEXT("Removed %d of %d SCS components; not removed: %s"), Removed,
                        ComponentNames.Num(), *FString::Join(Missing, TEXT(", "))));
  if (Missing.Num() > 0) {
    Result->SetStringField(TEXT("error"), Result->GetStringField(TEXT("message")));
    Result->SetStringField(TEXT("errorCode"), TEXT("SCS_REMOVE_INCOMPLETE"));
  }
  McpHandlerUtils::AddVerification(Result, Blueprint);
  return Result;
}
