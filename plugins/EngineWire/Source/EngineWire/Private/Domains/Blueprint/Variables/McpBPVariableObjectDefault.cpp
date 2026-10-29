#include "Domains/Blueprint/Variables/McpBPVariableObjectDefault.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Property/McpPropCdoPropagation.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPCompileHelpers.h"
#include "Foundation/BridgeHelpers/Properties/McpPropApply.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"

#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"

namespace McpBlueprintHandlers {
namespace {
// The followers (placed copies and derived defaults that still held the old default, collected before the write) take
// the new default from the class default object, as the editor's own default edit carries them. Left alone, a copy that
// never changed it read as an override once the default under it moved, and the compile kept the old value on it: a
// behaviour run again with once on left a placed sound trigger playing every time. Copied, not re-imported: a default
// can arrive as export text ("(X=0,Y=0,Z=1200)").
// ponytail: a behaviour rolled back after this (its compile broke, its key mapping failed) restores the Blueprint but
// leaves the followers on the new value; snapshot them with the Blueprint if that ever bites.
void FollowDefault(UObject *CDO, const FProperty *Property, const TArray<UObject *> &Followers) {
  for (UObject *Follower : Followers) {
    Follower->Modify();
    Property->CopyCompleteValue_InContainer(Follower, CDO);
    Follower->PostEditChange();
  }
}
} // namespace

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
  const TArray<UObject *> Followers = McpPropertyCdoPropagation::CollectFollowers(CDO, VarName.ToString());
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
    FollowDefault(CDO, Property, Followers);
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
  // Before the compile: it replaces the placed actors, and the followers would be the ones it threw away.
  FollowDefault(CDO, Property, Followers);
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
  McpSafeCompileBlueprint(Blueprint);
  return true;
}
} // namespace McpBlueprintHandlers
