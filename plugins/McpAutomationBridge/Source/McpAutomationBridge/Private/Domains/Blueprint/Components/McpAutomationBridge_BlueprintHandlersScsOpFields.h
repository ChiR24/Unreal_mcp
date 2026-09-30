#pragma once

#include "CoreMinimal.h"

#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"

// What an edit_scs operation names, under the spellings the single-op actions take
// (add_scs_component reads meshPath and mesh_path, materialPath and material_path). One reading
// for everything that asks what an operation is or carries, so the callers cannot disagree about
// a spelling again.
namespace McpBlueprintHandlers
{
inline FString ScsOpMeshPath(const TSharedPtr<FJsonObject> &Op)
{
  return McpGetFirstStringField(Op, {TEXT("meshPath"), TEXT("mesh_path"), TEXT("staticMesh")});
}

inline FString ScsOpMaterialPath(const TSharedPtr<FJsonObject> &Op)
{
  return McpGetFirstStringField(Op, {TEXT("materialPath"), TEXT("material_path")});
}

/**
 * True when Op carries anything for the modify path to do to a component: a placement, a property
 * bag, a mesh or a material. A JSON null is nothing. An add asks this before it hands the
 * component it made on; it asked only for the first two, so an add given just a mesh answered
 * success with an empty component.
 */
inline bool ScsOpHasEdits(const TSharedPtr<FJsonObject> &Op)
{
  for (const TCHAR *Key : {TEXT("transform"), TEXT("properties")})
  {
    const TSharedPtr<FJsonValue> Value = Op->TryGetField(Key);
    if (Value.IsValid() && !Value->IsNull())
      return true;
  }
  return !ScsOpMeshPath(Op).IsEmpty() || !ScsOpMaterialPath(Op).IsEmpty();
}
}
