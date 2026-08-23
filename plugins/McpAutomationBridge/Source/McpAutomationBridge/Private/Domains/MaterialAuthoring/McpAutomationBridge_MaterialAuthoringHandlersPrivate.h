#pragma once

// MCP Core
#include "McpAutomationBridgeSubsystem.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Core/Compatibility/McpVersionCompatibility.h"

// JSON & Serialization
#include "Dom/JsonObject.h"

// Engine Version
#include "Misc/EngineVersionComparison.h"

#if WITH_EDITOR

// Asset Tools & Registry
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"

// Graph
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"

// Material Core
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialInstanceConstant.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/Texture.h"

// UE 5.1+ MaterialDomain
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "MaterialDomain.h"
#endif

// Material Expressions (Basic)
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionFrac.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionIf.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionPixelDepth.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionReflectionVectorWS.h"

// UE 5.1+ MaterialExpressionRotator
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "Materials/MaterialExpressionRotator.h"
#endif

// Material Expressions (Parameters)
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"

// Material Expressions (Utility)
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionCrossProduct.h"
#include "Materials/MaterialExpressionDesaturation.h"

// Factories
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialFunctionFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"

// Core
#include "EditorAssetLibrary.h"

// Landscape (UE 5.0+)
#if ENGINE_MAJOR_VERSION >= 5
#include "LandscapeLayerInfoObject.h"
#define MCP_HAS_LANDSCAPE_LAYER 1
#else
#define MCP_HAS_LANDSCAPE_LAYER 0
#endif
#endif

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
// The material root output is a UMaterialGraphNode_Root, not a UMaterialExpression, so it is
// never present in the expression list every node lookup here walks. It is addressed by the
// "Main" sentinel instead. Callers reach for the name the material editor shows them, and the
// resulting miss used to surface as NODE_NOT_FOUND on a graph that cannot be terminated any
// other way, so accept the spellings people actually type.
inline bool IsMaterialRootTarget(const FString& TargetNodeId)
{
  FString Key = TargetNodeId.TrimStartAndEnd().ToLower();
  Key.ReplaceInline(TEXT(" "), TEXT(""));
  Key.ReplaceInline(TEXT("_"), TEXT(""));
  if (Key.IsEmpty() || Key == TEXT("main") || Key == TEXT("root") || Key == TEXT("rootnode") ||
      Key == TEXT("output") || Key == TEXT("materialoutput") || Key == TEXT("material")) {
    return true;
  }
  // The graph node class name, plus the instance index the editor appends ("..._Root_0").
  static const FString RootClassKey(TEXT("materialgraphnoderoot"));
  if (Key.StartsWith(RootClassKey)) {
    const FString Suffix = Key.RightChop(RootClassKey.Len());
    return Suffix.IsEmpty() || Suffix.IsNumeric();
  }
  return false;
}

// Root inputs are matched by name against fixed literals. FString comparison already ignores
// case, so only the separators a caller may type ("Base Color") need removing.
inline FString NormalizeMaterialInputName(const FString& InputName)
{
  FString Key = InputName.TrimStartAndEnd();
  Key.ReplaceInline(TEXT(" "), TEXT(""));
  Key.ReplaceInline(TEXT("_"), TEXT(""));
  return Key;
}

