#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
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
          if (!Node->ParentComponentOrVariableName.IsNone()) {
            ComponentObj->SetStringField(
                TEXT("parentComponent"),
                Node->ParentComponentOrVariableName.ToString());
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

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("components"), ComponentsArray);
    Result->SetNumberField(TEXT("componentCount"), ComponentsArray.Num());
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           FString::Printf(TEXT("Retrieved %d SCS components"),
                                           ComponentsArray.Num()),
                           Result, FString());
    return true;
  }

  // Reparent SCS component (simplified implementation)
  return false;
}
#endif
} // namespace McpBlueprintHandlers
