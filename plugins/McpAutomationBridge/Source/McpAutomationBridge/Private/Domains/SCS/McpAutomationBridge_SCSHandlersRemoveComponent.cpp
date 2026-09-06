#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/SCS/McpAutomationBridge_SCSHandlers.h"
#include "Domains/SCS/McpAutomationBridge_SCSHandlersSupport.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#endif

using namespace McpSCSHandlers;

TSharedPtr<FJsonObject>
FSCSHandlers::RemoveSCSComponent(const FString &BlueprintPath,
                                 const FString &ComponentName) {
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

#if WITH_EDITOR
  FString NormalizedPath;
  FString ErrorMsg;
  UBlueprint *Blueprint =
      LoadBlueprintAsset(BlueprintPath, NormalizedPath, ErrorMsg);
  if (!Blueprint) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        ErrorMsg.IsEmpty()
            ? FString::Printf(TEXT("Blueprint asset not found at path: %s"),
                              *BlueprintPath)
            : ErrorMsg);
    Result->SetStringField(TEXT("errorCode"), TEXT("ASSET_NOT_FOUND"));
    return Result;
  }

  if (!Blueprint->SimpleConstructionScript) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Blueprint has no SimpleConstructionScript: %s"),
                        *BlueprintPath));
    Result->SetStringField(TEXT("errorCode"), TEXT("SCS_NOT_FOUND"));
    return Result;
  }

  USimpleConstructionScript *SCS = Blueprint->SimpleConstructionScript;

  USCS_Node *NodeToRemove = FindSCSNodeByVariableName(SCS, ComponentName);

  if (!NodeToRemove) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    Result->SetStringField(TEXT("errorCode"), TEXT("SCS_COMPONENT_NOT_FOUND"));
    return Result;
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
#else
  return UnsupportedSCSAction();
#endif

  return Result;
}
