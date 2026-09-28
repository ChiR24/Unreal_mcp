#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/SCS/McpAutomationBridge_SCSHandlers.h"
#include "Domains/SCS/McpAutomationBridge_SCSHandlersSupport.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"

using namespace McpSCSHandlers;

TSharedPtr<FJsonObject>
FSCSHandlers::RemoveSCSComponent(const FString &BlueprintPath,
                                 const FString &ComponentName) {
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

  UBlueprint *Blueprint = LoadScsBlueprint(BlueprintPath, Result);
  if (!Blueprint) {
    return Result;
  }

  USimpleConstructionScript *SCS = Blueprint->SimpleConstructionScript;

  USCS_Node *NodeToRemove = FindSCSNodeByVariableName(SCS, ComponentName);

  if (!NodeToRemove) {
    return SCSFail(Result, FString::Printf(TEXT("Component not found: %s"), *ComponentName), TEXT("SCS_COMPONENT_NOT_FOUND"));
  }

  SCS->RemoveNode(NodeToRemove);

  bool bCompiled = false;
  bool bSaved = false;
  FinalizeBlueprintSCSChange(Blueprint, bCompiled, bSaved);

  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(
      TEXT("message"),
      FString::Printf(TEXT("Component '%s' removed from SCS"), *ComponentName));
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
  TArray<TSharedPtr<FJsonValue>> Results;
  TArray<FString> Missing;
  for (const FString &Name : ComponentNames) {
    USCS_Node *Node = FindSCSNodeByVariableName(SCS, Name);
    TSharedPtr<FJsonObject> One = McpHandlerUtils::CreateResultObject();
    One->SetStringField(TEXT("componentName"), Name);
    One->SetBoolField(TEXT("success"), Node != nullptr);
    if (Node) {
      SCS->RemoveNode(Node);
    } else {
      One->SetStringField(TEXT("error"), TEXT("Component not found"));
      Missing.Add(Name);
    }
    Results.Add(MakeShared<FJsonValueObject>(One));
  }
  bool bCompiled = false;
  bool bSaved = false;
  if (Missing.Num() < ComponentNames.Num()) {
    FinalizeBlueprintSCSChange(Blueprint, bCompiled, bSaved);
  }
  const int32 Removed = ComponentNames.Num() - Missing.Num();
  Result->SetArrayField(TEXT("results"), Results);
  Result->SetNumberField(TEXT("removed"), Removed);
  Result->SetBoolField(TEXT("compiled"), bCompiled);
  Result->SetBoolField(TEXT("saved"), bSaved);
  Result->SetBoolField(TEXT("success"), Missing.Num() == 0);
  Result->SetStringField(TEXT("message"), Missing.Num() == 0
      ? FString::Printf(TEXT("Removed %d SCS components"), Removed)
      : FString::Printf(TEXT("Removed %d of %d SCS components; not found: %s"), Removed,
                        ComponentNames.Num(), *FString::Join(Missing, TEXT(", "))));
  if (Missing.Num() > 0) {
    Result->SetStringField(TEXT("error"), Result->GetStringField(TEXT("message")));
    Result->SetStringField(TEXT("errorCode"), TEXT("SCS_REMOVE_INCOMPLETE"));
  }
  McpHandlerUtils::AddVerification(Result, Blueprint);
  return Result;
}
