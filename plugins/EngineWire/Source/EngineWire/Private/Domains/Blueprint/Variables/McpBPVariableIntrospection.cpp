#include "Domains/Blueprint/McpBPActionContext.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "EdGraphSchema_K2.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"

namespace McpBlueprintHandlers {
static FProperty *
FEngineWire_FindProperty(UBlueprint *Blueprint,
                                  const FString &PropertyName) {
  if (!Blueprint || PropertyName.TrimStartAndEnd().IsEmpty()) {
    return nullptr;
  }

  const FName PropFName(*PropertyName.TrimStartAndEnd());
  const TArray<UClass *> CandidateClasses = {Blueprint->GeneratedClass,
                                             Blueprint->SkeletonGeneratedClass,
                                             Blueprint->ParentClass};

  for (UClass *Candidate : CandidateClasses) {
    if (!Candidate) {
      continue;
    }

    if (FProperty *Found = Candidate->FindPropertyByName(PropFName)) {
      return Found;
    }
  }

  // SCS component properties are stored with "_GEN_VARIABLE" suffix
  const FString GenVariableName = PropertyName + TEXT("_GEN_VARIABLE");
  const FName GenVarFName(*GenVariableName);
  for (UClass *Candidate : CandidateClasses) {
    if (!Candidate) {
      continue;
    }
    if (FProperty *Found = Candidate->FindPropertyByName(GenVarFName)) {
      return Found;
    }
  }

  return nullptr;
}

static bool FEngineWire_CollectVariableMetadata(
    const UBlueprint *Blueprint, const FBPVariableDescription &VarDesc,
    TSharedPtr<FJsonObject> &OutMetadata) {
  OutMetadata.Reset();

  if (Blueprint) {
    TSharedPtr<FJsonObject> MetaJson = McpHandlerUtils::CreateResultObject();
    bool bAny = false;
    UBlueprint *MutableBlueprint = const_cast<UBlueprint *>(Blueprint);
    if (FProperty *Property = FEngineWire_FindProperty(
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

  return false;
}

TSharedPtr<FJsonObject>
FEngineWire_BuildVariableJson(const UBlueprint *Blueprint,
                                       const FBPVariableDescription &VarDesc) {
  TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
  Obj->SetStringField(TEXT("name"), VarDesc.VarName.ToString());
  Obj->SetStringField(TEXT("type"),
                      McpBlueprintUtils::DescribePinType(VarDesc.VarType));
  Obj->SetBoolField(TEXT("replicated"), (VarDesc.PropertyFlags & CPF_Net) != 0);
  // Public = the editor's eye toggle (Instance Editable), which add_variable's
  // isPublic sets; BlueprintReadOnly is a different flag.
  Obj->SetBoolField(TEXT("public"),
                    (VarDesc.PropertyFlags & CPF_DisableEditOnInstance) == 0);
  // Expose to Cinematics (CPF_Interp), which add_variable's exposeToCinematics sets.
  Obj->SetBoolField(TEXT("exposeToCinematics"), (VarDesc.PropertyFlags & CPF_Interp) != 0);
  const FString CategoryStr =
      VarDesc.Category.IsEmpty() ? FString() : VarDesc.Category.ToString();
  if (!CategoryStr.IsEmpty()) {
    Obj->SetStringField(TEXT("category"), CategoryStr);
  }
  TSharedPtr<FJsonObject> Metadata;
  if (FEngineWire_CollectVariableMetadata(Blueprint, VarDesc,
                                                   Metadata)) {
    // The editor keeps DisplayName and Category on every variable, copies of the name and category beside them.
    FString Value;
    if (Metadata->TryGetStringField(TEXT("DisplayName"), Value) && Value == VarDesc.VarName.ToString()) {
      Metadata->RemoveField(TEXT("DisplayName"));
    }
    if (Metadata->TryGetStringField(TEXT("Category"), Value) && Value == CategoryStr) {
      Metadata->RemoveField(TEXT("Category"));
    }
    if (Metadata->Values.Num() > 0) {
      Obj->SetObjectField(TEXT("metadata"), Metadata);
    }
  }
  return Obj;
}

TSharedPtr<FJsonObject> FEngineWire_CollectBlueprintDefaults(
    UBlueprint *Blueprint, const TArray<TSharedPtr<FJsonValue>> &Variables) {
  TSharedPtr<FJsonObject> Defaults = MakeShared<FJsonObject>();
  if (!Blueprint) {
    return Defaults;
  }

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
        FEngineWire_FindProperty(Blueprint, VariableName);
    // FindProperty also searches the skeleton class (SKEL_*_C). Until the
    // blueprint is recompiled a freshly added variable exists only there, and
    // reading it through the generated CDO asserts ("'Default__X_C' is of
    // class 'X_C' however property belongs to class 'SKEL_X_C'"). Only use a
    // property the CDO actually owns; otherwise use the authored default.
    if (Property && GeneratedCDO) {
      UClass *OwnerClass = Property->GetOwner<UClass>();
      if (!OwnerClass || !GeneratedCDO->IsA(OwnerClass)) {
        Property = GeneratedClass
                       ? GeneratedClass->FindPropertyByName(FName(*VariableName))
                       : nullptr;
      }
    }
    if (Property && GeneratedCDO) {
      if (void *PropertyAddress =
              Property->ContainerPtrToValuePtr<void>(GeneratedCDO)) {
        // A bool default is a JSON boolean, not "True"/"False" export text: the
        // caller compares it with the true/false it set, and receipt redaction
        // spares a boolean, where it masked a "bSecret" string default.
        if (const FBoolProperty *BoolProperty = CastField<FBoolProperty>(Property)) {
          Defaults->SetBoolField(VariableName, BoolProperty->GetPropertyValue(PropertyAddress));
          continue;
        }
        FString ExportedDefault;
        MCP_PROPERTY_EXPORT_TEXT(Property, ExportedDefault, PropertyAddress, nullptr, GeneratedCDO, PPF_SerializedAsImportText);
        Defaults->SetStringField(VariableName, ExportedDefault);
        continue;
      }
    }

    UBlueprint *DeclaringBlueprint = nullptr;
    const int32 NewVarIndex = FBlueprintEditorUtils::FindNewVariableIndexAndBlueprint(
        Blueprint, FName(*VariableName), DeclaringBlueprint);
    if (DeclaringBlueprint && NewVarIndex != INDEX_NONE &&
        DeclaringBlueprint->NewVariables.IsValidIndex(NewVarIndex)) {
      const FBPVariableDescription &Declared =
          DeclaringBlueprint->NewVariables[NewVarIndex];
      if (Declared.VarType.PinCategory == UEdGraphSchema_K2::PC_Boolean &&
          !Declared.VarType.IsContainer()) {
        Defaults->SetBoolField(VariableName, Declared.DefaultValue.ToBool());
      } else {
        Defaults->SetStringField(VariableName, Declared.DefaultValue);
      }
    }
  }

  return Defaults;
}
} // namespace McpBlueprintHandlers
