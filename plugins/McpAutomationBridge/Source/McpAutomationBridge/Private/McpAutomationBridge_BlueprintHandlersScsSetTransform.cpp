#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
bool HandleScsSetTransform(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_SCS_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("set_scs_transform"))) {
    UBlueprint *Blueprint = ResolveBlueprint();
    if (!Blueprint) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("set_scs_transform requires a valid blueprint"), nullptr,
          TEXT("INVALID_BLUEPRINT"));
      return true;
    }

    FString ComponentName;
    Payload->TryGetStringField(TEXT("componentName"), ComponentName);

    if (ComponentName.IsEmpty()) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("set_scs_transform requires componentName"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Get SCS with explicit null check
    USimpleConstructionScript *SCS = Blueprint->SimpleConstructionScript;
    if (!SCS) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("Blueprint does not have a SimpleConstructionScript"), nullptr,
          TEXT("NO_SCS"));
      return true;
    }

    // Find the SCS node by component name
    const TArray<USCS_Node *> &AllNodes = SCS->GetAllNodes();
    for (USCS_Node *Node : AllNodes) {
      if (Node && Node->GetVariableName().IsValid() &&
          Node->GetVariableName().ToString() == ComponentName) {
        // Read transform from payload
        const TArray<TSharedPtr<FJsonValue>> *LocationArray = nullptr;
        const TArray<TSharedPtr<FJsonValue>> *RotationArray = nullptr;
        const TArray<TSharedPtr<FJsonValue>> *ScaleArray = nullptr;

        FVector Location(0, 0, 0);
        FRotator Rotation(0, 0, 0);
        FVector Scale(1, 1, 1);

        if (Payload->TryGetArrayField(TEXT("location"), LocationArray) &&
            LocationArray->Num() >= 3) {
          Location.X = (*LocationArray)[0]->AsNumber();
          Location.Y = (*LocationArray)[1]->AsNumber();
          Location.Z = (*LocationArray)[2]->AsNumber();
        }

        if (Payload->TryGetArrayField(TEXT("rotation"), RotationArray) &&
            RotationArray->Num() >= 3) {
          Rotation.Pitch = (*RotationArray)[0]->AsNumber();
          Rotation.Yaw = (*RotationArray)[1]->AsNumber();
          Rotation.Roll = (*RotationArray)[2]->AsNumber();
        }

        if (Payload->TryGetArrayField(TEXT("scale"), ScaleArray) &&
            ScaleArray->Num() >= 3) {
          Scale.X = (*ScaleArray)[0]->AsNumber();
          Scale.Y = (*ScaleArray)[1]->AsNumber();
          Scale.Z = (*ScaleArray)[2]->AsNumber();
        }

        // Set the node transform (USCS_Node doesn't have SetRelativeTransform,
        // need to use the component template)
        bool bModified = false;
        if (UActorComponent *ComponentTemplate = Node->ComponentTemplate) {
          if (USceneComponent *SceneTemplate =
                  Cast<USceneComponent>(ComponentTemplate)) {
            SceneTemplate->SetRelativeTransform(
                FTransform(Rotation, Location, Scale));
            bModified = true;
          }
        }

        // Compile and save the blueprint
        bool bCompiled = false;
        bool bSaved = false;
        if (bModified) {
          FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
          bCompiled = McpSafeCompileBlueprint(Blueprint);

          bSaved = SaveLoadedAssetThrottled(Blueprint);
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("componentName"), ComponentName);
        Result->SetNumberField(TEXT("locationX"), Location.X);
        Result->SetNumberField(TEXT("locationY"), Location.Y);
        Result->SetNumberField(TEXT("locationZ"), Location.Z);
        Result->SetBoolField(TEXT("compiled"), bCompiled);
        Result->SetBoolField(TEXT("saved"), bSaved);
        Bridge.SendAutomationResponse(
            RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Set transform for component %s"),
                            *ComponentName),
            Result, FString());
        return true;
      }
    }

    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, false,
        FString::Printf(TEXT("Component %s not found in SCS"), *ComponentName),
        nullptr, TEXT("COMPONENT_NOT_FOUND"));
    return true;
  }

  // Remove SCS component
  return false;
}
#endif
} // namespace McpBlueprintHandlers
