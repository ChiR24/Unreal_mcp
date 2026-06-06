#pragma once

#include "McpVersionCompatibility.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInterface.h"

#if WITH_EDITOR

static UObject* LoadMaterialOrFunctionAW(const FString& AssetPath,
                                          UMaterial*& OutMaterial,
                                          UMaterialFunction*& OutFunction) {
  OutMaterial = LoadObject<UMaterial>(nullptr, *AssetPath);
  if (OutMaterial) return OutMaterial;
  OutFunction = LoadObject<UMaterialFunction>(nullptr, *AssetPath);
  return OutFunction;
}

// Return a reference to the expressions TArray for either host type.
// Caller must ensure at least one of Material/Function is non-null.
// Uses decltype(auto) so the return type matches the underlying member
// (TArray<TObjectPtr<...>>& on UE 5.1+, TArray<UMaterialExpression*>& on 5.0).
static decltype(auto) GetHostExpressions(
    UMaterial* Material, UMaterialFunction* Function) {
  return Material ? MCP_GET_MATERIAL_EXPRESSIONS(Material)
                  : MCP_GET_FUNCTION_EXPRESSIONS(Function);
}

// Find a material expression by GUID, name, path, parameter name, or numeric index.
// Templated to accept both TArray<TObjectPtr<...>> (UE 5.1+) and TArray<UMaterialExpression*>.
template <typename TExprArray>
static UMaterialExpression* FindExpressionInHost(TExprArray& Expressions, const FString& IdOrIndex) {
  if (IdOrIndex.IsEmpty()) return nullptr;

  FGuid GuidId;
  if (FGuid::Parse(IdOrIndex, GuidId)) {
    for (UMaterialExpression *Expr : Expressions) {
      if (Expr && Expr->MaterialExpressionGuid == GuidId) return Expr;
    }
  }
  for (UMaterialExpression *Expr : Expressions) {
    if (Expr) {
      if (Expr->GetName() == IdOrIndex || Expr->GetPathName() == IdOrIndex) return Expr;
      if (UMaterialExpressionParameter *Param = Cast<UMaterialExpressionParameter>(Expr)) {
        if (Param->ParameterName.ToString() == IdOrIndex) return Expr;
      }
    }
  }
  if (IdOrIndex.IsNumeric()) {
    int32 Index = FCString::Atoi(*IdOrIndex);
    if (Index >= 0 && Index < Expressions.Num()) return Expressions[Index];
  }
  return nullptr;
}

// PostEditChange + MarkPackageDirty on whichever host is non-null.
static void FinalizeHost(UMaterial* Material, UMaterialFunction* Function) {
  if (Material) { Material->PostEditChange(); Material->MarkPackageDirty(); }
  else if (Function) { Function->PostEditChange(); Function->MarkPackageDirty(); }
}

#endif // WITH_EDITOR
