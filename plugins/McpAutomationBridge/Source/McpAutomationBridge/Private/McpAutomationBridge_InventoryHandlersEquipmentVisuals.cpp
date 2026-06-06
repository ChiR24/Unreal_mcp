#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_InventoryHandlersShared.h"

bool HandleInventoryEquipmentVisualActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (SubAction == TEXT("configure_equipment_visuals")) {
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

    bool bAttachToSocket = GetPayloadBool(Payload, TEXT("attachToSocket"), true);
    FString DefaultSocket = GetPayloadString(Payload, TEXT("defaultSocket"), TEXT("hand_r"));

    // Add equipment visual configuration variables
    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    FEdGraphPinType NameType;
    NameType.PinCategory = UEdGraphSchema_K2::PC_Name;

    FEdGraphPinType NameArrayType;
    NameArrayType.PinCategory = UEdGraphSchema_K2::PC_Name;
    NameArrayType.ContainerType = EPinContainerType::Array;

    FEdGraphPinType SoftObjectType;
    SoftObjectType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;

    FEdGraphPinType TransformType;
    TransformType.PinCategory = UEdGraphSchema_K2::PC_Struct;
    TransformType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();

    // Visual configuration variables
    TArray<TPair<FName, FEdGraphPinType>> VisualVars = {
      TPair<FName, FEdGraphPinType>(TEXT("bAttachToSocket"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("DefaultAttachSocket"), NameType),
      TPair<FName, FEdGraphPinType>(TEXT("EquipmentSockets"), NameArrayType),
      TPair<FName, FEdGraphPinType>(TEXT("EquipmentMesh"), SoftObjectType),
      TPair<FName, FEdGraphPinType>(TEXT("AttachmentOffset"), TransformType),
      TPair<FName, FEdGraphPinType>(TEXT("bUseCustomAttachRules"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("bHideEquippedMesh"), BoolType)
    };

    TArray<TSharedPtr<FJsonValue>> AddedVars;

    for (const auto& VarPair : VisualVars) {
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
        FProperty* AttachProp = CDO->GetClass()->FindPropertyByName(TEXT("bAttachToSocket"));
        if (AttachProp) {
          TSharedPtr<FJsonValue> BoolVal = MakeShared<FJsonValueBoolean>(bAttachToSocket);
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, AttachProp, BoolVal, ApplyError);
        }

        FProperty* SocketProp = CDO->GetClass()->FindPropertyByName(TEXT("DefaultAttachSocket"));
        if (SocketProp) {
          TSharedPtr<FJsonValue> NameVal = MakeShared<FJsonValueString>(DefaultSocket);
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, SocketProp, NameVal, ApplyError);
        }
      }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("attachToSocket"), bAttachToSocket);
    Result->SetStringField(TEXT("defaultSocket"), DefaultSocket);
    Result->SetBoolField(TEXT("visualsConfigured"), true);
    Result->SetArrayField(TEXT("variablesAdded"), AddedVars);
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Equipment visuals configured"), Result);
    return true;
  }

  // ===========================================================================
  // 17.5 Loot System (4 actions)
  // ===========================================================================

  return false;
}
