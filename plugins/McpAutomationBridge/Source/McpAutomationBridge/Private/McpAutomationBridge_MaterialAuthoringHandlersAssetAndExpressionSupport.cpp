#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool SaveMaterialAsset(UMaterial *Material) {
  if (!Material)
    return false;

  // Use McpSafeAssetSave for proper asset registry notification
  return McpSafeAssetSave(Material);
}

bool SaveMaterialFunctionAsset(UMaterialFunction *Function) {
  if (!Function)
    return false;

  // Use McpSafeAssetSave for proper asset registry notification
  return McpSafeAssetSave(Function);
}

bool SaveMaterialInstanceAsset(UMaterialInstanceConstant *Instance) {
  if (!Instance)
    return false;

  // Use McpSafeAssetSave for proper asset registry notification
  return McpSafeAssetSave(Instance);
}

// Shared lookup logic for expressions in any array.
// Resolution order: object name (stable ID) > GUID (backwards compat) >
// expr_N index > full path > parameter/input/output name.
// If GUID matches multiple nodes, logs a warning and returns the first.
template<typename TExprArray>
static UMaterialExpression *FindExpressionInArray(TExprArray &Expressions,
                                                   const FString &IdOrName) {
  const FString Needle = IdOrName.TrimStartAndEnd();
  FString ShortNeedle = Needle;
  int32 SeparatorIndex = INDEX_NONE;
  if (ShortNeedle.FindLastChar(TEXT(':'), SeparatorIndex)) {
    ShortNeedle = ShortNeedle.Mid(SeparatorIndex + 1);
  }
  if (ShortNeedle.FindLastChar(TEXT('.'), SeparatorIndex)) {
    ShortNeedle = ShortNeedle.Mid(SeparatorIndex + 1);
  }

  // 1. expr_N index-based lookup
  if (Needle.StartsWith(TEXT("expr_"))) {
    int32 Index = FCString::Atoi(*Needle.Mid(5));
    if (Index >= 0 && Index < Expressions.Num()) {
      UMaterialExpression *Expr = static_cast<UMaterialExpression*>(Expressions[Index]);
      if (Expr) return Expr;
    }
  }

  // 2. Object name match (primary stable ID: "MaterialExpressionCustom_0")
  for (int32 i = 0; i < Expressions.Num(); ++i) {
    UMaterialExpression *Expr = static_cast<UMaterialExpression*>(Expressions[i]);
    if (!Expr) continue;
    if (Expr->GetName() == Needle || Expr->GetName() == ShortNeedle) return Expr;
  }

  // 3. GUID match (backwards compat) — detect collisions
  UMaterialExpression *GuidMatch = nullptr;
  int32 GuidMatchCount = 0;
  for (int32 i = 0; i < Expressions.Num(); ++i) {
    UMaterialExpression *Expr = static_cast<UMaterialExpression*>(Expressions[i]);
    if (!Expr) continue;
    if (Expr->MaterialExpressionGuid.ToString() == Needle) {
      GuidMatchCount++;
      if (!GuidMatch) GuidMatch = Expr;
    }
  }
  if (GuidMatch) {
    if (GuidMatchCount > 1) {
      UE_LOG(LogTemp, Warning,
             TEXT("MCP: GUID '%s' matches %d nodes — returning first. "
                  "Use object name '%s' for unambiguous lookup."),
             *Needle, GuidMatchCount, *GuidMatch->GetName());
    }
    return GuidMatch;
  }

  // 4. Full path match
  for (int32 i = 0; i < Expressions.Num(); ++i) {
    UMaterialExpression *Expr = static_cast<UMaterialExpression*>(Expressions[i]);
    if (!Expr) continue;
    if (Expr->GetPathName() == Needle) return Expr;
  }

  // 5. Semantic name match (parameter name, input/output name)
  for (int32 i = 0; i < Expressions.Num(); ++i) {
    UMaterialExpression *Expr = static_cast<UMaterialExpression*>(Expressions[i]);
    if (!Expr) continue;
    if (UMaterialExpressionParameter *P = Cast<UMaterialExpressionParameter>(Expr)) {
      if (P->ParameterName.ToString() == Needle || P->ParameterName.ToString() == ShortNeedle) return Expr;
    }
    if (UMaterialExpressionFunctionInput *In = Cast<UMaterialExpressionFunctionInput>(Expr)) {
      if (In->InputName.ToString() == Needle || In->InputName.ToString() == ShortNeedle) return Expr;
    }
    if (UMaterialExpressionFunctionOutput *Out = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
      if (Out->OutputName.ToString() == Needle || Out->OutputName.ToString() == ShortNeedle) return Expr;
    }
  }

  return nullptr;
}

