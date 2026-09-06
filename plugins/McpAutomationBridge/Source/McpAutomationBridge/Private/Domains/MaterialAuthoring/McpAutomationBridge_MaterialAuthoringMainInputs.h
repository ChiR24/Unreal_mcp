#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"

#if WITH_EDITORONLY_DATA
// Visits the eleven main material inputs as (PinName, FExpressionInput&).
template <typename TVisitor>
inline void ForEachMainMaterialInput(UMaterial* Material, TVisitor&& Visit)
{
  Visit(TEXT("BaseColor"), MCP_GET_MATERIAL_INPUT(Material, BaseColor));
  Visit(TEXT("EmissiveColor"), MCP_GET_MATERIAL_INPUT(Material, EmissiveColor));
  Visit(TEXT("Roughness"), MCP_GET_MATERIAL_INPUT(Material, Roughness));
  Visit(TEXT("Metallic"), MCP_GET_MATERIAL_INPUT(Material, Metallic));
  Visit(TEXT("Specular"), MCP_GET_MATERIAL_INPUT(Material, Specular));
  Visit(TEXT("Normal"), MCP_GET_MATERIAL_INPUT(Material, Normal));
  Visit(TEXT("Opacity"), MCP_GET_MATERIAL_INPUT(Material, Opacity));
  Visit(TEXT("OpacityMask"), MCP_GET_MATERIAL_INPUT(Material, OpacityMask));
  Visit(TEXT("AmbientOcclusion"), MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion));
  Visit(TEXT("SubsurfaceColor"), MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor));
  Visit(TEXT("WorldPositionOffset"), MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset));
}
#endif

// Main material input by pin name; nullptr when the name is not a main pin.
inline FExpressionInput* GetMainMaterialInput(UMaterial* Material, const FString& PinName)
{
  FExpressionInput* Found = nullptr;
#if WITH_EDITORONLY_DATA
  if (Material) {
    ForEachMainMaterialInput(Material, [&](const TCHAR* Name, FExpressionInput& Input) {
      if (!Found && PinName == Name) { Found = &Input; }
    });
  }
#endif
  return Found;
}
