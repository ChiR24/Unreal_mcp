#include "Domains/Blueprint/Variables/McpAutomationBridge_BlueprintVariableObjectDefault.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersPropertyApply.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"

#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"

namespace McpBlueprintHandlers {
bool McpApplyVariableDefault(UBlueprint *Blueprint, FName VarName,
                             const TSharedPtr<FJsonValue> &Value,
                             FString &OutError) {
  UObject *CDO = Blueprint && Blueprint->GeneratedClass
                     ? Blueprint->GeneratedClass->GetDefaultObject()
                     : nullptr;
  FProperty *Property =
      CDO ? CDO->GetClass()->FindPropertyByName(VarName) : nullptr;
  const int32 Index = Property ? FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VarName) : INDEX_NONE;
  if (Index == INDEX_NONE) {
    OutError = FString::Printf(
        TEXT("'%s' is not on the compiled class yet"), *VarName.ToString());
    return false;
  }
  FString Text;
  if (McpJsonScalarToString(Value, Text)) {
    // Empty text is the zero value, which the compiler skips as well.
    if (!Text.IsEmpty() &&
        !FBlueprintEditorUtils::PropertyValueFromString(Property, Text, reinterpret_cast<uint8 *>(CDO), CDO)) {
      OutError = FString::Printf(TEXT("'%s' is not a valid default for variable %s (%s)"), *Text,
                                 *VarName.ToString(), *Property->GetCPPType());
      return false;
    }
    Blueprint->NewVariables[Index].DefaultValue = Text;
    return true;
  }
  CDO->Modify();
  if (!ApplyJsonValueToProperty(CDO, Property, Value, OutError)) {
    return false;
  }
  FString DefaultText;
  MCP_PROPERTY_EXPORT_TEXT(Property, DefaultText,
                           Property->ContainerPtrToValuePtr<void>(CDO), nullptr,
                           nullptr, PPF_None);
  Blueprint->NewVariables[Index].DefaultValue = DefaultText;
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
  McpSafeCompileBlueprint(Blueprint);
  return true;
}
} // namespace McpBlueprintHandlers
