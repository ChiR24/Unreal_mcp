#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_SCSHandlers.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Components/SceneComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#endif

TSharedPtr<FJsonObject>
FSCSHandlers::GetBlueprintSCS(const FString &BlueprintPath) {
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
            ? FString::Printf(
                  TEXT(
                      "Blueprint not found or not a valid Blueprint asset: %s"),
                  *BlueprintPath)
            : ErrorMsg);
    return Result;
  }

  TArray<UBlueprint *> Chain;
  {
    UBlueprint *Current = Blueprint;
    while (Current) {
      Chain.Add(Current);
      UClass *ParentClass = Current->ParentClass;
      UBlueprint *ParentBP =
          ParentClass ? Cast<UBlueprint>(ParentClass->ClassGeneratedBy)
                      : nullptr;
      if (!ParentBP || ParentBP == Current || Chain.Contains(ParentBP)) {
        break;
      }
      Current = ParentBP;
    }
  }

  TArray<TSharedPtr<FJsonValue>> Components;
  for (int32 ChainIdx = Chain.Num() - 1; ChainIdx >= 0; --ChainIdx) {
    UBlueprint *CurrentBP = Chain[ChainIdx];
    USimpleConstructionScript *SCS = CurrentBP->SimpleConstructionScript;
    if (!SCS) {
      continue;
    }
    const bool bInherited = (CurrentBP != Blueprint);
    const TArray<USCS_Node *> &AllNodes = SCS->GetAllNodes();

    for (USCS_Node *Node : AllNodes) {
      if (Node && Node->GetVariableName().IsValid()) {
        TSharedPtr<FJsonObject> ComponentObj =
            McpHandlerUtils::CreateResultObject();
        ComponentObj->SetStringField(TEXT("name"),
                                     Node->GetVariableName().ToString());
        ComponentObj->SetStringField(
            TEXT("class"), Node->ComponentClass ? Node->ComponentClass->GetName()
                                                : TEXT("Unknown"));
        if (!Node->ParentComponentOrVariableName.IsNone()) {
          ComponentObj->SetStringField(
              TEXT("parent"), Node->ParentComponentOrVariableName.ToString());
        }

        if (Node->ComponentTemplate) {
          FTransform Transform = FTransform::Identity;
          if (USceneComponent *SceneComp =
                  Cast<USceneComponent>(Node->ComponentTemplate)) {
            Transform = SceneComp->GetRelativeTransform();
          }
          TSharedPtr<FJsonObject> TransformObj =
              McpHandlerUtils::CreateResultObject();

          FVector Loc = Transform.GetLocation();
          FRotator Rot = Transform.GetRotation().Rotator();
          FVector Scale = Transform.GetScale3D();

          TransformObj->SetStringField(
              TEXT("location"),
              FString::Printf(TEXT("X=%.2f Y=%.2f Z=%.2f"), Loc.X, Loc.Y,
                              Loc.Z));
          TransformObj->SetStringField(
              TEXT("rotation"), FString::Printf(TEXT("P=%.2f Y=%.2f R=%.2f"),
                                                Rot.Pitch, Rot.Yaw, Rot.Roll));
          TransformObj->SetStringField(
              TEXT("scale"), FString::Printf(TEXT("X=%.2f Y=%.2f Z=%.2f"),
                                             Scale.X, Scale.Y, Scale.Z));

          ComponentObj->SetObjectField(TEXT("transform"), TransformObj);
        }

        ComponentObj->SetNumberField(TEXT("child_count"),
                                     Node->GetChildNodes().Num());

        if (bInherited) {
          ComponentObj->SetBoolField(TEXT("inherited"), true);
          ComponentObj->SetStringField(TEXT("declaringBlueprint"),
                                       CurrentBP->GetName());
        }

        Components.Add(MakeShared<FJsonValueObject>(ComponentObj));
      }
    }
  }

  Result->SetBoolField(TEXT("success"), true);
  Result->SetArrayField(TEXT("components"), Components);
  Result->SetNumberField(TEXT("count"), Components.Num());
  Result->SetStringField(TEXT("blueprint_path"), BlueprintPath);
  McpHandlerUtils::AddVerification(Result, Blueprint);
#else
  Result->SetBoolField(TEXT("success"), false);
  Result->SetStringField(TEXT("error"),
                         TEXT("SCS operations require editor build"));
  return Result;
#endif

  return Result;
}
