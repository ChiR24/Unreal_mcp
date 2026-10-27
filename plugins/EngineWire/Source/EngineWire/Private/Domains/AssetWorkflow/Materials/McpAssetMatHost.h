#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInterface.h"


static UObject* LoadMaterialOrFunctionAW(const FString& AssetPath,
                                          UMaterial*& OutMaterial,
                                          UMaterialFunction*& OutFunction) {
  OutMaterial = LoadObject<UMaterial>(nullptr, *AssetPath);
  if (OutMaterial) return OutMaterial;
  OutFunction = LoadObject<UMaterialFunction>(nullptr, *AssetPath);
  return OutFunction;
}

// PostEditChange + MarkPackageDirty on whichever host is non-null.
static void FinalizeHost(UMaterial* Material, UMaterialFunction* Function) {
  if (Material) { Material->PostEditChange(); Material->MarkPackageDirty(); }
  else if (Function) { Function->PostEditChange(); Function->MarkPackageDirty(); }
}