UMaterialExpression *FindExpressionByIdOrName(UMaterial *Material,
                                                      const FString &IdOrName) {
  if (IdOrName.IsEmpty() || !Material) return nullptr;
  auto &Expressions = MCP_GET_MATERIAL_EXPRESSIONS(Material);
  return FindExpressionInArray(Expressions, IdOrName);
}

UMaterialExpression *
FindExpressionByIdOrNameInFunction(UMaterialFunction *Function,
                                   const FString &IdOrName) {
  if (IdOrName.IsEmpty() || !Function) return nullptr;
  auto &Expressions = MCP_GET_FUNCTION_EXPRESSIONS(Function);
  return FindExpressionInArray(Expressions, IdOrName);
}

// Resolve an asset path as either a UMaterial or a UMaterialFunction.
// Exactly one of OutMaterial / OutFunction will be populated on success.
UObject *LoadMaterialOrFunction(const FString &AssetPath,
                                       UMaterial *&OutMaterial,
                                       UMaterialFunction *&OutFunction) {
  OutMaterial = nullptr;
  OutFunction = nullptr;

  // Prefer a generic load and type-check the result: this avoids the
  // LoadObject<UMaterial> null when the target is actually a function.
  UObject *Loaded = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
  if (!Loaded) {
    // Fallback: try both concrete types directly.
    OutMaterial = LoadObject<UMaterial>(nullptr, *AssetPath);
    if (OutMaterial) return OutMaterial;
    OutFunction = LoadObject<UMaterialFunction>(nullptr, *AssetPath);
    return OutFunction;
  }

  if (UMaterial *AsMaterial = Cast<UMaterial>(Loaded)) {
    OutMaterial = AsMaterial;
    return AsMaterial;
  }
  if (UMaterialFunction *AsFunction = Cast<UMaterialFunction>(Loaded)) {
    OutFunction = AsFunction;
    return AsFunction;
  }
  // A UMaterialFunctionInterface that isn't a concrete UMaterialFunction
  // (e.g. UMaterialFunctionInstance) is not directly editable here.
  if (UMaterialFunctionInterface *AsIface =
          Cast<UMaterialFunctionInterface>(Loaded)) {
    // Resolve to the underlying parent function when possible.
    if (UMaterialFunction *Parent =
            Cast<UMaterialFunction>(AsIface->GetBaseFunction())) {
      OutFunction = Parent;
      return Parent;
    }
  }
  return nullptr;
}

void AddExpressionToContainer(UMaterial *Material,
                                     UMaterialFunction *Function,
                                     UMaterialExpression *Expr) {
  if (!Expr) return;
#if WITH_EDITORONLY_DATA
  if (Material) {
    MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(Expr);
  } else if (Function) {
    MCP_GET_FUNCTION_EXPRESSIONS(Function).Add(Expr);
  }
#endif
}

FString FunctionInputTypeToString(EFunctionInputType InType) {
  switch (InType) {
  case EFunctionInputType::FunctionInput_Scalar:             return TEXT("Scalar");
  case EFunctionInputType::FunctionInput_Vector2:            return TEXT("Vector2");
  case EFunctionInputType::FunctionInput_Vector3:            return TEXT("Vector3");
  case EFunctionInputType::FunctionInput_Vector4:            return TEXT("Vector4");
  case EFunctionInputType::FunctionInput_Texture2D:          return TEXT("Texture2D");
  case EFunctionInputType::FunctionInput_TextureCube:        return TEXT("TextureCube");
  case EFunctionInputType::FunctionInput_StaticBool:         return TEXT("StaticBool");
  case EFunctionInputType::FunctionInput_MaterialAttributes: return TEXT("MaterialAttributes");
  default:                                                   return TEXT("Unknown");
  }
}
}
#endif
