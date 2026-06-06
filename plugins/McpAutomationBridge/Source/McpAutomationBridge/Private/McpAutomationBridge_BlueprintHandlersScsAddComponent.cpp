#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Components/ActorComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
bool HandleScsAddComponent(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_SCS_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("add_component")) ||
      ActionMatchesPattern(TEXT("add_scs_component"))) {
    UBlueprint *Blueprint = ResolveBlueprint();
    if (!Blueprint) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("add_component requires a valid blueprint"),
                             nullptr, TEXT("INVALID_BLUEPRINT"));
      return true;
    }

    FString ComponentType;
    Payload->TryGetStringField(TEXT("componentType"), ComponentType);
    FString ComponentName;
    Payload->TryGetStringField(TEXT("componentName"), ComponentName);

    if (ComponentType.IsEmpty() || ComponentName.IsEmpty()) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("add_component requires componentType and componentName"),
          nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Get the SCS from the blueprint with explicit null check
    USimpleConstructionScript *SCS = Blueprint->SimpleConstructionScript;
    if (!SCS) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("Blueprint does not have a SimpleConstructionScript"), nullptr,
          TEXT("NO_SCS"));
      return true;
    }

    // Find component class
    UClass *ComponentClass = nullptr;
    if (ComponentType == TEXT("StaticMeshComponent")) {
      ComponentClass = UStaticMeshComponent::StaticClass();
    } else if (ComponentType == TEXT("SceneComponent")) {
      ComponentClass = USceneComponent::StaticClass();
    } else if (ComponentType == TEXT("ArrowComponent")) {
      ComponentClass = UArrowComponent::StaticClass();
    } else {
      // Try to load the class
      ComponentClass = LoadClass<UActorComponent>(nullptr, *ComponentType);
    }

    if (!ComponentClass) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Unknown component type: %s"), *ComponentType),
          nullptr, TEXT("INVALID_COMPONENT_TYPE"));
      return true;
    }

    // UE 5.7+: Let SCS create and own both the node and component template.
    USCS_Node *NewNode = SCS->CreateNode(ComponentClass, FName(*ComponentName));
    if (NewNode) {
      SCS->AddNode(NewNode);

      // Compile and save the blueprint
      bool bCompiled = false;
      bool bSaved = false;
      FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
      bCompiled = McpSafeCompileBlueprint(Blueprint);

      bSaved = SaveLoadedAssetThrottled(Blueprint);

      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("componentName"), ComponentName);
      Result->SetStringField(TEXT("componentType"), ComponentType);
      Result->SetStringField(TEXT("variableName"),
                             NewNode->GetVariableName().ToString());
      Result->SetBoolField(TEXT("compiled"), bCompiled);
      Result->SetBoolField(TEXT("saved"), bSaved);
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, true,
          FString::Printf(TEXT("Added component %s to blueprint SCS"),
                          *ComponentName),
          Result, FString());
      return true;
    }

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Failed to add component to SCS"), nullptr,
                           TEXT("OPERATION_FAILED"));
    return true;
  }

  // Set SCS transform
  return false;
}
#endif
} // namespace McpBlueprintHandlers
