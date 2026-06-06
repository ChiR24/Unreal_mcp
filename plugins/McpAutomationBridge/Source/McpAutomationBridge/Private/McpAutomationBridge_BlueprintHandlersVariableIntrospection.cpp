#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
FString
FMcpAutomationBridge_DescribePinType(const FEdGraphPinType &PinType) {
  // Delegate to centralized McpBlueprintUtils
  return McpBlueprintUtils::DescribePinType(PinType);
}

void FMcpAutomationBridge_AppendPinsJson(
    const TArray<TSharedPtr<FUserPinInfo>> &Pins,
    TArray<TSharedPtr<FJsonValue>> &Out) {
  for (const TSharedPtr<FUserPinInfo> &PinInfo : Pins) {
    if (!PinInfo.IsValid()) {
      continue;
    }
    const FString PinName = PinInfo->PinName.ToString();
    if (PinName.IsEmpty()) {
      continue;
    }
    TSharedPtr<FJsonObject> PinJson = McpHandlerUtils::CreateResultObject();
    PinJson->SetStringField(TEXT("name"), PinName);
    PinJson->SetStringField(
        TEXT("type"), FMcpAutomationBridge_DescribePinType(PinInfo->PinType));
    Out.Add(MakeShared<FJsonValueObject>(PinJson));
  }
}

bool FMcpAutomationBridge_CollectVariableMetadata(
    const UBlueprint *Blueprint, const FBPVariableDescription &VarDesc,
    TSharedPtr<FJsonObject> &OutMetadata) {
  OutMetadata.Reset();

#if WITH_EDITOR
  if (Blueprint) {
    TSharedPtr<FJsonObject> MetaJson = McpHandlerUtils::CreateResultObject();
    bool bAny = false;
    UBlueprint *MutableBlueprint = const_cast<UBlueprint *>(Blueprint);
    if (FProperty *Property = FMcpAutomationBridge_FindProperty(
            MutableBlueprint, VarDesc.VarName.ToString())) {
      if (const TMap<FName, FString> *MetaMap = Property->GetMetaDataMap()) {
        for (const TPair<FName, FString> &Pair : *MetaMap) {
          if (!Pair.Value.IsEmpty()) {
            MetaJson->SetStringField(Pair.Key.ToString(), Pair.Value);
            bAny = true;
          }
        }
      }
    }
    if (bAny && MetaJson->Values.Num() > 0) {
      OutMetadata = MetaJson;
      return true;
    }
  }
#endif

  return false;
}

TSharedPtr<FJsonObject>
FMcpAutomationBridge_BuildVariableJson(const UBlueprint *Blueprint,
                                       const FBPVariableDescription &VarDesc);

FString
FMcpAutomationBridge_DescribePropertyType(const FProperty *Property) {
  if (!Property) {
    return FString();
  }

#if WITH_EDITOR && MCP_HAS_EDGRAPH_SCHEMA_K2
  // Convert property to pin type for Blueprint-style type string
  FEdGraphPinType PinType;
  if (const UEdGraphSchema_K2 *Schema = GetDefault<UEdGraphSchema_K2>()) {
    if (Schema->ConvertPropertyToPinType(Property, PinType)) {
      return FMcpAutomationBridge_DescribePinType(PinType);
    }
  }
#endif

  // Fallback to C++ style if conversion fails
  FString ExtendedType;
  const FString BaseType = Property->GetCPPType(&ExtendedType);
  return ExtendedType.IsEmpty() ? BaseType : BaseType + ExtendedType;
}

void FMcpAutomationBridge_AnnotateVariableJson(
    const TSharedPtr<FJsonObject> &Obj, const UBlueprint *RequestedBlueprint,
    const UBlueprint *DeclaringBlueprint, bool bIsSCSVariable) {
  if (!Obj.IsValid()) {
    return;
  }

  // Mark as inherited if: RequestedBlueprint is valid AND
  // (DeclaringBlueprint is null = native parent, OR different blueprint)
  Obj->SetBoolField(TEXT("inherited"),
      RequestedBlueprint && (DeclaringBlueprint ? RequestedBlueprint != DeclaringBlueprint : true));
  if (DeclaringBlueprint) {
    Obj->SetStringField(TEXT("declaredInBlueprintPath"),
                        DeclaringBlueprint->GetPathName());
  }
  if (bIsSCSVariable) {
    Obj->SetBoolField(TEXT("component"), true);
  }
}

