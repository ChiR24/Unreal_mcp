#include "Foundation/BridgeHelpers/Blueprints/McpBPAssetLoad.h"

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/World.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsActionsPaths.h"

bool McpResolveLevelBlueprint(const FString &PackagePath, const TArray<FAssetData> &Assets,
                              UBlueprint *&OutBlueprint, FString &OutError) {
  OutBlueprint = nullptr;
  // Only the editor world's levels count: a world that is merely still in memory (closed, or loaded by
  // something else) is not what the caller sees, and an edit to its Blueprint would be lost with it.
  if (UWorld *EditorWorld = McpHandlerUtils::GetEditorWorld()) {
    for (ULevel *Level : EditorWorld->GetLevels()) {
      if (!Level || !Level->GetOutermost()->GetName().Equals(PackagePath, ESearchCase::IgnoreCase)) {
        continue;
      }
      OutBlueprint = Level->GetLevelScriptBlueprint(/*bDontCreate=*/true);
      if (!OutBlueprint) {
        OutError = FString::Printf(TEXT("Level %s has no level Blueprint yet; manage_level_structure edit_level_blueprint (edit add_node or open) creates it."), *PackagePath);
      }
      return true;
    }
  }
  if (Assets.Num() > 0 && Assets[0].GetClass() == UWorld::StaticClass()) {
    OutError = FString::Printf(TEXT("%s is a level that is not open in the editor; load it (manage_level load), then this path reaches its level Blueprint."), *PackagePath);
    return true;
  }
  return false;
}

FString McpBlueprintVariableDefault(const UBlueprint *Blueprint, const FBPVariableDescription &Variable) {
  UClass *GeneratedClass = Blueprint->GeneratedClass;
  UObject *CDO = GeneratedClass ? GeneratedClass->GetDefaultObject() : nullptr;
  FProperty *Property = GeneratedClass ? GeneratedClass->FindPropertyByName(Variable.VarName) : nullptr;
  if (!Property || !CDO) {
    return Variable.DefaultValue;
  }
  FString Text;
  MCP_PROPERTY_EXPORT_TEXT(Property, Text, Property->ContainerPtrToValuePtr<void>(CDO), nullptr, CDO, PPF_None);
  return Text;
}
