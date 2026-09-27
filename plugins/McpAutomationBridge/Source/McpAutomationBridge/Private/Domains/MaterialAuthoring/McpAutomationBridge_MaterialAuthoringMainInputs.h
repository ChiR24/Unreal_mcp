#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"

// Visits the main material inputs as (PinName, FExpressionInput&).
//
// Only eleven were listed here, which silently made whole classes of material
// unauthorable over the bridge: connect_nodes answers "Unknown input on main
// node" for anything missing, so a glass or heat-haze material could be built
// node-by-node and then never wired to Refraction. UMaterial exposes nineteen
// non-deprecated root inputs; the eight added below are the rest of the ones
// that carry an FExpressionInput (ShadingModelFromMaterialExpression is a
// different input type and stays out).
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
  // Present on UMaterial since 5.0.
  Visit(TEXT("Refraction"), MCP_GET_MATERIAL_INPUT(Material, Refraction));
  Visit(TEXT("Anisotropy"), MCP_GET_MATERIAL_INPUT(Material, Anisotropy));
  Visit(TEXT("Tangent"), MCP_GET_MATERIAL_INPUT(Material, Tangent));
  Visit(TEXT("PixelDepthOffset"), MCP_GET_MATERIAL_INPUT(Material, PixelDepthOffset));
  Visit(TEXT("ClearCoat"), MCP_GET_MATERIAL_INPUT(Material, ClearCoat));
  Visit(TEXT("ClearCoatRoughness"), MCP_GET_MATERIAL_INPUT(Material, ClearCoatRoughness));
  // Checked per release tag in Material.h: SurfaceThickness appears at 5.2.1
  // and Displacement at 5.3.2; neither exists in 5.0 or 5.1.
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
  Visit(TEXT("SurfaceThickness"), MCP_GET_MATERIAL_INPUT(Material, SurfaceThickness));
#endif
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
  Visit(TEXT("Displacement"), MCP_GET_MATERIAL_INPUT(Material, Displacement));
#endif
}

// Main material input by pin name; nullptr when the name is not a main pin.
inline FExpressionInput* GetMainMaterialInput(UMaterial* Material, const FString& PinName)
{
  FExpressionInput* Found = nullptr;
  if (Material) {
    ForEachMainMaterialInput(Material, [&](const TCHAR* Name, FExpressionInput& Input) {
      if (!Found && PinName == Name) { Found = &Input; }
    });
  }
  return Found;
}

// Visits every input of Expr as (FExpressionInput&, PinName): the reflected
// ExpressionInput properties by property name, then a Custom node's named
// inputs and a function call's inputs by their declared names.
template <typename TVisitor>
inline void ForEachExpressionInput(UMaterialExpression* Expr, TVisitor&& Visit)
{
  if (!Expr) return;
  for (TFieldIterator<FStructProperty> It(Expr->GetClass()); It; ++It) {
    if (It->Struct && It->Struct->GetFName() == FName(TEXT("ExpressionInput"))) {
      Visit(*It->ContainerPtrToValuePtr<FExpressionInput>(Expr), It->GetName());
    }
  }
  if (UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr)) {
    for (FCustomInput& Input : Custom->Inputs) { Visit(Input.Input, Input.InputName.ToString()); }
  }
  if (UMaterialExpressionMaterialFunctionCall* Call = Cast<UMaterialExpressionMaterialFunctionCall>(Expr)) {
    for (FFunctionExpressionInput& Input : Call->FunctionInputs) {
      Visit(Input.Input, Input.ExpressionInput ? Input.ExpressionInput->InputName.ToString() : FString());
    }
  }
}
