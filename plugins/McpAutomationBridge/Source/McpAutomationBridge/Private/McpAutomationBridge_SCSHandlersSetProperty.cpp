#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_SCSHandlers.h"
#include "McpAutomationBridge_SCSHandlersSupport.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

using namespace McpSCSHandlers;

TSharedPtr<FJsonObject> FSCSHandlers::SetSCSComponentProperty(
    const FString &BlueprintPath, const FString &ComponentName,
    const FString &PropertyName, const TSharedPtr<FJsonValue> &PropertyValue) {
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
    return Result;
  }

  USimpleConstructionScript *SCS = Blueprint->SimpleConstructionScript;

  USCS_Node *ComponentNode = nullptr;
  for (USCS_Node *Node : SCS->GetAllNodes()) {
    if (Node && Node->GetVariableName().IsValid() &&
        Node->GetVariableName().ToString().Equals(ComponentName,
                                                  ESearchCase::IgnoreCase)) {
      ComponentNode = Node;
      break;
    }
  }

  if (!ComponentNode || !ComponentNode->ComponentTemplate) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Component or template not found: %s"),
                        *ComponentName));
    Result->SetStringField(TEXT("errorCode"),
                           TEXT("SCS_COMPONENT_TEMPLATE_NOT_FOUND"));
    return Result;
  }

  if (PropertyValue.IsValid()) {
    void *ContainerPtr = nullptr;
    FString ResolveError;
    FString FailureMessage;
    FString FailureCode;
    bool bAppliedValue = false;
    FProperty *TargetProp =
        ResolveNestedPropertyPath(ComponentNode->ComponentTemplate,
                                  PropertyName, ContainerPtr, ResolveError);

    if (!TargetProp || !ContainerPtr) {
      Result->SetBoolField(TEXT("success"), false);
      Result->SetStringField(
          TEXT("error"),
          ResolveError.IsEmpty()
              ? FString::Printf(TEXT("Property not found: %s"), *PropertyName)
              : ResolveError);
      Result->SetStringField(TEXT("errorCode"), TEXT("SCS_PROPERTY_NOT_FOUND"));
      return Result;
    }

    if (ApplyJsonValueToProperty(ContainerPtr, TargetProp, PropertyValue,
                                 FailureMessage)) {
      bAppliedValue = true;
    } else {
      FailureCode = TEXT("SCS_PROPERTY_APPLY_FAILED");
    }

    if (!bAppliedValue) {
      Result->SetBoolField(TEXT("success"), false);
      Result->SetStringField(TEXT("error"),
                             FailureMessage.IsEmpty()
                                 ? TEXT("Failed to apply property value")
                                 : FailureMessage);
      if (!FailureCode.IsEmpty()) {
        Result->SetStringField(TEXT("errorCode"), FailureCode);
      }
      return Result;
    }
  } else {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"), TEXT("Property value is invalid"));
    Result->SetStringField(TEXT("errorCode"),
                           TEXT("SCS_PROPERTY_INVALID_VALUE"));
    return Result;
  }

  FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
  bool bCompiled = false;
  bool bSaved = false;
  FinalizeBlueprintSCSChange(Blueprint, bCompiled, bSaved);

  USCS_Node *VerifiedNode = FindSCSNodeByVariableName(SCS, ComponentName);
  if (!VerifiedNode || !VerifiedNode->ComponentTemplate) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Verification failed: Component '%s' missing after property set"),
                        *ComponentName));
    Result->SetStringField(TEXT("errorCode"),
                           TEXT("SCS_PROPERTY_VERIFICATION_FAILED"));
    Result->SetBoolField(TEXT("compiled"), bCompiled);
    Result->SetBoolField(TEXT("saved"), bSaved);
    return Result;
  }

  void *VerifiedContainerPtr = nullptr;
  FString VerifiedResolveError;
  FProperty *VerifiedProp = ResolveNestedPropertyPath(
      VerifiedNode->ComponentTemplate, PropertyName, VerifiedContainerPtr,
      VerifiedResolveError);
  if (!VerifiedProp || !VerifiedContainerPtr) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        VerifiedResolveError.IsEmpty()
            ? FString::Printf(TEXT("Verification failed: Property not found after set: %s"),
                              *PropertyName)
            : VerifiedResolveError);
    Result->SetStringField(TEXT("errorCode"),
                           TEXT("SCS_PROPERTY_VERIFICATION_FAILED"));
    Result->SetBoolField(TEXT("compiled"), bCompiled);
    Result->SetBoolField(TEXT("saved"), bSaved);
    AddSCSNodeVerification(Result, SCS, VerifiedNode);
    return Result;
  }

  TSharedPtr<FJsonValue> VerifiedValue =
      ExportPropertyToJsonValue(VerifiedContainerPtr, VerifiedProp);

  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(
      TEXT("message"),
      FString::Printf(TEXT("Property '%s' set on component '%s'"),
                      *PropertyName, *ComponentName));
  Result->SetBoolField(TEXT("compiled"), bCompiled);
  Result->SetBoolField(TEXT("saved"), bSaved);
  AddSCSNodeVerification(Result, SCS, VerifiedNode);
  if (VerifiedValue.IsValid()) {
    Result->SetField(TEXT("verifiedValue"), VerifiedValue);
  }
  McpHandlerUtils::AddVerification(Result, Blueprint);
#else
  return UnsupportedSCSAction();
#endif

  return Result;
}
