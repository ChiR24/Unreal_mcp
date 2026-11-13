#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/SCS/McpSCSHandlers.h"
#include "Domains/SCS/McpSCSSupport.h"
#include "Domains/Blueprint/Components/McpBPScsPropagate.h"

#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Foundation/BridgeHelpers/Blueprints/McpPropCdoComponents.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"

using namespace McpSCSHandlers;

TSharedPtr<FJsonObject> FSCSHandlers::SetSCSComponentProperty(
    const FString &BlueprintPath, const FString &ComponentName,
    const FString &PropertyName, const TSharedPtr<FJsonValue> &PropertyValue) {
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

  UBlueprint *Blueprint = LoadScsBlueprint(BlueprintPath, Result);
  if (!Blueprint) {
    return Result;
  }

  USimpleConstructionScript *SCS = Blueprint->SimpleConstructionScript;

  // Resolve through the shared component resolver instead of scanning only this Blueprint's own
  // SCS. The local-only scan missed three whole families: components inherited from a parent
  // Blueprint, native components on the CDO, and the UPROPERTY aliases native components are
  // normally referred to by (ACharacter's `Mesh`). bCreateInheritedOverride=true so that editing an
  // inherited component writes an override template on *this* Blueprint rather than mutating the
  // parent's archetype.
  UObject *CDO = Blueprint->GeneratedClass
                     ? Blueprint->GeneratedClass->GetDefaultObject()
                     : nullptr;
  bool bFoundComponent = false;
  // bCreateInheritedOverride=true has a SIDE EFFECT: for an inherited component it calls Blueprint->Modify()
  // and CreateOverridenComponentTemplate(), i.e. it permanently adds an ICH override and dirties the package
  // BEFORE we know whether the property name or value is even valid. Every early return below must therefore
  // undo it, or a *failed* call (one typo in propertyName) silently stops the child inheriting future parent
  // edits to that component — and the next successful bridge call on the Blueprint saves that to disk. Same
  // shape as upstream's own McpPropertyCdoComponents caller in ...\Domains\Property\...ObjectSet.cpp.
  McpPropertyCdoComponents::FCreatedInheritedOverride CreatedOverride;
  UObject *ComponentTemplate = McpPropertyCdoComponents::FindCdoComponent(
      Blueprint, CDO, ComponentName, /*bCreateInheritedOverride=*/true,
      &CreatedOverride,
      &bFoundComponent);

  if (!ComponentTemplate) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("errorCode"),
                           TEXT("SCS_COMPONENT_TEMPLATE_NOT_FOUND"));
    if (bFoundComponent) {
      // Exists, but this Blueprint may not legally override it.
      Result->SetStringField(
          TEXT("error"),
          FString::Printf(
              TEXT("Component '%s' is inherited and cannot be overridden on "
                   "this Blueprint. Set the property on the owning parent "
                   "Blueprint instead."),
              *ComponentName));
      return Result;
    }
    const TArray<FString> Available =
        McpPropertyCdoComponents::CollectResolvableComponentNames(Blueprint, CDO);
    Result->SetArrayField(TEXT("availableComponents"), McpHandlerUtils::ToJsonStringArray(Available));
    Result->SetStringField(
        TEXT("error"),
        FString::Printf(
            TEXT("Component or template not found: %s. Available components: %s"),
            *ComponentName,
            Available.Num() > 0 ? *FString::Join(Available, TEXT(", "))
                                : TEXT("<none>")));
    return Result;
  }

  UActorComponent *TemplateComponent = Cast<UActorComponent>(ComponentTemplate);
  // Placed actors keep their old component values across the recompile unless the
  // change is pushed to them (McpScsPropagate); one someone overrode keeps its own.
  McpScsPropagate::FDefaults Defaults{ComponentTemplate, Blueprint, FName(*ComponentName)};
  if (PropertyValue.IsValid() && McpIsCollisionSetterKey(TemplateComponent, PropertyName)) {
    Defaults.CaptureCollision();
    // Collision goes through the setters, as control_actor and modify_scs do.
    FString CollisionError;
    if (!McpApplyCollisionSetterKey(TemplateComponent, PropertyName, PropertyValue, CollisionError)) {
      SCSFail(Result, CollisionError, TEXT("SCS_PROPERTY_APPLY_FAILED"));
      CreatedOverride.Rollback();
      return Result;
    }
  } else if (PropertyValue.IsValid()) {
    void *ContainerPtr = nullptr;
    FString ResolveError;
    FString ResolvedPath;
    FString FailureMessage;
    // The shared resolver: dotted struct paths at any depth, and a bare name
    // that lives in exactly one struct member.
    FProperty *TargetProp = McpResolvePropertyPath(
        ComponentTemplate, PropertyName, ContainerPtr, ResolvedPath, ResolveError);

    if (!TargetProp || !ContainerPtr) {
      Result->SetBoolField(TEXT("success"), false);
      Result->SetStringField(
          TEXT("error"),
          ResolveError.IsEmpty()
              ? FString::Printf(TEXT("Property not found: %s"), *PropertyName)
              : ResolveError);
      Result->SetStringField(TEXT("errorCode"), TEXT("SCS_PROPERTY_NOT_FOUND"));
      CreatedOverride.Rollback();
      return Result;
    }

    Defaults.Capture(ResolvedPath);
    if (!ApplyJsonValueToProperty(ContainerPtr, TargetProp, PropertyValue,
                                  FailureMessage)) {
      SCSFail(Result, FailureMessage.IsEmpty() ? TEXT("Failed to apply property value") : FailureMessage,
              TEXT("SCS_PROPERTY_APPLY_FAILED"));
      CreatedOverride.Rollback();
      return Result;
    }
  } else {
    SCSFail(Result, TEXT("Property value is invalid"), TEXT("SCS_PROPERTY_INVALID_VALUE"));
    CreatedOverride.Rollback();
    return Result;
  }

  FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
  bool bCompiled = false;
  bool bSaved = false;
  FinalizeTemplateEdit(Blueprint, Defaults, Result, bCompiled, bSaved);

  // Re-resolve through the same resolver: an inherited or native component has no SCS node on this
  // Blueprint, so looking one up here would fail verification for exactly the cases the fix above
  // just enabled. The node lookup is kept only to enrich the response when one happens to exist.
  USCS_Node *VerifiedNode = FindScsNodeByName(SCS, ComponentName);
  UObject *VerifiedTemplate = McpPropertyCdoComponents::FindCdoComponent(
      Blueprint,
      Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject()
                                : nullptr,
      ComponentName, /*bCreateInheritedOverride=*/false);
  if (!VerifiedTemplate) {
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
  FString VerifiedPath;
  FProperty *VerifiedProp = McpResolvePropertyPath(
      VerifiedTemplate, PropertyName, VerifiedContainerPtr, VerifiedPath,
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
      McpPropertyReflection::ExportPropertyToJsonValue(VerifiedContainerPtr, VerifiedProp);

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

  return Result;
}
