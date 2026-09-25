#include "Domains/Blueprint/Variables/McpAutomationBridge_BlueprintVariableObjectDefault.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersPropertyApply.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
bool McpApplyVariableObjectDefault(UBlueprint *Blueprint, FName VarName,
                                   const TSharedPtr<FJsonValue> &Value,
                                   FString &OutError) {
  UObject *CDO = Blueprint && Blueprint->GeneratedClass
                     ? Blueprint->GeneratedClass->GetDefaultObject()
                     : nullptr;
  FProperty *Property =
      CDO ? CDO->GetClass()->FindPropertyByName(VarName) : nullptr;
  if (!Property) {
    OutError = FString::Printf(
        TEXT("'%s' is not on the compiled class yet"), *VarName.ToString());
    return false;
  }
  CDO->Modify();
  if (!ApplyJsonValueToProperty(CDO, Property, Value, OutError)) {
    return false;
  }
  FString DefaultText;
  MCP_PROPERTY_EXPORT_TEXT(Property, DefaultText,
                           Property->ContainerPtrToValuePtr<void>(CDO), nullptr,
                           nullptr, PPF_None);
  for (FBPVariableDescription &Var : Blueprint->NewVariables) {
    if (Var.VarName == VarName) {
      Var.DefaultValue = DefaultText;
    }
  }
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
  McpSafeCompileBlueprint(Blueprint);
  return true;
}
#endif
} // namespace McpBlueprintHandlers
