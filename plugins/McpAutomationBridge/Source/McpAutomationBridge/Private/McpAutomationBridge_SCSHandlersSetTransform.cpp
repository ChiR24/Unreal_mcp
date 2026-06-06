#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_SCSHandlers.h"
#include "McpAutomationBridge_SCSHandlersSupport.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Components/SceneComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#endif

using namespace McpSCSHandlers;

TSharedPtr<FJsonObject> FSCSHandlers::SetSCSComponentTransform(
    const FString &BlueprintPath, const FString &ComponentName,
    const TSharedPtr<FJsonObject> &TransformData) {
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

  FVector Location(0, 0, 0);
  FRotator Rotation(0, 0, 0);
  FVector Scale(1, 1, 1);

  const TArray<TSharedPtr<FJsonValue>> *LocArray;
  if (TransformData->TryGetArrayField(TEXT("location"), LocArray) &&
      LocArray->Num() >= 3) {
    Location.X = (*LocArray)[0]->AsNumber();
    Location.Y = (*LocArray)[1]->AsNumber();
    Location.Z = (*LocArray)[2]->AsNumber();
  } else {
    const TSharedPtr<FJsonObject> *LocObj = nullptr;
    if (TransformData->TryGetObjectField(TEXT("location"), LocObj) && LocObj &&
        LocObj->IsValid()) {
      (*LocObj)->TryGetNumberField(TEXT("x"), Location.X);
      (*LocObj)->TryGetNumberField(TEXT("y"), Location.Y);
      (*LocObj)->TryGetNumberField(TEXT("z"), Location.Z);
    }
  }

  const TArray<TSharedPtr<FJsonValue>> *RotArray;
  if (TransformData->TryGetArrayField(TEXT("rotation"), RotArray) &&
      RotArray->Num() >= 3) {
    Rotation.Pitch = (*RotArray)[0]->AsNumber();
    Rotation.Yaw = (*RotArray)[1]->AsNumber();
    Rotation.Roll = (*RotArray)[2]->AsNumber();
  } else {
    const TSharedPtr<FJsonObject> *RotObj = nullptr;
    if (TransformData->TryGetObjectField(TEXT("rotation"), RotObj) && RotObj &&
        RotObj->IsValid()) {
      (*RotObj)->TryGetNumberField(TEXT("pitch"), Rotation.Pitch);
      (*RotObj)->TryGetNumberField(TEXT("yaw"), Rotation.Yaw);
      (*RotObj)->TryGetNumberField(TEXT("roll"), Rotation.Roll);
    }
  }

  const TArray<TSharedPtr<FJsonValue>> *ScaleArray;
  if (TransformData->TryGetArrayField(TEXT("scale"), ScaleArray) &&
      ScaleArray->Num() >= 3) {
    Scale.X = (*ScaleArray)[0]->AsNumber();
    Scale.Y = (*ScaleArray)[1]->AsNumber();
    Scale.Z = (*ScaleArray)[2]->AsNumber();
  } else {
    const TSharedPtr<FJsonObject> *ScaleObj = nullptr;
    if (TransformData->TryGetObjectField(TEXT("scale"), ScaleObj) && ScaleObj &&
        ScaleObj->IsValid()) {
      (*ScaleObj)->TryGetNumberField(TEXT("x"), Scale.X);
      (*ScaleObj)->TryGetNumberField(TEXT("y"), Scale.Y);
      (*ScaleObj)->TryGetNumberField(TEXT("z"), Scale.Z);
    }
  }

  FTransform NewTransform(Rotation, Location, Scale);

  if (USceneComponent *SceneComp =
          Cast<USceneComponent>(ComponentNode->ComponentTemplate)) {
    SceneComp->SetRelativeTransform(NewTransform);

    bool bCompiled = false;
    bool bSaved = false;
    FinalizeBlueprintSCSChange(Blueprint, bCompiled, bSaved);

    USCS_Node *VerifiedNode = FindSCSNodeByVariableName(SCS, ComponentName);
    USceneComponent *VerifiedSceneComp =
        VerifiedNode ? Cast<USceneComponent>(VerifiedNode->ComponentTemplate)
                     : nullptr;
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
    AddSCSNodeVerification(Result, SCS, VerifiedNode);
    McpHandlerUtils::AddVerification(Result, Blueprint);
  } else {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        TEXT("Component is not a SceneComponent (no transform)"));
    Result->SetStringField(TEXT("errorCode"), TEXT("SCS_NOT_SCENE_COMPONENT"));
  }
#else
  return UnsupportedSCSAction();
#endif

  return Result;
}
