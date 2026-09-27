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
