#include "Domains/SCS/McpAutomationBridge_SCSHandlers.h"

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/SCS/McpAutomationBridge_SCSHandlersSupport.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"
#include "GameFramework/Actor.h"
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
  if (!GEditor) {
    return false;
  }
  if (GEditor->IsPlaySessionInProgress()) {
    return true;
  }
  {
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
  Verification->SetNumberField(TEXT("childCount"), Node->GetChildNodes().Num());
  if (Node->ComponentClass) {
    Verification->SetStringField(TEXT("componentClass"),
                                 Node->ComponentClass->GetName());
  }

  // A node with no SCS parent is not necessarily at the root: it may be attached
  // to an inherited native component, or be a non-scene component sitting at the
  // top of the tree. Reporting "(root)" and isRoot:true for both was how a
  // RotatingMovementComponent came back looking like the Blueprint's root.
  USCS_Node *ParentNode = FindSCSParentNode(SCS, Node);
  const bool bNativeParent =
      !ParentNode && Node->bIsParentComponentNative &&
      !Node->ParentComponentOrVariableName.IsNone();
  const bool bSceneComponent =
      Node->ComponentClass &&
      Node->ComponentClass->IsChildOf(USceneComponent::StaticClass());
  if (ParentNode) {
    Verification->SetStringField(TEXT("parent"), GetSCSNodeName(ParentNode));
  } else if (bNativeParent) {
    Verification->SetStringField(TEXT("parent"),
                                 Node->ParentComponentOrVariableName.ToString());
    Verification->SetBoolField(TEXT("parentIsInherited"), true);
  } else if (!bSceneComponent) {
    // Non-scene components have no attachment at all; the SCS just holds them.
    Verification->SetStringField(TEXT("parent"), TEXT("(none - not a scene component)"));
  } else {
    Verification->SetStringField(TEXT("parent"), TEXT("(root)"));
  }
  Verification->SetBoolField(TEXT("isRoot"),
                             bSceneComponent && !ParentNode && !bNativeParent &&
                                 IsSCSRootNode(SCS, Node));
  Verification->SetBoolField(TEXT("parentVerified"),
                             ParentNode != nullptr || bNativeParent ||
                                 !bSceneComponent || IsSCSRootNode(SCS, Node));

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
    // No specific parent requested: accept the engine's default placements — the node is
    // root, is not yet parented, or was auto-attached as a child of the root (the default
    // for a 2nd component added without an explicit parent).
    return ActualParent == nullptr || IsSCSRootNode(SCS, Node) ||
           IsSCSRootNode(SCS, ActualParent);
  }
  if (IsSCSRootAlias(ExpectedParentName)) {
    return ActualParent ? IsSCSRootNode(SCS, ActualParent)
                        : IsSCSRootNode(SCS, Node);
  }
  if (ActualParent) {
    return GetSCSNodeName(ActualParent)
        .Equals(ExpectedParentName, ESearchCase::IgnoreCase);
  }
  // A node attached to an inherited native component has no SCS parent: the
  // parent is recorded on the node itself. Verifying only the SCS tree rejected
  // every native attach as "parent did not match" after it had already worked.
  if (!Node->bIsParentComponentNative || Node->ParentComponentOrVariableName.IsNone()) {
    return false;
  }
  const FString StoredParent = Node->ParentComponentOrVariableName.ToString();
  if (StoredParent.Equals(ExpectedParentName, ESearchCase::IgnoreCase)) {
    return true;
  }
  // SetParent stores the component's object name (CollisionCylinder) while
  // callers name the property that exposes it (CapsuleComponent). Resolve the
  // requested name on the owner CDO and compare the component itself.
  UClass *OwnerClass = SCS->GetOwnerClass();
  AActor *CDO = OwnerClass ? Cast<AActor>(OwnerClass->GetDefaultObject()) : nullptr;
  if (!CDO) {
    return false;
  }
  for (TFieldIterator<FObjectProperty> It(OwnerClass); It; ++It) {
    FObjectProperty *Prop = *It;
    if (!Prop || !Prop->PropertyClass ||
        !Prop->PropertyClass->IsChildOf(USceneComponent::StaticClass()) ||
        !Prop->GetName().Equals(ExpectedParentName, ESearchCase::IgnoreCase)) {
      continue;
    }
    if (UObject *Value = Prop->GetObjectPropertyValue_InContainer(CDO)) {
      return Value->GetName().Equals(StoredParent, ESearchCase::IgnoreCase);
    }
  }
  return false;
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
