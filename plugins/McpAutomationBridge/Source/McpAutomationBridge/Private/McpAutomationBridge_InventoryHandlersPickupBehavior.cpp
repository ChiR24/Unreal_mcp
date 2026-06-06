#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_InventoryHandlersShared.h"

bool HandleInventoryPickupBehaviorActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (SubAction == TEXT("configure_pickup_respawn")) {
    FString PickupPath = GetPayloadString(Payload, TEXT("pickupPath"));
    bool Respawnable = GetPayloadBool(Payload, TEXT("respawnable"), false);
    double RespawnTime = GetPayloadNumber(Payload, TEXT("respawnTime"), 30.0);

    if (PickupPath.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: pickupPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Load the pickup blueprint
    UBlueprint* Blueprint =
        Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *PickupPath));
    if (!Blueprint) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Pickup blueprint not found: %s"), *PickupPath),
          TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    // Add respawn-related Blueprint variables
    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

    // Add bRespawnable variable
    bool bRespawnableExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("bRespawnable")) {
        bRespawnableExists = true;
        break;
      }
    }
    if (!bRespawnableExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bRespawnable"), BoolType);
    }

    // Add RespawnTime variable
    bool bRespawnTimeExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("RespawnTime")) {
        bRespawnTimeExists = true;
        break;
      }
    }
    if (!bRespawnTimeExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("RespawnTime"), FloatType);
    }

    // Set default values on the CDO if available
    if (Blueprint->GeneratedClass) {
      UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
      if (CDO) {
        FProperty* RespawnableProp = CDO->GetClass()->FindPropertyByName(TEXT("bRespawnable"));
        if (RespawnableProp) {
          TSharedPtr<FJsonValue> BoolValue = MakeShared<FJsonValueBoolean>(Respawnable);
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, RespawnableProp, BoolValue, ApplyError);
        }

        FProperty* RespawnTimeProp = CDO->GetClass()->FindPropertyByName(TEXT("RespawnTime"));
        if (RespawnTimeProp) {
          TSharedPtr<FJsonValue> FloatValue = MakeShared<FJsonValueNumber>(RespawnTime);
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, RespawnTimeProp, FloatValue, ApplyError);
        }
      }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("pickupPath"), PickupPath);
    Result->SetBoolField(TEXT("respawnable"), Respawnable);
    Result->SetNumberField(TEXT("respawnTime"), RespawnTime);
    Result->SetBoolField(TEXT("configured"), true);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Pickup respawn configured"), Result);
    return true;
  }

  if (SubAction == TEXT("configure_pickup_effects")) {
    FString PickupPath = GetPayloadString(Payload, TEXT("pickupPath"));
    bool bBobbing = GetPayloadBool(Payload, TEXT("bobbing"), true);
    bool bRotation = GetPayloadBool(Payload, TEXT("rotation"), true);
    bool bGlowEffect = GetPayloadBool(Payload, TEXT("glowEffect"), false);

    if (PickupPath.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: pickupPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Load the pickup blueprint
    UBlueprint* Blueprint =
        Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *PickupPath));
    if (!Blueprint) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Pickup blueprint not found: %s"), *PickupPath),
          TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    // Add effect-related Blueprint variables
    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

    // Add effect control variables
    TArray<TPair<FName, bool>> EffectVars = {
      TPair<FName, bool>(TEXT("bEnableBobbing"), bBobbing),
      TPair<FName, bool>(TEXT("bEnableRotation"), bRotation),
      TPair<FName, bool>(TEXT("bEnableGlowEffect"), bGlowEffect)
    };

    for (const auto& VarPair : EffectVars) {
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == VarPair.Key) {
          bExists = true;
          break;
        }
      }
      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarPair.Key, BoolType);
      }
    }

    // Add bobbing/rotation parameters
    TArray<FName> FloatVars = {
      TEXT("BobbingSpeed"),
      TEXT("BobbingHeight"),
      TEXT("RotationSpeed")
    };

    for (const FName& VarName : FloatVars) {
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == VarName) {
          bExists = true;
          break;
        }
      }
      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarName, FloatType);
      }
    }

    // Set default values on the CDO if available
    if (Blueprint->GeneratedClass) {
      UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
      if (CDO) {
        for (const auto& VarPair : EffectVars) {
          FProperty* Prop = CDO->GetClass()->FindPropertyByName(VarPair.Key);
          if (Prop) {
            TSharedPtr<FJsonValue> BoolValue = MakeShared<FJsonValueBoolean>(VarPair.Value);
            FString ApplyError;
            ApplyJsonValueToProperty(CDO, Prop, BoolValue, ApplyError);
          }
        }
      }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("pickupPath"), PickupPath);
    Result->SetBoolField(TEXT("bobbing"), bBobbing);
    Result->SetBoolField(TEXT("rotation"), bRotation);
    Result->SetBoolField(TEXT("glowEffect"), bGlowEffect);
    Result->SetBoolField(TEXT("configured"), true);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Pickup effects configured"), Result);
    return true;
  }

  // ===========================================================================
  // 17.4 Equipment System (5 actions)
  // ===========================================================================

  return false;
}
