#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#if !MCP_HAS_MATERIAL_ATTRIBUTE_DEFINITION_MAP_HEADER
#include "MaterialShared.h"
#else
#include "Materials/MaterialAttributeDefinitionMap.h"
#endif

// The name a main pin answers to: the engine's attribute name, but ClearCoat and ClearCoatRoughness for the engine's
// CustomData0 and CustomData1, as the folder has always named them (CanonicalMainInputName takes the engine's names).
inline FString McpMainInputName(EMaterialProperty Property)
{
  return Property == MP_CustomData0 ? FString(TEXT("ClearCoat"))
       : Property == MP_CustomData1 ? FString(TEXT("ClearCoatRoughness"))
       : FString(FMaterialAttributeDefinitionMap::GetAttributeName(Property));
}

// Visits the main material inputs as (PinName, FExpressionInput&): every input the material holds
// (GetExpressionInputForProperty over the engine's material properties), so MaterialAttributes, the customized UVs,
// the shading model pin and FrontMaterial are pins too. A hand list of nineteen left them out: a wire into one, made
// in the editor, read as no wire, and connect_nodes answered "Unknown input on main node" for it.
template <typename TVisitor>
inline void ForEachMainMaterialInput(UMaterial* Material, TVisitor&& Visit)
{
  for (int32 Index = 0; Index < MP_MAX; ++Index) {
    const EMaterialProperty Property = static_cast<EMaterialProperty>(Index);
    if (FExpressionInput* Input = Material->GetExpressionInputForProperty(Property)) {
      const FString Name = McpMainInputName(Property);
      Visit(*Name, *Input);
    }
  }
}

// The names other shading models show for the same pins: a Cloth material's "Cloth" amount
// and the engine's CustomData0/1 are the ClearCoat/ClearCoatRoughness inputs, and its "Fuzz
// Color" is SubsurfaceColor. Only "ClearCoat" was accepted, so a Cloth material could be
// wired only through the clear-coat name.
inline FString CanonicalMainInputName(const FString& PinName)
{
  if (PinName == TEXT("Cloth") || PinName == TEXT("CustomData0")) return TEXT("ClearCoat");
  if (PinName == TEXT("CustomData1")) return TEXT("ClearCoatRoughness");
  if (PinName == TEXT("FuzzColor")) return TEXT("SubsurfaceColor");
  return PinName;
}

// Main material input by pin name (or one of the names above); nullptr when it is not a main pin.
inline FExpressionInput* GetMainMaterialInput(UMaterial* Material, const FString& PinName)
{
  FExpressionInput* Found = nullptr;
  const FString Canonical = CanonicalMainInputName(PinName);
  if (Material) {
    ForEachMainMaterialInput(Material, [&](const TCHAR* Name, FExpressionInput& Input) {
      if (!Found && Canonical == Name) { Found = &Input; }
    });
  }
  return Found;
}

// Every main pin name this engine accepts, for a refusal to list.
inline FString ListMainMaterialInputs(UMaterial* Material)
{
  TArray<FString> Names;
  ForEachMainMaterialInput(Material, [&](const TCHAR* Name, FExpressionInput&) { Names.Add(Name); });
  return FString::Join(Names, TEXT(", ")) + TEXT(" (also Cloth and CustomData0 = ClearCoat, CustomData1 = ClearCoatRoughness, FuzzColor = SubsurfaceColor)");
}

// How many inputs Expr has, from the engine's own list. A walk that calls GetInput(Index) until it returns null does
// not end before UE 5.5: seven engine overrides never return null there (SetMaterialAttributes indexes past its
// array, the custom-output and vertex-interpolator nodes return their one input for every index, and to 5.2 the
// quality, feature-level and shading-path switches index a C array), so every walk counts to this.
inline int32 McpCountMaterialInputs(UMaterialExpression* Expr)
{
#if MCP_HAS_MATERIAL_EXPRESSION_COUNT_INPUTS
  return Expr->CountInputs();
#elif MCP_HAS_MATERIAL_EXPRESSION_INPUTS_VIEW
  return Expr->GetInputsView().Num();
#else
  return Expr->GetInputs().Num();
#endif
}

