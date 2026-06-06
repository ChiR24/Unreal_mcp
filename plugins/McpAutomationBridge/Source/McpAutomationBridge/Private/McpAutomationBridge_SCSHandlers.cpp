#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_SCSHandlers.h"
#include "McpAutomationBridge_SCSHandlersSupport.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Components/SceneComponent.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

#if WITH_EDITOR
void FSCSHandlers::FinalizeBlueprintSCSChange(UBlueprint *Blueprint,
                                              bool &bOutCompiled,
                                              bool &bOutSaved) {
  bOutCompiled = false;
  bOutSaved = false;

  if (!Blueprint) {
    return;
  }

  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
  bOutCompiled = McpSafeCompileBlueprint(Blueprint);

  bOutSaved = McpSafeAssetSave(Blueprint);
  if (!bOutSaved) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("McpSafeAssetSave reported failure for '%s' after SCS "
                "change"),
           *Blueprint->GetPathName());
  }
}

namespace McpSCSHandlers {

bool IsPlayInEditorActive() {
  if (GEditor && GEditor->IsPlaySessionInProgress()) {
    return true;
  }
  if (GEditor) {
    for (const FWorldContext &Context : GEngine->GetWorldContexts()) {
      if (Context.WorldType == EWorldType::PIE ||
          Context.WorldType == EWorldType::Game) {
        return true;
      }
    }
  }
  return false;
}

TSharedPtr<FJsonObject> PIEActiveError() {
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), false);
  Result->SetStringField(
      TEXT("error"),
      TEXT("SCS operations cannot modify Blueprints during Play In Editor "
           "(PIE). Please stop the play session first."));
  Result->SetStringField(TEXT("errorCode"), TEXT("PIE_ACTIVE"));
  return Result;
}

FString GetSCSNodeName(const USCS_Node *Node) {
  if (!Node || !Node->GetVariableName().IsValid()) {
    return FString();
  }
  return Node->GetVariableName().ToString();
}

USCS_Node *FindSCSNodeByVariableName(USimpleConstructionScript *SCS,
                                     const FString &Name) {
  if (!SCS || Name.IsEmpty()) {
    return nullptr;
  }
  for (USCS_Node *Node : SCS->GetAllNodes()) {
    if (Node && Node->GetVariableName().IsValid() &&
        Node->GetVariableName().ToString().Equals(Name,
                                                  ESearchCase::IgnoreCase)) {
      return Node;
    }
  }
  return nullptr;
}

USCS_Node *FindSCSParentNode(USimpleConstructionScript *SCS,
                             USCS_Node *ChildNode) {
  if (!SCS || !ChildNode) {
    return nullptr;
  }
  for (USCS_Node *Candidate : SCS->GetAllNodes()) {
    if (Candidate && Candidate->GetChildNodes().Contains(ChildNode)) {
      return Candidate;
    }
  }
  return nullptr;
}

bool IsSCSRootAlias(const FString &Name) {
  return Name.Equals(TEXT("RootComponent"), ESearchCase::IgnoreCase) ||
         Name.Equals(TEXT("DefaultSceneRoot"), ESearchCase::IgnoreCase) ||
         Name.Equals(TEXT("Root"), ESearchCase::IgnoreCase);
}

bool IsSCSRootNode(USimpleConstructionScript *SCS, USCS_Node *Node) {
  return SCS && Node && SCS->GetRootNodes().Contains(Node);
}

TSharedPtr<FJsonObject> MakeTransformJson(const FTransform &Transform) {
  TSharedPtr<FJsonObject> TransformObj = MakeShared<FJsonObject>();
  const FVector Loc = Transform.GetLocation();
  const FRotator Rot = Transform.GetRotation().Rotator();
  const FVector Scale = Transform.GetScale3D();

  TArray<TSharedPtr<FJsonValue>> LocationArray;
  LocationArray.Add(MakeShared<FJsonValueNumber>(Loc.X));
  LocationArray.Add(MakeShared<FJsonValueNumber>(Loc.Y));
  LocationArray.Add(MakeShared<FJsonValueNumber>(Loc.Z));
  TransformObj->SetArrayField(TEXT("location"), LocationArray);

  TArray<TSharedPtr<FJsonValue>> RotationArray;
  RotationArray.Add(MakeShared<FJsonValueNumber>(Rot.Pitch));
  RotationArray.Add(MakeShared<FJsonValueNumber>(Rot.Yaw));
  RotationArray.Add(MakeShared<FJsonValueNumber>(Rot.Roll));
  TransformObj->SetArrayField(TEXT("rotation"), RotationArray);

  TArray<TSharedPtr<FJsonValue>> ScaleArray;
  ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.X));
  ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Y));
  ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Z));
  TransformObj->SetArrayField(TEXT("scale"), ScaleArray);

  return TransformObj;
}

void AddSCSNodeVerification(TSharedPtr<FJsonObject> Result,
                            USimpleConstructionScript *SCS, USCS_Node *Node) {
  if (!Result || !SCS || !Node) {
    return;
  }

  const FString NodeName = GetSCSNodeName(Node);
  TSharedPtr<FJsonObject> Verification = MakeShared<FJsonObject>();
  Verification->SetStringField(TEXT("componentName"), NodeName);
  Verification->SetBoolField(
      TEXT("existsInSCS"), FindSCSNodeByVariableName(SCS, NodeName) == Node);
  Verification->SetBoolField(TEXT("isRoot"), IsSCSRootNode(SCS, Node));
  Verification->SetNumberField(TEXT("childCount"), Node->GetChildNodes().Num());
  if (Node->ComponentClass) {
    Verification->SetStringField(TEXT("componentClass"),
                                 Node->ComponentClass->GetName());
  }

  USCS_Node *ParentNode = FindSCSParentNode(SCS, Node);
  Verification->SetStringField(TEXT("parent"),
                               ParentNode ? GetSCSNodeName(ParentNode)
                                          : TEXT("(root)"));
  Verification->SetBoolField(TEXT("parentVerified"),
                             ParentNode != nullptr || IsSCSRootNode(SCS, Node));

  if (USceneComponent *SceneComp =
          Cast<USceneComponent>(Node->ComponentTemplate)) {
    Verification->SetObjectField(
        TEXT("transform"), MakeTransformJson(SceneComp->GetRelativeTransform()));
  }

  Result->SetObjectField(TEXT("scsVerification"), Verification);
}

bool SCSParentMatches(USimpleConstructionScript *SCS, USCS_Node *Node,
                      const FString &ExpectedParentName) {
  if (!SCS || !Node) {
    return false;
  }
  USCS_Node *ActualParent = FindSCSParentNode(SCS, Node);
  if (ExpectedParentName.IsEmpty()) {
    return ActualParent == nullptr || IsSCSRootNode(SCS, Node);
  }
  if (IsSCSRootAlias(ExpectedParentName)) {
    return ActualParent ? IsSCSRootNode(SCS, ActualParent)
                        : IsSCSRootNode(SCS, Node);
  }
  return ActualParent &&
         GetSCSNodeName(ActualParent).Equals(ExpectedParentName,
                                             ESearchCase::IgnoreCase);
}

}
#endif

#if !WITH_EDITOR
namespace McpSCSHandlers {

TSharedPtr<FJsonObject> UnsupportedSCSAction() {
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), false);
  Result->SetStringField(TEXT("error"),
                         TEXT("SCS operations require editor build"));
  return Result;
}

}
#endif
