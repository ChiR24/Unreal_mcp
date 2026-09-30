#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"

namespace McpMaterialAuthoringHandlers
{
bool PrepareNewMaterialAsset(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                             const FString& RawName, FString RequestedPath, const TCHAR* DefaultPath, const TCHAR* Noun,
                             FString& OutName, FString& OutPackagePath, bool& bOutParentFolderCreated) {
  bOutParentFolderCreated = false;
  if (RawName.IsEmpty()) {
    Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'name'."), TEXT("INVALID_ARGUMENT"));
    return false;
  }
  // Only underscores may change: anything else means the name had characters an asset name cannot hold.
  OutName = SanitizeAssetName(RawName);
  if (OutName.Replace(TEXT("_"), TEXT("")) != RawName.Replace(TEXT("_"), TEXT(""))) {
    Bridge->SendAutomationError(Socket, RequestId,
        FString::Printf(TEXT("Invalid %s name '%s': contains characters that cannot be used in asset names. Valid name would be: '%s'"),
                        Noun, *RawName, *OutName),
        TEXT("INVALID_NAME"));
    return false;
  }
  if (RequestedPath.IsEmpty()) {
    RequestedPath = DefaultPath;
  }
  FString PathError;
  if (!ValidateAssetCreationPath(RequestedPath, OutName, OutPackagePath, PathError)) {
    Bridge->SendAutomationError(Socket, RequestId, PathError, TEXT("INVALID_PATH"));
    return false;
  }
  if (OutPackagePath.Contains(TEXT(":"))) {
    Bridge->SendAutomationError(Socket, RequestId,
        FString::Printf(TEXT("Invalid path '%s': absolute Windows paths are not allowed"), *OutPackagePath),
        TEXT("INVALID_PATH"));
    return false;
  }
  FText MountReason;
  if (!FPackageName::IsValidLongPackageName(OutPackagePath, true, &MountReason)) {
    Bridge->SendAutomationError(Socket, RequestId,
        FString::Printf(TEXT("Invalid package path '%s': %s"), *OutPackagePath, *MountReason.ToString()),
        TEXT("INVALID_PATH"));
    return false;
  }
  // Make the parent chain rather than refusing: create_folder creates parents too. The asset
  // registry can lag a folder already on disk, so ask the directory itself.
  const FString ParentFolderPath = FPackageName::GetLongPackagePath(OutPackagePath);
  if (!FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().PathExists(FName(*ParentFolderPath))) {
    const bool bAlreadyOnDisk = UEditorAssetLibrary::DoesDirectoryExist(ParentFolderPath);
    if (!UEditorAssetLibrary::MakeDirectory(ParentFolderPath)) {
      Bridge->SendAutomationError(Socket, RequestId,
          FString::Printf(TEXT("Parent folder does not exist: %s. Create the folder first or use an existing path."), *ParentFolderPath),
          TEXT("PARENT_FOLDER_NOT_FOUND"));
      return false;
    }
    bOutParentFolderCreated = !bAlreadyOnDisk;
  }
  // Creating over an existing asset of another class is a fatal engine error.
  const FString FullAssetPath = OutPackagePath + TEXT(".") + OutName;
  if (McpAssetExists(FullAssetPath)) {
    const UObject* Existing = McpLoadAsset(FullAssetPath);
    Bridge->SendAutomationError(Socket, RequestId,
        FString::Printf(TEXT("Asset '%s' already exists as %s. Cannot create %s with the same name."),
                        *FullAssetPath, Existing ? *Existing->GetClass()->GetName() : TEXT("Unknown"), Noun),
        TEXT("ASSET_EXISTS"));
    return false;
  }
  return true;
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
    return nullptr;
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
  if (Material) {
    MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(Expr);
  } else if (Function) {
    MCP_GET_FUNCTION_EXPRESSIONS(Function).Add(Expr);
  }
}

FString FunctionInputTypeToString(EFunctionInputType InType) {
  const FString Name = StaticEnum<EFunctionInputType>()->GetNameStringByValue(InType);
  return Name.IsEmpty() ? FString(TEXT("Unknown")) : Name.RightChop(14); // "FunctionInput_"
}
}