bool SaveMaterialAsset(UMaterial* Material);
bool SaveMaterialFunctionAsset(UMaterialFunction* Function);
bool SaveMaterialInstanceAsset(UMaterialInstanceConstant* Instance);
UMaterialExpression* FindExpressionByIdOrName(UMaterial* Material, const FString& NodeIdOrName);
UMaterialExpression* FindExpressionByIdOrNameInFunction(UMaterialFunction* Function, const FString& NodeIdOrName);
UObject* LoadMaterialOrFunction(const FString& AssetPath, UMaterial*& OutMaterial, UMaterialFunction*& OutFunction);
void AddExpressionToContainer(UMaterial* Material, UMaterialFunction* Function, UMaterialExpression* Expr);
FString FunctionInputTypeToString(EFunctionInputType InType);
void AppendMaterialInfoConnections(UMaterial* Material, UMaterialFunction* Function, const TSharedPtr<FJsonObject>& Payload, const TSharedPtr<FJsonObject>& Result);
bool HandleCreateMaterial(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSetBlendMode(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSetShadingModel(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSetMaterialDomain(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAddTextureSample(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAddTextureCoordinate(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAddScalarParameter(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAddVectorParameter(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAddStaticSwitchParameter(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAddMathNode(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAddSceneDataNode(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAddConditionalNode(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAddComponentMask(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAddDotProduct(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAddCrossProduct(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAddDesaturation(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAddAppendVector(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAddCustomExpression(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleConnectNodes(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleDisconnectNodes(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateMaterialFunction(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleFunctionInputsOutputs(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleUseMaterialFunction(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateMaterialInstance(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSetScalarParameterValue(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSetVectorParameterValue(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSetTextureParameterValue(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateSpecializedMaterial(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAddLandscapeLayer(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleConfigureLayerBlend(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCompileMaterial(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleGetMaterialInfo(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleGetMaterialFunctionInfo(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleFindNode(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleGetNodeConnections(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleGetNodeProperties(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSetStaticSwitchParameterValue(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleDeleteNode(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleUpdateCustomExpression(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleGetNodeChain(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleGetConnectedSubgraph(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAddMaterialNode(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleRemoveMaterialNode(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSetNodePosition(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSetMaterialParameter(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleGetMaterialNodeDetails(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSetTwoSided(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSetCastShadows(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
}

// Helper macro for expression creation - validates path BEFORE loading
#define LOAD_MATERIAL_OR_RETURN()                                              \
  FString AssetPath;                                                           \
  if ((!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||            \
       AssetPath.IsEmpty()) &&                                                 \
      (!Payload->TryGetStringField(TEXT("materialPath"), AssetPath) ||         \
       AssetPath.IsEmpty())) {                                                 \
    Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath' or 'materialPath'."), \
                        TEXT("INVALID_ARGUMENT"));                             \
    return true;                                                               \
  }                                                                            \
  /* SECURITY: Validate path BEFORE loading asset */                           \
  FString ValidatedAssetPath = SanitizeProjectRelativePath(AssetPath);         \
  if (ValidatedAssetPath.IsEmpty()) {                                          \
    Bridge->SendAutomationError(Socket, RequestId,                                     \
                        FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath), \
                        TEXT("INVALID_PATH"));                                \
    return true;                                                               \
  }                                                                            \
  AssetPath = ValidatedAssetPath;                                              \
  UMaterial *Material = LoadObject<UMaterial>(nullptr, *AssetPath);            \
  if (!Material) {                                                             \
    Bridge->SendAutomationError(Socket, RequestId, TEXT("Could not load Material."),   \
                        TEXT("ASSET_NOT_FOUND"));                              \
    return true;                                                               \
  }                                                                            \
  float X = 0.0f, Y = 0.0f;                                                    \
  Payload->TryGetNumberField(TEXT("x"), X);                                    \
  Payload->TryGetNumberField(TEXT("y"), Y)

// MF-aware variant of LOAD_MATERIAL_OR_RETURN.
// Declares Material*, Function*, HostOuter (whichever is non-null), and X/Y.
// Exactly one of {Material, Function} will be non-null on success.
#define LOAD_MATERIAL_OR_FUNCTION_OR_RETURN()                                  \
  FString AssetPath;                                                           \
  if ((!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||            \
       AssetPath.IsEmpty()) &&                                                 \
      (!Payload->TryGetStringField(TEXT("materialPath"), AssetPath) ||         \
       AssetPath.IsEmpty())) {                                                 \
    Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath' or 'materialPath'."), \
                        TEXT("INVALID_ARGUMENT"));                             \
    return true;                                                               \
  }                                                                            \
  FString ValidatedAssetPath = SanitizeProjectRelativePath(AssetPath);         \
  if (ValidatedAssetPath.IsEmpty()) {                                          \
    Bridge->SendAutomationError(Socket, RequestId,                                     \
                        FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath), \
                        TEXT("INVALID_PATH"));                                 \
    return true;                                                               \
  }                                                                            \
  AssetPath = ValidatedAssetPath;                                              \
  UMaterial *Material = nullptr;                                               \
  UMaterialFunction *Function = nullptr;                                       \
  LoadMaterialOrFunction(AssetPath, Material, Function);                       \
  if (!Material && !Function) {                                                \
    Bridge->SendAutomationError(Socket, RequestId,                                     \
                        TEXT("Could not load Material or Material Function."),\
                        TEXT("ASSET_NOT_FOUND"));                              \
    return true;                                                               \
  }                                                                            \
  UObject *HostOuter = Material ? static_cast<UObject*>(Material)              \
                                 : static_cast<UObject*>(Function);            \
  float X = 0.0f, Y = 0.0f;                                                    \
  Payload->TryGetNumberField(TEXT("x"), X);                                    \
  Payload->TryGetNumberField(TEXT("y"), Y)

// Find an expression in either container by GUID / name / parameter name.
#define FIND_EXPR_IN_HOST(NodeIdOrName)                                      \
    (Material ? FindExpressionByIdOrName(Material, (NodeIdOrName))             \
              : FindExpressionByIdOrNameInFunction(Function, (NodeIdOrName)))

// Finalize edits for either container.
#define FINALIZE_HOST()                                                      \
    do {                                                                       \
      if (Material) { Material->PostEditChange(); Material->MarkPackageDirty(); } \
      else if (Function) { Function->PostEditChange(); Function->MarkPackageDirty(); } \
    } while (0)

// Stable node ID: use UObject name (e.g. "MaterialExpressionCustom_0")
// which is unique within an asset and immune to GUID duplication.
// Responses also include "guid" for backwards compatibility.
#define MCP_NODE_ID(Expr) ((Expr)->GetName())

/**
 * Estimated on-graph extent of a material expression.
 *
 * Slate decides the real size when the material editor draws, so it does not
 * exist on this path. Callers still place expressions by MaterialExpressionEditorX/Y,
 * and with no size back from the tool consecutive parameter nodes stack on top
 * of one another. Estimated from connector count plus an allowance for the
 * inline default-value widget a parameter node draws (a vector parameter's
 * colour swatch is much taller than a scalar's number box).
 */
inline void EstimateMaterialNodeExtent(UMaterialExpression *Expr, float &OutWidth, float &OutHeight) {
  OutWidth = 200.0f;
  OutHeight = 64.0f;
  if (!Expr) {
    return;
  }
  // CountInputs/GetOutputs are the stable accessors across the 5.0-5.8 range the
  // plugin supports; GetInputs() does not exist on UMaterialExpression.
  const int32 InputCount = Expr->CountInputs();
  const int32 OutputCount = Expr->GetOutputs().Num();
  const int32 Rows = FMath::Max(InputCount, OutputCount);
  OutHeight = 48.0f + Rows * 26.0f;

  // Parameter nodes draw their default value inline under the title.
  if (Expr->IsA<UMaterialExpressionVectorParameter>()) {
    OutHeight += 118.0f; // colour swatch
  } else if (Expr->IsA<UMaterialExpressionScalarParameter>() ||
             Expr->IsA<UMaterialExpressionStaticBoolParameter>()) {
    OutHeight += 34.0f; // single value row
  }

  const FString Caption = Expr->GetName();
  OutWidth = FMath::Max(200.0f, Caption.Len() * 8.0f + 56.0f);
}

/**
 * Adds posX/posY and the estimated extent to a material node result, plus an
 * overlap warning naming the expressions the new node was dropped on top of.
 * Returns the warning text (empty when the placement is clear).
 */
inline FString AddMaterialNodePlacementFields(const TSharedPtr<FJsonObject> &Result,
                                              UMaterial *Material,
                                              UMaterialExpression *NewExpr) {
  if (!Result.IsValid() || !NewExpr) {
    return FString();
  }
  float NewWidth = 0.0f;
  float NewHeight = 0.0f;
  EstimateMaterialNodeExtent(NewExpr, NewWidth, NewHeight);
  const float NewLeft = static_cast<float>(NewExpr->MaterialExpressionEditorX);
  const float NewTop = static_cast<float>(NewExpr->MaterialExpressionEditorY);
  Result->SetNumberField(TEXT("posX"), NewExpr->MaterialExpressionEditorX);
  Result->SetNumberField(TEXT("posY"), NewExpr->MaterialExpressionEditorY);
  Result->SetNumberField(TEXT("estimatedWidth"), NewWidth);
  Result->SetNumberField(TEXT("estimatedHeight"), NewHeight);

  // Overlap is only computed for a base material; a material function's
  // expression list is reached differently and is left unchecked rather than
  // reported wrongly.
  if (!Material) {
    return FString();
  }

  TArray<FString> Overlapping;
  for (UMaterialExpression *Other : MCP_GET_MATERIAL_EXPRESSIONS(Material)) {
    if (!Other || Other == NewExpr) {
      continue;
    }
    float OtherWidth = 0.0f;
    float OtherHeight = 0.0f;
    EstimateMaterialNodeExtent(Other, OtherWidth, OtherHeight);
    const float OtherLeft = static_cast<float>(Other->MaterialExpressionEditorX);
    const float OtherTop = static_cast<float>(Other->MaterialExpressionEditorY);
    const bool bSeparated = NewLeft + NewWidth <= OtherLeft ||
                            OtherLeft + OtherWidth <= NewLeft ||
                            NewTop + NewHeight <= OtherTop ||
                            OtherTop + OtherHeight <= NewTop;
    if (!bSeparated) {
      Overlapping.Add(Other->GetName());
    }
  }
  if (Overlapping.Num() == 0) {
    return FString();
  }

  TArray<TSharedPtr<FJsonValue>> OverlapValues;
  for (const FString &Name : Overlapping) {
    OverlapValues.Add(MakeShared<FJsonValueString>(Name));
  }
  Result->SetArrayField(TEXT("overlappingNodes"), OverlapValues);
  const FString Warning = FString::Printf(
      TEXT("Node placed at (%d, %d) overlaps %d existing node(s): %s. Estimated ")
      TEXT("size is %.0fx%.0f; offset the next node by at least that height."),
      NewExpr->MaterialExpressionEditorX, NewExpr->MaterialExpressionEditorY,
      Overlapping.Num(), *FString::Join(Overlapping, TEXT(", ")), NewWidth, NewHeight);
  Result->SetStringField(TEXT("placementWarning"), Warning);
  return Warning;
}

#endif
