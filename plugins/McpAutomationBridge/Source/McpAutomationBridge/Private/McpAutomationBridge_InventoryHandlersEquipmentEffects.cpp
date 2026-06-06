#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_InventoryHandlersShared.h"

bool HandleInventoryEquipmentEffectActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (SubAction == TEXT("configure_equipment_effects")) {
    FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));

    if (BlueprintPath.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: blueprintPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Load the blueprint
    UBlueprint* Blueprint =
        Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *BlueprintPath));
    if (!Blueprint) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
          TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    // Add equipment effect configuration variables
    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

    FEdGraphPinType SoftObjectArrayType;
    SoftObjectArrayType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
    SoftObjectArrayType.ContainerType = EPinContainerType::Array;

    FEdGraphPinType NameArrayType;
    NameArrayType.PinCategory = UEdGraphSchema_K2::PC_Name;
    NameArrayType.ContainerType = EPinContainerType::Array;

    // Stat modifier variables
    TArray<TPair<FName, FEdGraphPinType>> EffectVars = {
      TPair<FName, FEdGraphPinType>(TEXT("bApplyStatModifiers"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("StatModifierMultiplier"), FloatType),
      TPair<FName, FEdGraphPinType>(TEXT("bGrantAbilitiesOnEquip"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("GrantedAbilities"), SoftObjectArrayType),
      TPair<FName, FEdGraphPinType>(TEXT("bApplyPassiveEffects"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("PassiveEffectClasses"), SoftObjectArrayType),
      TPair<FName, FEdGraphPinType>(TEXT("EffectTags"), NameArrayType)
    };

    TArray<TSharedPtr<FJsonValue>> AddedVars;

    for (const auto& VarPair : EffectVars) {
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == VarPair.Key) {
          bExists = true;
          break;
        }
      }
      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarPair.Key, VarPair.Value);
        AddedVars.Add(MakeShared<FJsonValueString>(VarPair.Key.ToString()));
      }
    }

    // Set default values on CDO if available
    if (Blueprint->GeneratedClass) {
      UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
      if (CDO) {
        FProperty* StatModProp = CDO->GetClass()->FindPropertyByName(TEXT("bApplyStatModifiers"));
        if (StatModProp) {
          TSharedPtr<FJsonValue> BoolVal = MakeShared<FJsonValueBoolean>(GetPayloadBool(Payload, TEXT("statModifiers"), true));
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, StatModProp, BoolVal, ApplyError);
        }

        FProperty* AbilityProp = CDO->GetClass()->FindPropertyByName(TEXT("bGrantAbilitiesOnEquip"));
        if (AbilityProp) {
          TSharedPtr<FJsonValue> BoolVal = MakeShared<FJsonValueBoolean>(GetPayloadBool(Payload, TEXT("abilityGrants"), true));
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, AbilityProp, BoolVal, ApplyError);
        }

        FProperty* PassiveProp = CDO->GetClass()->FindPropertyByName(TEXT("bApplyPassiveEffects"));
        if (PassiveProp) {
          TSharedPtr<FJsonValue> BoolVal = MakeShared<FJsonValueBoolean>(GetPayloadBool(Payload, TEXT("passiveEffects"), true));
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, PassiveProp, BoolVal, ApplyError);
        }
      }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("statModifiersConfigured"), GetPayloadBool(Payload, TEXT("statModifiers"), true));
    Result->SetBoolField(TEXT("abilityGrantsConfigured"), GetPayloadBool(Payload, TEXT("abilityGrants"), true));
    Result->SetBoolField(TEXT("passiveEffectsConfigured"), GetPayloadBool(Payload, TEXT("passiveEffects"), true));
    Result->SetArrayField(TEXT("variablesAdded"), AddedVars);
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Equipment effects configured"), Result);
    return true;
  }

  return false;
}
