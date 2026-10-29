// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"

class UMaterialInstanceConstant;

// The small material assets set_material (color, glow) and set_light_flicker make once under a fixed folder, save,
// and find again by name on every later call.

/** An expression added to a material's graph at an editor position, transactional as the material editor adds one. */
template <typename T>
T* McpAddMaterialExpression(UMaterial* Material, int32 X, int32 Y)
{
    T* Expression = NewObject<T>(Material, NAME_None, RF_Transactional);
    Expression->MaterialExpressionEditorX = X;
    Expression->MaterialExpressionEditorY = Y;
    MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(Expression);
    return Expression;
}

/** The material or instance Folder + Name, already loaded or loaded from disk; null when there is none. */
UMaterialInterface* McpFindMaterialAsset(const TCHAR* Folder, const FString& Name);

/** A new, empty material asset Folder + Name; null when it cannot be made. */
UMaterial* McpNewMaterial(const TCHAR* Folder, const FString& Name);

/** A new instance of Parent as the asset Folder + Name, its parameters left to the caller; null when it cannot be made. */
UMaterialInstanceConstant* McpNewMaterialInstance(UMaterialInterface* Parent, const TCHAR* Folder, const FString& Name);

/** The last step of making one: PostEditChange, marked dirty, announced to the asset registry and saved. */
void McpSaveNewMaterial(UMaterialInterface* Material);