TSharedPtr<FJsonObject>
FMcpAutomationBridge_BuildVariableJson(const UBlueprint *Blueprint,
                                       const FBPVariableDescription &VarDesc) {
  TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
  Obj->SetStringField(TEXT("name"), VarDesc.VarName.ToString());
  Obj->SetStringField(TEXT("type"),
                      FMcpAutomationBridge_DescribePinType(VarDesc.VarType));
  Obj->SetBoolField(TEXT("replicated"), (VarDesc.PropertyFlags & CPF_Net) != 0);
  Obj->SetBoolField(TEXT("public"),
                    (VarDesc.PropertyFlags & CPF_BlueprintReadOnly) == 0);
  const FString CategoryStr =
      VarDesc.Category.IsEmpty() ? FString() : VarDesc.Category.ToString();
  if (!CategoryStr.IsEmpty()) {
    Obj->SetStringField(TEXT("category"), CategoryStr);
  }
  TSharedPtr<FJsonObject> Metadata;
  if (FMcpAutomationBridge_CollectVariableMetadata(Blueprint, VarDesc,
                                                   Metadata)) {
    Obj->SetObjectField(TEXT("metadata"), Metadata);
  }
  return Obj;
}

TArray<TSharedPtr<FJsonValue>>
FMcpAutomationBridge_CollectBlueprintVariables(UBlueprint *Blueprint) {
  // Delegate to centralized McpBlueprintUtils
  return McpBlueprintUtils::CollectBlueprintVariables(Blueprint);
}

TSharedPtr<FJsonObject> FMcpAutomationBridge_CollectBlueprintDefaults(
    UBlueprint *Blueprint, const TArray<TSharedPtr<FJsonValue>> &Variables) {
  TSharedPtr<FJsonObject> Defaults = MakeShared<FJsonObject>();
  if (!Blueprint) {
    return Defaults;
  }

#if WITH_EDITOR
  UClass *GeneratedClass = Blueprint->GeneratedClass;
  UObject *GeneratedCDO = GeneratedClass ? GeneratedClass->GetDefaultObject() : nullptr;

  for (const TSharedPtr<FJsonValue> &VariableValue : Variables) {
    if (!VariableValue.IsValid() || VariableValue->Type != EJson::Object) {
      continue;
    }

    const TSharedPtr<FJsonObject> VariableObj = VariableValue->AsObject();
    FString VariableName;
    if (!VariableObj.IsValid() ||
        !VariableObj->TryGetStringField(TEXT("name"), VariableName) ||
        VariableName.IsEmpty()) {
      continue;
    }

    FProperty *Property =
        FMcpAutomationBridge_FindProperty(Blueprint, VariableName);
    if (Property && GeneratedCDO) {
      if (void *PropertyAddress =
              Property->ContainerPtrToValuePtr<void>(GeneratedCDO)) {
        FString ExportedDefault;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        Property->ExportTextItem_Direct(ExportedDefault, PropertyAddress,
                                        nullptr, GeneratedCDO,
                                        PPF_SerializedAsImportText);
#else
        // UE 5.0: ExportTextItem is the virtual function
        Property->ExportTextItem(ExportedDefault, PropertyAddress,
                                        nullptr, GeneratedCDO,
                                        PPF_SerializedAsImportText);
#endif
        Defaults->SetStringField(VariableName, ExportedDefault);
        continue;
      }
    }

    UBlueprint *DeclaringBlueprint = nullptr;
    const int32 NewVarIndex = FBlueprintEditorUtils::FindNewVariableIndexAndBlueprint(
        Blueprint, FName(*VariableName), DeclaringBlueprint);
    if (DeclaringBlueprint && NewVarIndex != INDEX_NONE &&
        DeclaringBlueprint->NewVariables.IsValidIndex(NewVarIndex)) {
      Defaults->SetStringField(
          VariableName,
          DeclaringBlueprint->NewVariables[NewVarIndex].DefaultValue);
    }
  }
#endif

  return Defaults;
}
#endif
} // namespace McpBlueprintHandlers
