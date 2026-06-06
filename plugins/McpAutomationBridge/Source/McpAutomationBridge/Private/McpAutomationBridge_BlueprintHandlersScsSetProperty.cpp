#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
bool HandleScsSetProperty(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_SCS_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("set_scs_property"))) {
    UBlueprint *Blueprint = ResolveBlueprint();
    if (!Blueprint) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("set_scs_property requires a valid blueprint"), nullptr,
          TEXT("INVALID_BLUEPRINT"));
      return true;
    }

    FString ComponentName;
    FString PropertyName;
    FString PropertyValue;
    Payload->TryGetStringField(TEXT("componentName"), ComponentName);
    Payload->TryGetStringField(TEXT("propertyName"), PropertyName);
    Payload->TryGetStringField(TEXT("value"), PropertyValue);

    if (ComponentName.IsEmpty() || PropertyName.IsEmpty()) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("set_scs_property requires componentName, "
                                  "propertyName, and value"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Find the SCS node for this component
    USCS_Node *FoundNode = nullptr;
    if (Blueprint->SimpleConstructionScript) {
      const TArray<USCS_Node *> &Nodes =
          Blueprint->SimpleConstructionScript->GetAllNodes();
      for (USCS_Node *Node : Nodes) {
        if (Node && Node->GetVariableName().IsValid() &&
            Node->GetVariableName().ToString() == ComponentName) {
          FoundNode = Node;
          break;
        }
      }
    }

    if (!FoundNode) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Component '%s' not found in SCS"),
                          *ComponentName),
          nullptr, TEXT("COMPONENT_NOT_FOUND"));
      return true;
    }

    // Get the component template (CDO) to access properties
    UObject *ComponentTemplate = FoundNode->ComponentTemplate;
    if (!ComponentTemplate) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Component template not found for '%s'"),
                          *ComponentName),
          nullptr, TEXT("TEMPLATE_NOT_FOUND"));
      return true;
    }

    // Find the property on the component class
    FProperty *FoundProperty =
        ComponentTemplate->GetClass()->FindPropertyByName(FName(*PropertyName));
    if (!FoundProperty) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Property '%s' not found on component '%s'"),
                          *PropertyName, *ComponentName),
          nullptr, TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }

    // Set the property value based on type
    bool bSuccess = false;
    FString ErrorMessage;

    if (FStrProperty *StrProp = CastField<FStrProperty>(FoundProperty)) {
      void *PropAddr = StrProp->ContainerPtrToValuePtr<void>(ComponentTemplate);
      StrProp->SetPropertyValue(PropAddr, PropertyValue);
      bSuccess = true;
    } else if (FFloatProperty *FloatProp =
                   CastField<FFloatProperty>(FoundProperty)) {
      void *PropAddr =
          FloatProp->ContainerPtrToValuePtr<void>(ComponentTemplate);
      float Value = FCString::Atof(*PropertyValue);
      FloatProp->SetPropertyValue(PropAddr, Value);
      bSuccess = true;
    } else if (FDoubleProperty *DoubleProp =
                   CastField<FDoubleProperty>(FoundProperty)) {
      void *PropAddr =
          DoubleProp->ContainerPtrToValuePtr<void>(ComponentTemplate);
      double Value = FCString::Atod(*PropertyValue);
      DoubleProp->SetPropertyValue(PropAddr, Value);
      bSuccess = true;
    } else if (FIntProperty *IntProp = CastField<FIntProperty>(FoundProperty)) {
      void *PropAddr = IntProp->ContainerPtrToValuePtr<void>(ComponentTemplate);
      int32 Value = FCString::Atoi(*PropertyValue);
      IntProp->SetPropertyValue(PropAddr, Value);
      bSuccess = true;
    } else if (FInt64Property *Int64Prop =
                   CastField<FInt64Property>(FoundProperty)) {
      void *PropAddr =
          Int64Prop->ContainerPtrToValuePtr<void>(ComponentTemplate);
      int64 Value = FCString::Atoi64(*PropertyValue);
      Int64Prop->SetPropertyValue(PropAddr, Value);
      bSuccess = true;
    } else if (FBoolProperty *BoolProp =
                   CastField<FBoolProperty>(FoundProperty)) {
      void *PropAddr =
          BoolProp->ContainerPtrToValuePtr<void>(ComponentTemplate);
      bool Value = PropertyValue.ToBool();
      BoolProp->SetPropertyValue(PropAddr, Value);
      bSuccess = true;
    } else if (FObjectProperty *ObjProp =
                   CastField<FObjectProperty>(FoundProperty)) {
      // Try to find the object by path
      UObject *ObjValue = FindObject<UObject>(nullptr, *PropertyValue);
      if (ObjValue || PropertyValue.IsEmpty()) {
        void *PropAddr =
            ObjProp->ContainerPtrToValuePtr<void>(ComponentTemplate);
        ObjProp->SetPropertyValue(PropAddr, ObjValue);
        bSuccess = true;
      } else {
        ErrorMessage = FString::Printf(
            TEXT("Object property requires valid object path, got: %s"),
            *PropertyValue);
      }
    } else if (FStructProperty *StructProp =
                   CastField<FStructProperty>(FoundProperty)) {
      // Handle struct properties (FVector, FVector2D, FLinearColor, etc.)
      void *PropAddr =
          StructProp->ContainerPtrToValuePtr<void>(ComponentTemplate);
      FString StructName =
          StructProp->Struct ? StructProp->Struct->GetName() : FString();

      // Try to parse JSON object value from payload
      const TSharedPtr<FJsonObject> *JsonObjValue = nullptr;
      if (Payload->TryGetObjectField(TEXT("value"), JsonObjValue) &&
          JsonObjValue->IsValid()) {
        // Handle FVector explicitly
        if (StructName.Equals(TEXT("Vector"), ESearchCase::IgnoreCase)) {
          FVector *Vec = static_cast<FVector *>(PropAddr);
          double X = 0, Y = 0, Z = 0;
          (*JsonObjValue)->TryGetNumberField(TEXT("X"), X);
          (*JsonObjValue)->TryGetNumberField(TEXT("Y"), Y);
          (*JsonObjValue)->TryGetNumberField(TEXT("Z"), Z);
          // Also try lowercase
          if (X == 0 && Y == 0 && Z == 0) {
            (*JsonObjValue)->TryGetNumberField(TEXT("x"), X);
            (*JsonObjValue)->TryGetNumberField(TEXT("y"), Y);
            (*JsonObjValue)->TryGetNumberField(TEXT("z"), Z);
          }
          *Vec = FVector(X, Y, Z);
          bSuccess = true;
        }
        // Handle FVector2D
        else if (StructName.Equals(TEXT("Vector2D"), ESearchCase::IgnoreCase)) {
          FVector2D *Vec = static_cast<FVector2D *>(PropAddr);
          double X = 0, Y = 0;
          (*JsonObjValue)->TryGetNumberField(TEXT("X"), X);
          (*JsonObjValue)->TryGetNumberField(TEXT("Y"), Y);
          if (X == 0 && Y == 0) {
            (*JsonObjValue)->TryGetNumberField(TEXT("x"), X);
            (*JsonObjValue)->TryGetNumberField(TEXT("y"), Y);
          }
          *Vec = FVector2D(X, Y);
          bSuccess = true;
        }
        // Handle FLinearColor
        else if (StructName.Equals(TEXT("LinearColor"),
                                   ESearchCase::IgnoreCase)) {
          FLinearColor *Color = static_cast<FLinearColor *>(PropAddr);
          double R = 0, G = 0, B = 0, A = 1;
          (*JsonObjValue)->TryGetNumberField(TEXT("R"), R);
          (*JsonObjValue)->TryGetNumberField(TEXT("G"), G);
          (*JsonObjValue)->TryGetNumberField(TEXT("B"), B);
          (*JsonObjValue)->TryGetNumberField(TEXT("A"), A);
          if (R == 0 && G == 0 && B == 0) {
            (*JsonObjValue)->TryGetNumberField(TEXT("r"), R);
            (*JsonObjValue)->TryGetNumberField(TEXT("g"), G);
            (*JsonObjValue)->TryGetNumberField(TEXT("b"), B);
            (*JsonObjValue)->TryGetNumberField(TEXT("a"), A);
          }
          *Color = FLinearColor(R, G, B, A);
          bSuccess = true;
        }
        // Handle FRotator
        else if (StructName.Equals(TEXT("Rotator"), ESearchCase::IgnoreCase)) {
          FRotator *Rot = static_cast<FRotator *>(PropAddr);
          double Pitch = 0, Yaw = 0, Roll = 0;
          (*JsonObjValue)->TryGetNumberField(TEXT("Pitch"), Pitch);
          (*JsonObjValue)->TryGetNumberField(TEXT("Yaw"), Yaw);
          (*JsonObjValue)->TryGetNumberField(TEXT("Roll"), Roll);
          if (Pitch == 0 && Yaw == 0 && Roll == 0) {
            (*JsonObjValue)->TryGetNumberField(TEXT("pitch"), Pitch);
            (*JsonObjValue)->TryGetNumberField(TEXT("yaw"), Yaw);
            (*JsonObjValue)->TryGetNumberField(TEXT("roll"), Roll);
          }
          *Rot = FRotator(Pitch, Yaw, Roll);
          bSuccess = true;
        }
      }

      // Fallback: try ImportText for string representation
      if (!bSuccess && !PropertyValue.IsEmpty() && StructProp->Struct) {
        const TCHAR *Buffer = *PropertyValue;
        // Use UScriptStruct::ImportText (not FStructProperty)
        const TCHAR *Result = StructProp->Struct->ImportText(
            Buffer, PropAddr, nullptr, PPF_None, GLog, StructName);
        bSuccess = (Result != nullptr);
        if (!bSuccess) {
          ErrorMessage = FString::Printf(
              TEXT("Failed to parse struct value '%s' for property '%s' of "
                   "type '%s'. For FVector use {\"X\":val,\"Y\":val,\"Z\":val} "
                   "or string \"(X=val,Y=val,Z=val)\""),
              *PropertyValue, *PropertyName, *StructName);
        }
      }

      if (!bSuccess && ErrorMessage.IsEmpty()) {
        ErrorMessage = FString::Printf(
            TEXT("Struct property '%s' of type '%s' requires JSON object "
                 "value like {\"X\":val,\"Y\":val,\"Z\":val}"),
            *PropertyName, *StructName);
      }
    } else {
      ErrorMessage =
          FString::Printf(TEXT("Property type '%s' not supported for setting"),
                          *FoundProperty->GetClass()->GetName());
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("componentName"), ComponentName);
    Result->SetStringField(TEXT("propertyName"), PropertyName);
    Result->SetStringField(TEXT("value"), PropertyValue);

    if (bSuccess) {
      // Compile and save the blueprint
      bool bCompiled = false;
      bool bSaved = false;
      FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
      bCompiled = McpSafeCompileBlueprint(Blueprint);

      bSaved = SaveLoadedAssetThrottled(Blueprint);

      Result->SetBoolField(TEXT("compiled"), bCompiled);
      Result->SetBoolField(TEXT("saved"), bSaved);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("SCS property set successfully"), Result,
                             FString());
    } else {
      Result->SetStringField(TEXT("error"), ErrorMessage);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Failed to set SCS property"), Result,
                             TEXT("PROPERTY_SET_FAILED"));
    }
    return true;
  }

  // Unknown blueprint action - send explicit error instead of returning false
  return false;
}
#endif
} // namespace McpBlueprintHandlers
