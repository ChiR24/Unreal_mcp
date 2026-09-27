#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/SCS/McpAutomationBridge_SCSHandlers.h"
#include "Domains/SCS/McpAutomationBridge_SCSHandlersSupport.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Components/SceneComponent.h"
#include "Domains/Property/McpAutomationBridge_PropertyHandlersCdoComponents.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"

using namespace McpSCSHandlers;

TSharedPtr<FJsonObject> FSCSHandlers::SetSCSComponentTransform(
    const FString &BlueprintPath, const FString &ComponentName,
    const TSharedPtr<FJsonObject> &TransformData) {
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

  UBlueprint *Blueprint = LoadScsBlueprint(BlueprintPath, Result);
  if (!Blueprint) {
    return Result;
  }

  // Resolve through the same shared resolver set_scs_property uses. Scanning only
  // this Blueprint's own SCS missed every inherited component — including the
  // native Mesh a Character Blueprint gets from ACharacter — so set_transform
  // answered SCS_COMPONENT_TEMPLATE_NOT_FOUND for a component set_property had
  // just written to a moment earlier.
  UObject *CDO = Blueprint->GeneratedClass
                     ? Blueprint->GeneratedClass->GetDefaultObject()
                     : nullptr;
  bool bFoundComponent = false;
  McpPropertyCdoComponents::FCreatedInheritedOverride CreatedOverride;
  UObject *ComponentTemplate = McpPropertyCdoComponents::FindCdoComponent(
      Blueprint, CDO, ComponentName, /*bCreateInheritedOverride=*/true,
      &CreatedOverride.Handler, &CreatedOverride.Key,
      &bFoundComponent);

  if (!ComponentTemplate) {
    return SCSFail(Result, bFoundComponent ? FString::Printf( TEXT("Component '%s' is inherited and cannot be overridden on " "this Blueprint. Set the transform on the owning parent " "Blueprint instead."), *ComponentName) : FString::Printf(TEXT("Component or template not found: %s"), *ComponentName), TEXT("SCS_COMPONENT_TEMPLATE_NOT_FOUND"));
  }

  USceneComponent *SceneComp = Cast<USceneComponent>(ComponentTemplate);
  if (!SceneComp) {
    // Undo the override the resolver may have just created, so a component that
    // has no transform does not leave a stray ICH entry behind.
    CreatedOverride.Rollback();
    return SCSFail(Result, TEXT("Component is not a SceneComponent (no transform)"), TEXT("SCS_NOT_SCENE_COMPONENT"));
  }

  // Read-modify-write: start from the template's CURRENT values so a partial
  // payload (e.g. location only) does not stomp rotation/scale back to defaults.
  const FVector CurrentLocation = SceneComp->GetRelativeLocation();
  const FRotator CurrentRotation = SceneComp->GetRelativeRotation();
  const FVector CurrentScale = SceneComp->GetRelativeScale3D();
  double Location[3] = {CurrentLocation.X, CurrentLocation.Y, CurrentLocation.Z};
  double Rotation[3] = {CurrentRotation.Pitch, CurrentRotation.Yaw, CurrentRotation.Roll};
  double Scale[3] = {CurrentScale.X, CurrentScale.Y, CurrentScale.Z};
  const bool bHasLocation = ReadJsonTriple(TransformData->TryGetField(TEXT("location")), {TEXT("x"), TEXT("y"), TEXT("z")}, Location);
  const bool bHasRotation = ReadJsonTriple(TransformData->TryGetField(TEXT("rotation")), {TEXT("pitch"), TEXT("yaw"), TEXT("roll")}, Rotation);
  const bool bHasScale = ReadJsonTriple(TransformData->TryGetField(TEXT("scale")), {TEXT("x"), TEXT("y"), TEXT("z")}, Scale);

  // Writing engine defaults because the caller's fields never arrived is the
  // silent no-op this handler was bitten by — refuse loudly instead.
  if (!bHasLocation && !bHasRotation && !bHasScale) {
    return SCSFail(Result, TEXT("No transform fields provided — pass at least one of location/" "rotation/scale as a [x,y,z] array (or {x,y,z}/{pitch,yaw,roll} object)."), TEXT("INVALID_ARGUMENT"));
  }

  const FTransform NewTransform(FRotator(Rotation[0], Rotation[1], Rotation[2]),
                                FVector(Location[0], Location[1], Location[2]),
                                FVector(Scale[0], Scale[1], Scale[2]));

  {
    SceneComp->Modify();
    SceneComp->SetRelativeTransform(NewTransform);

    bool bCompiled = false;
    bool bSaved = false;
    FinalizeBlueprintSCSChange(Blueprint, bCompiled, bSaved);

    // An inherited component has no SCS node of its own, so verifying only
    // through the node lookup would report failure for a write that landed. Read
    // the template back instead, and attach node verification when there is one.
    USimpleConstructionScript *SCS = Blueprint->SimpleConstructionScript;
    USCS_Node *VerifiedNode = FindSCSNodeByVariableName(SCS, ComponentName);
    USceneComponent *VerifiedSceneComp =
        VerifiedNode ? Cast<USceneComponent>(VerifiedNode->ComponentTemplate)
                     : SceneComp;
    if (!VerifiedSceneComp ||
        !VerifiedSceneComp->GetRelativeTransform().Equals(NewTransform)) {
      Result->SetBoolField(TEXT("success"), false);
      Result->SetStringField(
          TEXT("error"),
          FString::Printf(TEXT("Verification failed: Transform did not stick for component '%s'"),
                          *ComponentName));
      Result->SetStringField(TEXT("errorCode"),
                             TEXT("SCS_TRANSFORM_VERIFICATION_FAILED"));
      Result->SetBoolField(TEXT("compiled"), bCompiled);
      Result->SetBoolField(TEXT("saved"), bSaved);
      if (VerifiedNode) {
        AddSCSNodeVerification(Result, SCS, VerifiedNode);
      }
      return Result;
    }

    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(
        TEXT("message"),
        FString::Printf(TEXT("Transform set for component '%s'"),
                        *ComponentName));
    Result->SetBoolField(TEXT("compiled"), bCompiled);
    Result->SetBoolField(TEXT("saved"), bSaved);
    if (VerifiedNode) {
      AddSCSNodeVerification(Result, SCS, VerifiedNode);
    }
    McpHandlerUtils::AddVerification(Result, Blueprint);
  }

  return Result;
}