// Visits every input of Expr as (FExpressionInput&, PinName): the reflected
// ExpressionInput properties by property name, then a Custom node's named
// inputs and a function call's inputs by their declared names, then every other
// input in the engine's own list by the name the engine gives it.
template <typename TVisitor>
inline void ForEachExpressionInput(UMaterialExpression* Expr, TVisitor&& Visit)
{
  if (!Expr) return;
  TSet<const FExpressionInput*> Seen;
  const auto VisitOnce = [&Seen, &Visit](FExpressionInput& Input, const FString& Name) {
    Seen.Add(&Input);
    Visit(Input, Name);
  };
  for (TFieldIterator<FStructProperty> It(Expr->GetClass()); It; ++It) {
    if (It->Struct && It->Struct->GetFName() == FName(TEXT("ExpressionInput"))) {
      VisitOnce(*It->ContainerPtrToValuePtr<FExpressionInput>(Expr), It->GetName());
    }
  }
  if (UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr)) {
    for (FCustomInput& Input : Custom->Inputs) { VisitOnce(Input.Input, Input.InputName.ToString()); }
  }
  if (UMaterialExpressionMaterialFunctionCall* Call = Cast<UMaterialExpressionMaterialFunctionCall>(Expr)) {
    for (FFunctionExpressionInput& Input : Call->FunctionInputs) {
      VisitOnce(Input.Input, Input.Input.InputName.ToString());
    }
  }
  // What connect_nodes wires and the walks above miss: material-attribute inputs (BlendMaterialAttributes A and B),
  // array-held ones (SetMaterialAttributes, Switch) and the switches' inputs past the first.
  for (int32 Index = 0, Count = McpCountMaterialInputs(Expr); Index < Count && Expr->GetInput(Index); ++Index) {
    if (FExpressionInput* Input = Expr->GetInput(Index); !Seen.Contains(Input)) {
      Visit(*Input, Expr->GetInputName(Index).ToString());
    }
  }
}

// The declaration a named reroute usage reads, through a pointer and not an input (null for any other node). A graph
// walk follows it as the engine's own unused-node finder does (UMaterialGraph::GetUnusedMaterialExpressions).
inline UMaterialExpression* McpNamedRerouteSource(UMaterialExpression* Expr)
{
  const UMaterialExpressionNamedRerouteUsage* Usage = Cast<UMaterialExpressionNamedRerouteUsage>(Expr);
  return Usage ? Usage->Declaration.Get() : nullptr;
}

// The input of Expression a reflected property names, or null. Only storage the engine's own input list holds is an
// input: a vector parameter's DefaultValue is a struct property too, and wiring it as an input wrote over the colour.
inline FExpressionInput* FindMaterialInputProperty(UMaterialExpression* Expression, const FString& Name)
{
  const FStructProperty* Prop = CastField<FStructProperty>(Expression->GetClass()->FindPropertyByName(FName(*Name)));
  FExpressionInput* Storage = Prop ? Prop->ContainerPtrToValuePtr<FExpressionInput>(Expression) : nullptr;
  for (int32 Index = 0, Count = Storage ? McpCountMaterialInputs(Expression) : 0; Index < Count; ++Index) {
    if (Expression->GetInput(Index) == Storage) {
      return Storage;
    }
  }
  return nullptr;
}

// Whether Wanted names the input labelled Label: by the label, by a function call's label before " (Type)", or by the
// short name the material editor shows on the pin (Coordinates as UVs, TextureObject as Tex, Exponent as Exp): a
// texture sample's "UVs", as the graph shows it, was refused with "Its inputs: Coordinates".
inline bool McpMaterialInputNamed(const FString& Label, const FString& Wanted)
{
  FString Plain;
  if (!Label.Split(TEXT(" ("), &Plain, nullptr)) {
    Plain = Label;
  }
  const FName Shown = UMaterialGraphNode::GetShortenPinName(FName(*Label));
  return Label.Equals(Wanted, ESearchCase::IgnoreCase) || Plain.Equals(Wanted, ESearchCase::IgnoreCase) ||
         (Shown != NAME_None && Shown.ToString().Equals(Wanted, ESearchCase::IgnoreCase));
}

// The input of Expression that Name names: by its property name, else by its label as McpMaterialInputNamed matches
// it (a custom node's or a function call's inputs are no properties). Null on a miss, OutAvailable then listing the
// labels for the refusal.
inline FExpressionInput* FindMaterialInputByNameOrLabel(UMaterialExpression* Expression, const FString& Name, FString& OutAvailable)
{
  if (FExpressionInput* Input = FindMaterialInputProperty(Expression, Name)) {
    return Input;
  }
  for (int32 Index = 0, Count = McpCountMaterialInputs(Expression); Index < Count && Expression->GetInput(Index); ++Index) {
    const FString Label = Expression->GetInputName(Index).ToString();
    if (!Name.IsEmpty() && McpMaterialInputNamed(Label, Name)) {
      return Expression->GetInput(Index);
    }
    OutAvailable += FString::Printf(TEXT("%s%s"), OutAvailable.IsEmpty() ? TEXT("") : TEXT(", "), *Label);
  }
  return nullptr;
}
