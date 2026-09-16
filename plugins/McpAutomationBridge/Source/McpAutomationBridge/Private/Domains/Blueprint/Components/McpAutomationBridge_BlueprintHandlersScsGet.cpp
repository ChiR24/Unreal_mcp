#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#if WITH_EDITOR
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
bool HandleScsGet(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_SCS_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("get_scs"))) {
    UBlueprint *Blueprint = ResolveBlueprint();
    if (!Blueprint) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("get_scs requires a valid blueprint"),
                             nullptr, TEXT("INVALID_BLUEPRINT"));
      return true;
    }

    TArray<TSharedPtr<FJsonValue>> ComponentsArray;

    // Get SCS with explicit null check
    USimpleConstructionScript *SCS = Blueprint->SimpleConstructionScript;
    if (SCS) {
      const TArray<USCS_Node *> &AllNodes = SCS->GetAllNodes();
      for (USCS_Node *Node : AllNodes) {
        if (Node && Node->GetVariableName().IsValid()) {
          TSharedPtr<FJsonObject> ComponentObj = McpHandlerUtils::CreateResultObject();
          ComponentObj->SetStringField(TEXT("componentName"),
                                       Node->GetVariableName().ToString());
          ComponentObj->SetStringField(TEXT("componentType"),
                                       Node->ComponentClass
                                           ? Node->ComponentClass->GetName()
                                           : TEXT("Unknown"));

          // Add parent info if available
          // USCS_Node doesn't have GetParent() - use
          // ParentComponentOrVariableName instead
          // SCS-owned parents are found through the tree; ParentComponentOrVariableName
          // only names inherited native parents (dogfood #193: reparented nodes showed no parent).
          if (USCS_Node *ParentNode = SCS->FindParentNode(Node)) {
            ComponentObj->SetStringField(TEXT("parentComponent"), ParentNode->GetVariableName().ToString());
          } else if (!Node->ParentComponentOrVariableName.IsNone()) {
            ComponentObj->SetStringField(TEXT("parentComponent"), Node->ParentComponentOrVariableName.ToString());
          }
          // A non-scene component (RotatingMovementComponent, a movement or
          // audio component) is a top-level SCS node but is not the Blueprint's
          // root: reporting isRoot for both made a pickup look like it had two
          // roots. Distinguish the two, and name an inherited parent.
          const bool bIsSceneComponent =
              Node->ComponentClass &&
              Node->ComponentClass->IsChildOf(USceneComponent::StaticClass());
          ComponentObj->SetBoolField(TEXT("isSceneComponent"), bIsSceneComponent);
          ComponentObj->SetBoolField(
              TEXT("isRoot"),
              bIsSceneComponent && SCS->GetRootNodes().Contains(Node));
          if (Node->bIsParentComponentNative &&
              !Node->ParentComponentOrVariableName.IsNone()) {
            ComponentObj->SetBoolField(TEXT("parentIsInherited"), true);
          }

          // Add transform
          // Get component transform from template
          FTransform Transform;
          if (UActorComponent *ComponentTemplate = Node->ComponentTemplate) {
            if (USceneComponent *SceneTemplate =
                    Cast<USceneComponent>(ComponentTemplate)) {
              Transform = SceneTemplate->GetRelativeTransform();
            }
          } else {
            Transform = FTransform::Identity;
          }
          TSharedPtr<FJsonObject> TransformObj = McpHandlerUtils::CreateResultObject();

          TSharedPtr<FJsonObject> LocationObj = McpHandlerUtils::CreateResultObject();
          LocationObj->SetNumberField(TEXT("x"), Transform.GetLocation().X);
          LocationObj->SetNumberField(TEXT("y"), Transform.GetLocation().Y);
          LocationObj->SetNumberField(TEXT("z"), Transform.GetLocation().Z);
          TransformObj->SetObjectField(TEXT("location"), LocationObj);

          TSharedPtr<FJsonObject> RotationObj = McpHandlerUtils::CreateResultObject();
          RotationObj->SetNumberField(TEXT("pitch"),
                                      Transform.GetRotation().Rotator().Pitch);
          RotationObj->SetNumberField(TEXT("yaw"),
                                      Transform.GetRotation().Rotator().Yaw);
          RotationObj->SetNumberField(TEXT("roll"),
                                      Transform.GetRotation().Rotator().Roll);
          TransformObj->SetObjectField(TEXT("rotation"), RotationObj);

          TSharedPtr<FJsonObject> ScaleObj = McpHandlerUtils::CreateResultObject();
          ScaleObj->SetNumberField(TEXT("x"), Transform.GetScale3D().X);
          ScaleObj->SetNumberField(TEXT("y"), Transform.GetScale3D().Y);
          ScaleObj->SetNumberField(TEXT("z"), Transform.GetScale3D().Z);
          TransformObj->SetObjectField(TEXT("scale"), ScaleObj);

          ComponentObj->SetObjectField(TEXT("transform"), TransformObj);
          ComponentsArray.Add(MakeShared<FJsonValueObject>(ComponentObj));
        }
      }
    }

    // A Character Blueprint with no SCS nodes reported "Retrieved 0 SCS
    // components", which reads as "this Blueprint has no components" -- it has
    // several, inherited from its parent class, and they are the ones callers
    // need to name as an attach parent. List them alongside, marked inherited.
    TArray<TSharedPtr<FJsonValue>> InheritedArray;
    UClass *ParentClass = Blueprint ? Blueprint->ParentClass : nullptr;
    if (AActor *ParentCDO =
            ParentClass ? Cast<AActor>(ParentClass->GetDefaultObject()) : nullptr) {
      for (UActorComponent *Comp : ParentCDO->GetComponents()) {
        if (!Comp) {
          continue;
        }
        TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
        Obj->SetStringField(TEXT("componentName"), Comp->GetName());
        Obj->SetStringField(TEXT("componentType"), Comp->GetClass()->GetName());
        Obj->SetBoolField(TEXT("inherited"), true);
        Obj->SetBoolField(TEXT("isSceneComponent"),
                          Cast<USceneComponent>(Comp) != nullptr);
        Obj->SetStringField(TEXT("ownerClass"), ParentClass->GetName());
        InheritedArray.Add(MakeShared<FJsonValueObject>(Obj));
      }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("components"), ComponentsArray);
    Result->SetNumberField(TEXT("componentCount"), ComponentsArray.Num());
    Result->SetArrayField(TEXT("inheritedComponents"), InheritedArray);
    Result->SetNumberField(TEXT("inheritedComponentCount"), InheritedArray.Num());
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Retrieved %d SCS component(s) and %d inherited component(s)"),
                        ComponentsArray.Num(), InheritedArray.Num()),
        Result, FString());
    return true;
  }

  return false;
}
#endif
} // namespace McpBlueprintHandlers
