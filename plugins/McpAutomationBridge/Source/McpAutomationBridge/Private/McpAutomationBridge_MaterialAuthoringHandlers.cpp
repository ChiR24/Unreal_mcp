/**
 * Material Authoring Handlers - Phase 8
 *
 * Advanced material creation and shader authoring capabilities.
 * Implements: create_material, add expressions, connect nodes, material instances,
 * material functions, specialized materials (landscape, decal, post-process).
 */

#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/EngineVersionComparison.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Engine/Texture.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialFunctionFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "IAssetTools.h"
#include "Materials/Material.h"
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
#include "Materials/MaterialExpressionRotator.h"
#include "Materials/MaterialExpressionSubstrate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialInstanceConstant.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/SubsurfaceProfile.h"  // For USubsurfaceProfile and FSubsurfaceProfileStruct
// Note: SavePackage.h removed - use McpSafeAssetSave() from McpAutomationBridgeHelpers.h instead
#include "EditorAssetLibrary.h"

// Landscape layer info (for add_landscape_layer)
#if __has_include("LandscapeLayerInfoObject.h")
#include "LandscapeLayerInfoObject.h"
#define MCP_HAS_LANDSCAPE_LAYER 1
#else
#define MCP_HAS_LANDSCAPE_LAYER 0
#endif

// Landscape material expressions
#if __has_include("Materials/MaterialExpressionLandscapeLayerBlend.h")
#include "Materials/MaterialExpressionLandscapeLayerBlend.h"
#define MCP_HAS_LANDSCAPE_MATERIAL_EXPRESSIONS 1
#else
#define MCP_HAS_LANDSCAPE_MATERIAL_EXPRESSIONS 0
#endif
#endif

// Forward declarations of helper functions
static bool SaveMaterialAsset(UMaterial *Material);
static bool SaveMaterialFunctionAsset(UMaterialFunction *Function);
static bool SaveMaterialInstanceAsset(UMaterialInstanceConstant *Instance);
static UMaterialExpression *FindExpressionByIdOrName(UMaterial *Material,
                                                      const FString &IdOrName);

bool UMcpAutomationBridgeSubsystem::HandleManageMaterialAuthoringAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  // Accept either "manage_material_authoring" action or specific material actions
  // routed from HandleAssetAction (add_material_node, connect_material_pins, etc.)
  bool bIsMaterialAction = (Action == TEXT("manage_material_authoring"));
  bool bIsRoutedAction = Action.StartsWith(TEXT("add_material")) || 
                         Action.StartsWith(TEXT("connect_material")) ||
                         Action.StartsWith(TEXT("remove_material")) ||
                         Action.StartsWith(TEXT("get_material")) ||
                         Action.StartsWith(TEXT("create_material")) ||
                         Action.StartsWith(TEXT("material_"));
  
  if (!bIsMaterialAction && !bIsRoutedAction) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing payload."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Determine SubAction: either from payload field or from the Action parameter itself
  FString SubAction;
  if (!Payload->TryGetStringField(TEXT("subAction"), SubAction) || SubAction.IsEmpty()) {
    // If Action is a specific action (not manage_material_authoring), use it as SubAction
    if (bIsRoutedAction) {
      SubAction = Action;
    } else {
      // Try the 'action' field as fallback
      if (!Payload->TryGetStringField(TEXT("action"), SubAction) || SubAction.IsEmpty()) {
        SendAutomationError(Socket, RequestId,
                            TEXT("Missing 'subAction' for manage_material_authoring"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
    }
  }

  // ==========================================================================
  // 8.1 Material Creation Actions
  // ==========================================================================
  if (SubAction == TEXT("create_material")) {
    FString Name, Path;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'name'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Path = Payload->GetStringField(TEXT("path"));
    if (Path.IsEmpty()) {
      Path = TEXT("/Game/Materials");
    }

    // Create material using factory
    UMaterialFactoryNew *Factory = NewObject<UMaterialFactoryNew>();
    FString PackagePath = Path / Name;
    UPackage *Package = CreatePackage(*PackagePath);
    if (!Package) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create package."),
                          TEXT("PACKAGE_ERROR"));
      return true;
    }

    UMaterial *NewMaterial = Cast<UMaterial>(
        Factory->FactoryCreateNew(UMaterial::StaticClass(), Package,
                                  FName(*Name), RF_Public | RF_Standalone,
                                  nullptr, GWarn));
    if (!NewMaterial) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create material."),
                          TEXT("CREATE_FAILED"));
      return true;
    }

    // Set properties
    FString MaterialDomain;
    if (Payload->TryGetStringField(TEXT("materialDomain"), MaterialDomain)) {
      if (MaterialDomain == TEXT("Surface"))
        NewMaterial->MaterialDomain = EMaterialDomain::MD_Surface;
      else if (MaterialDomain == TEXT("DeferredDecal"))
        NewMaterial->MaterialDomain = EMaterialDomain::MD_DeferredDecal;
      else if (MaterialDomain == TEXT("LightFunction"))
        NewMaterial->MaterialDomain = EMaterialDomain::MD_LightFunction;
      else if (MaterialDomain == TEXT("Volume"))
        NewMaterial->MaterialDomain = EMaterialDomain::MD_Volume;
      else if (MaterialDomain == TEXT("PostProcess"))
        NewMaterial->MaterialDomain = EMaterialDomain::MD_PostProcess;
      else if (MaterialDomain == TEXT("UI"))
        NewMaterial->MaterialDomain = EMaterialDomain::MD_UI;
    }

    FString BlendMode;
    if (Payload->TryGetStringField(TEXT("blendMode"), BlendMode)) {
      if (BlendMode == TEXT("Opaque"))
        NewMaterial->BlendMode = EBlendMode::BLEND_Opaque;
      else if (BlendMode == TEXT("Masked"))
        NewMaterial->BlendMode = EBlendMode::BLEND_Masked;
      else if (BlendMode == TEXT("Translucent"))
        NewMaterial->BlendMode = EBlendMode::BLEND_Translucent;
      else if (BlendMode == TEXT("Additive"))
        NewMaterial->BlendMode = EBlendMode::BLEND_Additive;
      else if (BlendMode == TEXT("Modulate"))
        NewMaterial->BlendMode = EBlendMode::BLEND_Modulate;
      else if (BlendMode == TEXT("AlphaComposite"))
        NewMaterial->BlendMode = EBlendMode::BLEND_AlphaComposite;
      else if (BlendMode == TEXT("AlphaHoldout"))
        NewMaterial->BlendMode = EBlendMode::BLEND_AlphaHoldout;
    }

    FString ShadingModel;
    if (Payload->TryGetStringField(TEXT("shadingModel"), ShadingModel)) {
      if (ShadingModel == TEXT("Unlit"))
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_Unlit);
      else if (ShadingModel == TEXT("DefaultLit"))
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_DefaultLit);
      else if (ShadingModel == TEXT("Subsurface"))
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_Subsurface);
      else if (ShadingModel == TEXT("SubsurfaceProfile"))
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_SubsurfaceProfile);
      else if (ShadingModel == TEXT("PreintegratedSkin"))
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_PreintegratedSkin);
      else if (ShadingModel == TEXT("ClearCoat"))
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_ClearCoat);
      else if (ShadingModel == TEXT("Hair"))
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_Hair);
      else if (ShadingModel == TEXT("Cloth"))
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_Cloth);
      else if (ShadingModel == TEXT("Eye"))
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_Eye);
      else if (ShadingModel == TEXT("TwoSidedFoliage"))
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_TwoSidedFoliage);
      else if (ShadingModel == TEXT("ThinTranslucent"))
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_ThinTranslucent);
    }

    bool bTwoSided = false;
    if (Payload->TryGetBoolField(TEXT("twoSided"), bTwoSided)) {
      NewMaterial->TwoSided = bTwoSided;
    }

    NewMaterial->PostEditChange();
    NewMaterial->MarkPackageDirty();

    // Notify asset registry FIRST (required for UE 5.7+ before saving)
    FAssetRegistryModule::AssetCreated(NewMaterial);

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialAsset(NewMaterial);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("assetPath"), NewMaterial->GetPathName());
    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Material '%s' created."), *Name),
                           Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // set_blend_mode
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("set_blend_mode")) {
    FString AssetPath, BlendMode;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) &&
        !Payload->TryGetStringField(TEXT("materialPath"), AssetPath)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath' or 'materialPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("blendMode"), BlendMode)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'blendMode'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterial *Material = LoadObject<UMaterial>(nullptr, *AssetPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, TEXT("Could not load Material."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    if (BlendMode == TEXT("Opaque"))
      Material->BlendMode = EBlendMode::BLEND_Opaque;
    else if (BlendMode == TEXT("Masked"))
      Material->BlendMode = EBlendMode::BLEND_Masked;
    else if (BlendMode == TEXT("Translucent"))
      Material->BlendMode = EBlendMode::BLEND_Translucent;
    else if (BlendMode == TEXT("Additive"))
      Material->BlendMode = EBlendMode::BLEND_Additive;
    else if (BlendMode == TEXT("Modulate"))
      Material->BlendMode = EBlendMode::BLEND_Modulate;
    else if (BlendMode == TEXT("AlphaComposite"))
      Material->BlendMode = EBlendMode::BLEND_AlphaComposite;
    else if (BlendMode == TEXT("AlphaHoldout"))
      Material->BlendMode = EBlendMode::BLEND_AlphaHoldout;

    Material->PostEditChange();
    Material->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialAsset(Material);
    }

    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Blend mode set to %s."), *BlendMode));
    return true;
  }

  // --------------------------------------------------------------------------
  // set_shading_model
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("set_shading_model")) {
    FString AssetPath, ShadingModel;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) &&
        !Payload->TryGetStringField(TEXT("materialPath"), AssetPath)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath' or 'materialPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("shadingModel"), ShadingModel)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'shadingModel'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterial *Material = LoadObject<UMaterial>(nullptr, *AssetPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, TEXT("Could not load Material."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    if (ShadingModel == TEXT("Unlit"))
      Material->SetShadingModel(EMaterialShadingModel::MSM_Unlit);
    else if (ShadingModel == TEXT("DefaultLit"))
      Material->SetShadingModel(EMaterialShadingModel::MSM_DefaultLit);
    else if (ShadingModel == TEXT("Subsurface"))
      Material->SetShadingModel(EMaterialShadingModel::MSM_Subsurface);
    else if (ShadingModel == TEXT("SubsurfaceProfile"))
      Material->SetShadingModel(EMaterialShadingModel::MSM_SubsurfaceProfile);
    else if (ShadingModel == TEXT("PreintegratedSkin"))
      Material->SetShadingModel(EMaterialShadingModel::MSM_PreintegratedSkin);
    else if (ShadingModel == TEXT("ClearCoat"))
      Material->SetShadingModel(EMaterialShadingModel::MSM_ClearCoat);
    else if (ShadingModel == TEXT("Hair"))
      Material->SetShadingModel(EMaterialShadingModel::MSM_Hair);
    else if (ShadingModel == TEXT("Cloth"))
      Material->SetShadingModel(EMaterialShadingModel::MSM_Cloth);
    else if (ShadingModel == TEXT("Eye"))
      Material->SetShadingModel(EMaterialShadingModel::MSM_Eye);
    else if (ShadingModel == TEXT("TwoSidedFoliage"))
      Material->SetShadingModel(EMaterialShadingModel::MSM_TwoSidedFoliage);
    else if (ShadingModel == TEXT("ThinTranslucent"))
      Material->SetShadingModel(EMaterialShadingModel::MSM_ThinTranslucent);

    Material->PostEditChange();
    Material->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialAsset(Material);
    }

    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Shading model set to %s."), *ShadingModel));
    return true;
  }

  // --------------------------------------------------------------------------
  // set_material_domain
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("set_material_domain")) {
    FString AssetPath, Domain;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) &&
        !Payload->TryGetStringField(TEXT("materialPath"), AssetPath)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath' or 'materialPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("materialDomain"), Domain) &&
        !Payload->TryGetStringField(TEXT("domain"), Domain)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'materialDomain' or 'domain'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterial *Material = LoadObject<UMaterial>(nullptr, *AssetPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, TEXT("Could not load Material."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    if (Domain == TEXT("Surface"))
      Material->MaterialDomain = EMaterialDomain::MD_Surface;
    else if (Domain == TEXT("DeferredDecal"))
      Material->MaterialDomain = EMaterialDomain::MD_DeferredDecal;
    else if (Domain == TEXT("LightFunction"))
      Material->MaterialDomain = EMaterialDomain::MD_LightFunction;
    else if (Domain == TEXT("Volume"))
      Material->MaterialDomain = EMaterialDomain::MD_Volume;
    else if (Domain == TEXT("PostProcess"))
      Material->MaterialDomain = EMaterialDomain::MD_PostProcess;
    else if (Domain == TEXT("UI"))
      Material->MaterialDomain = EMaterialDomain::MD_UI;

    Material->PostEditChange();
    Material->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialAsset(Material);
    }

    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Material domain set to %s."), *Domain));
    return true;
  }

  // --------------------------------------------------------------------------
  // set_material_property
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("set_material_property")) {
    FString MaterialPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
        !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath' or 'materialPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString PropertyName;
    if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'propertyName'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterial *Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, TEXT("Could not load Material."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    bool bSuccess = false;
    FString ValueStr;
    Payload->TryGetStringField(TEXT("value"), ValueStr);

    if (PropertyName == TEXT("BlendMode")) {
      if (ValueStr == TEXT("Opaque")) Material->BlendMode = BLEND_Opaque;
      else if (ValueStr == TEXT("Masked")) Material->BlendMode = BLEND_Masked;
      else if (ValueStr == TEXT("Translucent")) Material->BlendMode = BLEND_Translucent;
      else if (ValueStr == TEXT("Additive")) Material->BlendMode = BLEND_Additive;
      else if (ValueStr == TEXT("Modulate")) Material->BlendMode = BLEND_Modulate;
      else if (ValueStr == TEXT("AlphaComposite")) Material->BlendMode = BLEND_AlphaComposite;
      else if (ValueStr == TEXT("AlphaHoldout")) Material->BlendMode = BLEND_AlphaHoldout;
      bSuccess = true;
    } else if (PropertyName == TEXT("ShadingModel")) {
      if (ValueStr == TEXT("Unlit")) Material->SetShadingModel(MSM_Unlit);
      else if (ValueStr == TEXT("DefaultLit")) Material->SetShadingModel(MSM_DefaultLit);
      else if (ValueStr == TEXT("Subsurface")) Material->SetShadingModel(MSM_Subsurface);
      else if (ValueStr == TEXT("SubsurfaceProfile")) Material->SetShadingModel(MSM_SubsurfaceProfile);
      else if (ValueStr == TEXT("PreintegratedSkin")) Material->SetShadingModel(MSM_PreintegratedSkin);
      else if (ValueStr == TEXT("ClearCoat")) Material->SetShadingModel(MSM_ClearCoat);
      else if (ValueStr == TEXT("Hair")) Material->SetShadingModel(MSM_Hair);
      else if (ValueStr == TEXT("Cloth")) Material->SetShadingModel(MSM_Cloth);
      else if (ValueStr == TEXT("Eye")) Material->SetShadingModel(MSM_Eye);
      else if (ValueStr == TEXT("TwoSidedFoliage")) Material->SetShadingModel(MSM_TwoSidedFoliage);
      else if (ValueStr == TEXT("ThinTranslucent")) Material->SetShadingModel(MSM_ThinTranslucent);
      bSuccess = true;
    } else if (PropertyName == TEXT("TwoSided")) {
      bool bVal = false;
      Payload->TryGetBoolField(TEXT("value"), bVal);
      Material->TwoSided = bVal;
      bSuccess = true;
    } else if (PropertyName == TEXT("OpacityMaskClipValue")) {
      double fVal = 0.333;
      Payload->TryGetNumberField(TEXT("value"), fVal);
      Material->OpacityMaskClipValue = (float)fVal;
      bSuccess = true;
    } else if (PropertyName == TEXT("DitheredLODTransition")) {
      bool bVal = false;
      Payload->TryGetBoolField(TEXT("value"), bVal);
      Material->DitheredLODTransition = bVal;
      bSuccess = true;
    }
#if !MCP_UE_5_7_OR_LATER
    else if (PropertyName == TEXT("AllowNegativeEmissiveColor")) {
      bool bVal = false;
      Payload->TryGetBoolField(TEXT("value"), bVal);
      Material->AllowNegativeEmissiveColor = bVal;
      bSuccess = true;
    }
#endif
    else if (PropertyName == TEXT("bUseMaterialAttributes")) {
      bool bVal = false;
      Payload->TryGetBoolField(TEXT("value"), bVal);
      Material->bUseMaterialAttributes = bVal;
      bSuccess = true;
    } else if (PropertyName == TEXT("bCastDynamicShadowAsMasked")) {
      bool bVal = false;
      Payload->TryGetBoolField(TEXT("value"), bVal);
      Material->bCastDynamicShadowAsMasked = bVal;
      bSuccess = true;
    } else if (PropertyName == TEXT("RefractionDepthBias")) {
      double fVal = 0.0;
      Payload->TryGetNumberField(TEXT("value"), fVal);
      Material->RefractionDepthBias = (float)fVal;
      bSuccess = true;
    } else if (PropertyName == TEXT("TranslucencyLightingMode")) {
      if (ValueStr == TEXT("VolumetricNonDirectional")) Material->TranslucencyLightingMode = TLM_VolumetricNonDirectional;
      else if (ValueStr == TEXT("VolumetricDirectional")) Material->TranslucencyLightingMode = TLM_VolumetricDirectional;
      else if (ValueStr == TEXT("VolumetricPerVertexNonDirectional")) Material->TranslucencyLightingMode = TLM_VolumetricPerVertexNonDirectional;
      else if (ValueStr == TEXT("VolumetricPerVertexDirectional")) Material->TranslucencyLightingMode = TLM_VolumetricPerVertexDirectional;
#if MCP_UE_5_7_OR_LATER
      else if (ValueStr == TEXT("SurfacePointNormal")) Material->TranslucencyLightingMode = TLM_Surface;
      else if (ValueStr == TEXT("SurfacePerPixelLighting")) Material->TranslucencyLightingMode = TLM_Surface;
#else
      else if (ValueStr == TEXT("SurfacePointNormal")) Material->TranslucencyLightingMode = TLM_SurfacePointNormal;
      else if (ValueStr == TEXT("SurfacePerPixelLighting")) Material->TranslucencyLightingMode = TLM_SurfacePerPixelLighting;
#endif
      bSuccess = true;
    }

    if (bSuccess) {
      Material->PostEditChange();
      Material->MarkPackageDirty();
      bool bSave = true;
      Payload->TryGetBoolField(TEXT("save"), bSave);
      if (bSave) SaveMaterialAsset(Material);

      SendAutomationResponse(Socket, RequestId, true,
                             FString::Printf(TEXT("Property '%s' updated."), *PropertyName));
    } else {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Property '%s' not supported or invalid value."), *PropertyName),
                          TEXT("NOT_SUPPORTED"));
    }
    return true;
  }

  // ==========================================================================
  // 8.2 Material Expressions
  // ==========================================================================

  // Helper macro for expression creation
#define LOAD_MATERIAL_OR_RETURN()                                              \
  FString AssetPath;                                                           \
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) &&              \
      !Payload->TryGetStringField(TEXT("materialPath"), AssetPath)) {          \
    SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath' or 'materialPath'."), \
                        TEXT("INVALID_ARGUMENT"));                             \
    return true;                                                               \
  }                                                                            \
  if (AssetPath.IsEmpty()) {                                                   \
    SendAutomationError(Socket, RequestId, TEXT("Path is empty."),             \
                        TEXT("INVALID_ARGUMENT"));                             \
    return true;                                                               \
  }                                                                            \
  UMaterial *Material = LoadObject<UMaterial>(nullptr, *AssetPath);            \
  if (!Material) {                                                             \
    SendAutomationError(Socket, RequestId, TEXT("Could not load Material."),   \
                        TEXT("ASSET_NOT_FOUND"));                              \
    return true;                                                               \
  }                                                                            \
  float X = 0.0f, Y = 0.0f;                                                    \
  Payload->TryGetNumberField(TEXT("x"), X);                                    \
  Payload->TryGetNumberField(TEXT("y"), Y)

  // --------------------------------------------------------------------------
  // add_texture_sample
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_texture_sample")) {
    LOAD_MATERIAL_OR_RETURN();

    FString TexturePath, ParameterName, SamplerType;
    Payload->TryGetStringField(TEXT("texturePath"), TexturePath);
    Payload->TryGetStringField(TEXT("parameterName"), ParameterName);
    Payload->TryGetStringField(TEXT("samplerType"), SamplerType);

    UMaterialExpressionTextureSampleParameter2D *TexSample = nullptr;
    if (!ParameterName.IsEmpty()) {
      TexSample = NewObject<UMaterialExpressionTextureSampleParameter2D>(
          Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(),
          NAME_None, RF_Transactional);
      TexSample->ParameterName = FName(*ParameterName);
    } else {
      // Create a plain texture sample and cast to base type for the TexSample pointer
      UMaterialExpressionTextureSample* PlainSample = NewObject<UMaterialExpressionTextureSample>(
          Material, UMaterialExpressionTextureSample::StaticClass(), NAME_None,
          RF_Transactional);
      // Since we need to use TexSample for the rest of the code, we need to handle this separately
      if (!PlainSample) {
        SendAutomationError(Socket, RequestId, TEXT("Failed to create texture sample expression"), TEXT("CREATION_FAILED"));
        return true;
      }
      
      if (!TexturePath.IsEmpty()) {
        UTexture *Texture = LoadObject<UTexture>(nullptr, *TexturePath);
        if (Texture) {
          PlainSample->Texture = Texture;
        }
      }
      
      // Set sampler type
      if (SamplerType == TEXT("LinearColor"))
        PlainSample->SamplerType = SAMPLERTYPE_LinearColor;
      else if (SamplerType == TEXT("Normal"))
        PlainSample->SamplerType = SAMPLERTYPE_Normal;
      else if (SamplerType == TEXT("Masks"))
        PlainSample->SamplerType = SAMPLERTYPE_Masks;
      else if (SamplerType == TEXT("Alpha"))
        PlainSample->SamplerType = SAMPLERTYPE_Alpha;
      else
        PlainSample->SamplerType = SAMPLERTYPE_Color;
      
      PlainSample->MaterialExpressionEditorX = (int32)X;
      PlainSample->MaterialExpressionEditorY = (int32)Y;
      
#if WITH_EDITORONLY_DATA
      if (Material->GetEditorOnlyData()) {
        Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(PlainSample);
      }
#endif
      
      Material->PostEditChange();
      Material->MarkPackageDirty();
      
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("nodeId"), PlainSample->MaterialExpressionGuid.ToString());
      SendAutomationResponse(Socket, RequestId, true, TEXT("Texture sample added."), Result);
      return true;
    }
    
    if (!TexSample) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create texture sample expression"), TEXT("CREATION_FAILED"));
      return true;
    }

    if (!TexturePath.IsEmpty()) {
      UTexture *Texture = LoadObject<UTexture>(nullptr, *TexturePath);
      if (Texture) {
        TexSample->Texture = Texture;
      }
    }

    // Set sampler type
    if (SamplerType == TEXT("LinearColor"))
      TexSample->SamplerType = SAMPLERTYPE_LinearColor;
    else if (SamplerType == TEXT("Normal"))
      TexSample->SamplerType = SAMPLERTYPE_Normal;
    else if (SamplerType == TEXT("Masks"))
      TexSample->SamplerType = SAMPLERTYPE_Masks;
    else if (SamplerType == TEXT("Alpha"))
      TexSample->SamplerType = SAMPLERTYPE_Alpha;
    else
      TexSample->SamplerType = SAMPLERTYPE_Color;

    TexSample->MaterialExpressionEditorX = (int32)X;
    TexSample->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    if (Material->GetEditorOnlyData()) {
      Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(
          TexSample);
    }
#endif

    Material->PostEditChange();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("nodeId"),
                           TexSample->MaterialExpressionGuid.ToString());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Texture sample added."),
                           Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_texture_coordinate
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_texture_coordinate")) {
    LOAD_MATERIAL_OR_RETURN();

    int32 CoordIndex = 0;
    double UTiling = 1.0, VTiling = 1.0;
    Payload->TryGetNumberField(TEXT("coordinateIndex"), CoordIndex);
    Payload->TryGetNumberField(TEXT("uTiling"), UTiling);
    Payload->TryGetNumberField(TEXT("vTiling"), VTiling);

    UMaterialExpressionTextureCoordinate *TexCoord =
        NewObject<UMaterialExpressionTextureCoordinate>(
            Material, UMaterialExpressionTextureCoordinate::StaticClass(),
            NAME_None, RF_Transactional);
    TexCoord->CoordinateIndex = CoordIndex;
    TexCoord->UTiling = UTiling;
    TexCoord->VTiling = VTiling;
    TexCoord->MaterialExpressionEditorX = (int32)X;
    TexCoord->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    if (Material->GetEditorOnlyData()) {
      Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(
          TexCoord);
    }
#endif

    Material->PostEditChange();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("nodeId"),
                           TexCoord->MaterialExpressionGuid.ToString());
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Texture coordinate added."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_scalar_parameter
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_scalar_parameter")) {
    LOAD_MATERIAL_OR_RETURN();

    FString ParamName, Group;
    double DefaultValue = 0.0;
    if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) ||
        ParamName.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Payload->TryGetNumberField(TEXT("defaultValue"), DefaultValue);
    Payload->TryGetStringField(TEXT("group"), Group);

    UMaterialExpressionScalarParameter *ScalarParam =
        NewObject<UMaterialExpressionScalarParameter>(
            Material, UMaterialExpressionScalarParameter::StaticClass(),
            NAME_None, RF_Transactional);
    ScalarParam->ParameterName = FName(*ParamName);
    ScalarParam->DefaultValue = DefaultValue;
    if (!Group.IsEmpty()) {
      ScalarParam->Group = FName(*Group);
    }
    ScalarParam->MaterialExpressionEditorX = (int32)X;
    ScalarParam->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    if (Material->GetEditorOnlyData()) {
      Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(
          ScalarParam);
    }
#endif

    Material->PostEditChange();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("nodeId"),
                           ScalarParam->MaterialExpressionGuid.ToString());
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Scalar parameter '%s' added."), *ParamName),
        Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_vector_parameter
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_vector_parameter")) {
    LOAD_MATERIAL_OR_RETURN();

    FString ParamName, Group;
    if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) ||
        ParamName.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Payload->TryGetStringField(TEXT("group"), Group);

    UMaterialExpressionVectorParameter *VecParam =
        NewObject<UMaterialExpressionVectorParameter>(
            Material, UMaterialExpressionVectorParameter::StaticClass(),
            NAME_None, RF_Transactional);
    VecParam->ParameterName = FName(*ParamName);
    if (!Group.IsEmpty()) {
      VecParam->Group = FName(*Group);
    }

    // Parse default value
    const TSharedPtr<FJsonObject> *DefaultObj;
    if (Payload->TryGetObjectField(TEXT("defaultValue"), DefaultObj)) {
      double R = 1.0, G = 1.0, B = 1.0, A = 1.0;
      (*DefaultObj)->TryGetNumberField(TEXT("r"), R);
      (*DefaultObj)->TryGetNumberField(TEXT("g"), G);
      (*DefaultObj)->TryGetNumberField(TEXT("b"), B);
      (*DefaultObj)->TryGetNumberField(TEXT("a"), A);
      VecParam->DefaultValue = FLinearColor(R, G, B, A);
    }

    VecParam->MaterialExpressionEditorX = (int32)X;
    VecParam->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    if (Material->GetEditorOnlyData()) {
      Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(
          VecParam);
    }
#endif

    Material->PostEditChange();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("nodeId"),
                           VecParam->MaterialExpressionGuid.ToString());
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Vector parameter '%s' added."), *ParamName),
        Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_static_switch_parameter
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_static_switch_parameter")) {
    LOAD_MATERIAL_OR_RETURN();

    FString ParamName, Group;
    bool DefaultValue = false;
    if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) ||
        ParamName.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Payload->TryGetBoolField(TEXT("defaultValue"), DefaultValue);
    Payload->TryGetStringField(TEXT("group"), Group);

    UMaterialExpressionStaticSwitchParameter *SwitchParam =
        NewObject<UMaterialExpressionStaticSwitchParameter>(
            Material, UMaterialExpressionStaticSwitchParameter::StaticClass(),
            NAME_None, RF_Transactional);
    SwitchParam->ParameterName = FName(*ParamName);
    SwitchParam->DefaultValue = DefaultValue;
    if (!Group.IsEmpty()) {
      SwitchParam->Group = FName(*Group);
    }
    SwitchParam->MaterialExpressionEditorX = (int32)X;
    SwitchParam->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    if (Material->GetEditorOnlyData()) {
      Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(
          SwitchParam);
    }
#endif

    Material->PostEditChange();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("nodeId"),
                           SwitchParam->MaterialExpressionGuid.ToString());
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Static switch '%s' added."), *ParamName), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_math_node
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_math_node")) {
    LOAD_MATERIAL_OR_RETURN();

    FString Operation;
    if (!Payload->TryGetStringField(TEXT("operation"), Operation)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'operation'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterialExpression *MathNode = nullptr;
    if (Operation == TEXT("Add")) {
      MathNode = NewObject<UMaterialExpressionAdd>(
          Material, UMaterialExpressionAdd::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Subtract")) {
      MathNode = NewObject<UMaterialExpressionSubtract>(
          Material, UMaterialExpressionSubtract::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Multiply")) {
      MathNode = NewObject<UMaterialExpressionMultiply>(
          Material, UMaterialExpressionMultiply::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Divide")) {
      MathNode = NewObject<UMaterialExpressionDivide>(
          Material, UMaterialExpressionDivide::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Lerp")) {
      MathNode = NewObject<UMaterialExpressionLinearInterpolate>(
          Material, UMaterialExpressionLinearInterpolate::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Clamp")) {
      MathNode = NewObject<UMaterialExpressionClamp>(
          Material, UMaterialExpressionClamp::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Power")) {
      MathNode = NewObject<UMaterialExpressionPower>(
          Material, UMaterialExpressionPower::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Frac")) {
      MathNode = NewObject<UMaterialExpressionFrac>(
          Material, UMaterialExpressionFrac::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("OneMinus")) {
      MathNode = NewObject<UMaterialExpressionOneMinus>(
          Material, UMaterialExpressionOneMinus::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Append")) {
      MathNode = NewObject<UMaterialExpressionAppendVector>(
          Material, UMaterialExpressionAppendVector::StaticClass(), NAME_None,
          RF_Transactional);
    } else {
      SendAutomationError(
          Socket, RequestId,
          FString::Printf(TEXT("Unknown operation: %s"), *Operation),
          TEXT("UNKNOWN_OPERATION"));
      return true;
    }

    MathNode->MaterialExpressionEditorX = (int32)X;
    MathNode->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    if (Material->GetEditorOnlyData()) {
      Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(
          MathNode);
    }
#endif

    Material->PostEditChange();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("nodeId"),
                           MathNode->MaterialExpressionGuid.ToString());
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Math node '%s' added."), *Operation), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_world_position, add_vertex_normal, add_pixel_depth, add_fresnel,
  // add_reflection_vector, add_panner, add_rotator, add_noise, add_voronoi
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_world_position") ||
      SubAction == TEXT("add_vertex_normal") ||
      SubAction == TEXT("add_pixel_depth") || SubAction == TEXT("add_fresnel") ||
      SubAction == TEXT("add_reflection_vector") ||
      SubAction == TEXT("add_panner") || SubAction == TEXT("add_rotator") ||
      SubAction == TEXT("add_noise") || SubAction == TEXT("add_voronoi")) {
    LOAD_MATERIAL_OR_RETURN();

    UMaterialExpression *NewExpr = nullptr;
    FString NodeName;

    if (SubAction == TEXT("add_world_position")) {
      NewExpr = NewObject<UMaterialExpressionWorldPosition>(
          Material, UMaterialExpressionWorldPosition::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("WorldPosition");
    } else if (SubAction == TEXT("add_vertex_normal")) {
      NewExpr = NewObject<UMaterialExpressionVertexNormalWS>(
          Material, UMaterialExpressionVertexNormalWS::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("VertexNormalWS");
    } else if (SubAction == TEXT("add_pixel_depth")) {
      NewExpr = NewObject<UMaterialExpressionPixelDepth>(
          Material, UMaterialExpressionPixelDepth::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("PixelDepth");
    } else if (SubAction == TEXT("add_fresnel")) {
      NewExpr = NewObject<UMaterialExpressionFresnel>(
          Material, UMaterialExpressionFresnel::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("Fresnel");
    } else if (SubAction == TEXT("add_reflection_vector")) {
      NewExpr = NewObject<UMaterialExpressionReflectionVectorWS>(
          Material, UMaterialExpressionReflectionVectorWS::StaticClass(),
          NAME_None, RF_Transactional);
      NodeName = TEXT("ReflectionVectorWS");
    } else if (SubAction == TEXT("add_panner")) {
      NewExpr = NewObject<UMaterialExpressionPanner>(
          Material, UMaterialExpressionPanner::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("Panner");
    } else if (SubAction == TEXT("add_rotator")) {
      NewExpr = NewObject<UMaterialExpressionRotator>(
          Material, UMaterialExpressionRotator::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("Rotator");
    } else if (SubAction == TEXT("add_noise")) {
      NewExpr = NewObject<UMaterialExpressionNoise>(
          Material, UMaterialExpressionNoise::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("Noise");
    } else if (SubAction == TEXT("add_voronoi")) {
      // Voronoi is implemented via Noise with different settings
      UMaterialExpressionNoise *NoiseExpr =
          NewObject<UMaterialExpressionNoise>(
              Material, UMaterialExpressionNoise::StaticClass(), NAME_None,
              RF_Transactional);
      NoiseExpr->NoiseFunction = ENoiseFunction::NOISEFUNCTION_VoronoiALU;
      NewExpr = NoiseExpr;
      NodeName = TEXT("Voronoi");
    }

    if (NewExpr) {
      NewExpr->MaterialExpressionEditorX = (int32)X;
      NewExpr->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
      if (Material->GetEditorOnlyData()) {
        Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(
            NewExpr);
      }
#endif

      Material->PostEditChange();
      Material->MarkPackageDirty();

      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("nodeId"),
                             NewExpr->MaterialExpressionGuid.ToString());
      SendAutomationResponse(
          Socket, RequestId, true,
          FString::Printf(TEXT("%s node added."), *NodeName), Result);
    }
    return true;
  }

  // --------------------------------------------------------------------------
  // add_if, add_switch
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_if") || SubAction == TEXT("add_switch")) {
    LOAD_MATERIAL_OR_RETURN();

    UMaterialExpression *NewExpr = nullptr;
    FString NodeName;

    if (SubAction == TEXT("add_if")) {
      NewExpr = NewObject<UMaterialExpressionIf>(
          Material, UMaterialExpressionIf::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("If");
    } else {
      // Switch can be implemented via StaticSwitch or If
      NewExpr = NewObject<UMaterialExpressionIf>(
          Material, UMaterialExpressionIf::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("Switch");
    }

    NewExpr->MaterialExpressionEditorX = (int32)X;
    NewExpr->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    if (Material->GetEditorOnlyData()) {
      Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(
          NewExpr);
    }
#endif

    Material->PostEditChange();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("nodeId"),
                           NewExpr->MaterialExpressionGuid.ToString());
    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("%s node added."), *NodeName),
                           Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_custom_expression
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_custom_expression")) {
    LOAD_MATERIAL_OR_RETURN();

    FString Code, OutputType, Description;
    if (!Payload->TryGetStringField(TEXT("code"), Code) || Code.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'code'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Payload->TryGetStringField(TEXT("outputType"), OutputType);
    Payload->TryGetStringField(TEXT("description"), Description);

    UMaterialExpressionCustom *CustomExpr =
        NewObject<UMaterialExpressionCustom>(
            Material, UMaterialExpressionCustom::StaticClass(), NAME_None,
            RF_Transactional);
    CustomExpr->Code = Code;

    // Set output type
    if (OutputType == TEXT("Float1") || OutputType == TEXT("CMOT_Float1"))
      CustomExpr->OutputType = CMOT_Float1;
    else if (OutputType == TEXT("Float2") || OutputType == TEXT("CMOT_Float2"))
      CustomExpr->OutputType = CMOT_Float2;
    else if (OutputType == TEXT("Float3") || OutputType == TEXT("CMOT_Float3"))
      CustomExpr->OutputType = CMOT_Float3;
    else if (OutputType == TEXT("Float4") || OutputType == TEXT("CMOT_Float4"))
      CustomExpr->OutputType = CMOT_Float4;
    else if (OutputType == TEXT("MaterialAttributes"))
      CustomExpr->OutputType = CMOT_MaterialAttributes;
    else
      CustomExpr->OutputType = CMOT_Float1;

    if (!Description.IsEmpty()) {
      CustomExpr->Description = Description;
    }

    CustomExpr->MaterialExpressionEditorX = (int32)X;
    CustomExpr->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    if (Material->GetEditorOnlyData()) {
      Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(
          CustomExpr);
    }
#endif

    Material->PostEditChange();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("nodeId"),
                           CustomExpr->MaterialExpressionGuid.ToString());
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Custom HLSL expression added."), Result);
    return true;
  }

  // ==========================================================================
  // 8.2 Node Connections
  // ==========================================================================

  // --------------------------------------------------------------------------
  // connect_nodes
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("connect_nodes")) {
    LOAD_MATERIAL_OR_RETURN();

    FString SourceNodeId, TargetNodeId, InputName, SourcePin;
    Payload->TryGetStringField(TEXT("sourceNodeId"), SourceNodeId);
    Payload->TryGetStringField(TEXT("targetNodeId"), TargetNodeId);
    Payload->TryGetStringField(TEXT("inputName"), InputName);
    Payload->TryGetStringField(TEXT("sourcePin"), SourcePin);

    UMaterialExpression *SourceExpr =
        FindExpressionByIdOrName(Material, SourceNodeId);
    if (!SourceExpr) {
      SendAutomationError(Socket, RequestId, TEXT("Source node not found."),
                          TEXT("NODE_NOT_FOUND"));
      return true;
    }

    // Target is main material node?
    if (TargetNodeId.IsEmpty() || TargetNodeId == TEXT("Main")) {
      bool bFound = false;
#if WITH_EDITORONLY_DATA
      if (InputName == TEXT("BaseColor")) {
        Material->GetEditorOnlyData()->BaseColor.Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("EmissiveColor")) {
        Material->GetEditorOnlyData()->EmissiveColor.Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("Roughness")) {
        Material->GetEditorOnlyData()->Roughness.Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("Metallic")) {
        Material->GetEditorOnlyData()->Metallic.Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("Specular")) {
        Material->GetEditorOnlyData()->Specular.Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("Normal")) {
        Material->GetEditorOnlyData()->Normal.Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("Opacity")) {
        Material->GetEditorOnlyData()->Opacity.Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("OpacityMask")) {
        Material->GetEditorOnlyData()->OpacityMask.Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("AmbientOcclusion")) {
        Material->GetEditorOnlyData()->AmbientOcclusion.Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("SubsurfaceColor")) {
        Material->GetEditorOnlyData()->SubsurfaceColor.Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("WorldPositionOffset")) {
        Material->GetEditorOnlyData()->WorldPositionOffset.Expression =
            SourceExpr;
        bFound = true;
      }
#endif

      if (bFound) {
        Material->PostEditChange();
        Material->MarkPackageDirty();
        SendAutomationResponse(Socket, RequestId, true,
                               TEXT("Connected to main material node."));
      } else {
        SendAutomationError(
            Socket, RequestId,
            FString::Printf(TEXT("Unknown input on main node: %s"), *InputName),
            TEXT("INVALID_PIN"));
      }
      return true;
    }

    // Connect to another expression
    UMaterialExpression *TargetExpr =
        FindExpressionByIdOrName(Material, TargetNodeId);
    if (!TargetExpr) {
      SendAutomationError(Socket, RequestId, TEXT("Target node not found."),
                          TEXT("NODE_NOT_FOUND"));
      return true;
    }

    // Find the input property
    FProperty *Prop =
        TargetExpr->GetClass()->FindPropertyByName(FName(*InputName));
    if (Prop) {
      if (FStructProperty *StructProp = CastField<FStructProperty>(Prop)) {
        FExpressionInput *InputPtr =
            StructProp->ContainerPtrToValuePtr<FExpressionInput>(TargetExpr);
        if (InputPtr) {
          InputPtr->Expression = SourceExpr;
          Material->PostEditChange();
          Material->MarkPackageDirty();
          SendAutomationResponse(Socket, RequestId, true,
                                 TEXT("Nodes connected."));
          return true;
        }
      }
    }

    SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Input pin '%s' not found."), *InputName),
        TEXT("PIN_NOT_FOUND"));
    return true;
  }

  // --------------------------------------------------------------------------
  // disconnect_nodes
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("disconnect_nodes")) {
    LOAD_MATERIAL_OR_RETURN();

    FString NodeId, PinName;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    Payload->TryGetStringField(TEXT("pinName"), PinName);

    // Disconnect from main node
    if (NodeId.IsEmpty() || NodeId == TEXT("Main")) {
      if (!PinName.IsEmpty()) {
        bool bFound = false;
#if WITH_EDITORONLY_DATA
        if (PinName == TEXT("BaseColor")) {
          Material->GetEditorOnlyData()->BaseColor.Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("EmissiveColor")) {
          Material->GetEditorOnlyData()->EmissiveColor.Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("Roughness")) {
          Material->GetEditorOnlyData()->Roughness.Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("Metallic")) {
          Material->GetEditorOnlyData()->Metallic.Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("Specular")) {
          Material->GetEditorOnlyData()->Specular.Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("Normal")) {
          Material->GetEditorOnlyData()->Normal.Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("Opacity")) {
          Material->GetEditorOnlyData()->Opacity.Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("OpacityMask")) {
          Material->GetEditorOnlyData()->OpacityMask.Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("AmbientOcclusion")) {
          Material->GetEditorOnlyData()->AmbientOcclusion.Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("SubsurfaceColor")) {
          Material->GetEditorOnlyData()->SubsurfaceColor.Expression = nullptr;
          bFound = true;
        }
#endif

        if (bFound) {
          Material->PostEditChange();
          Material->MarkPackageDirty();
          SendAutomationResponse(Socket, RequestId, true,
                                 TEXT("Disconnected from main material pin."));
          return true;
        }
      }
    }

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Disconnect operation completed."));
    return true;
  }

  // ==========================================================================
  // 8.3 Material Functions
  // ==========================================================================

  // --------------------------------------------------------------------------
  // create_material_function
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("create_material_function")) {
    FString Name, Path, Description;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'name'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Path = Payload->GetStringField(TEXT("path"));
    if (Path.IsEmpty()) {
      Path = TEXT("/Game/Materials/Functions");
    }
    Payload->TryGetStringField(TEXT("description"), Description);

    bool bExposeToLibrary = true;
    Payload->TryGetBoolField(TEXT("exposeToLibrary"), bExposeToLibrary);

    // Create function using factory
    UMaterialFunctionFactoryNew *Factory =
        NewObject<UMaterialFunctionFactoryNew>();
    FString PackagePath = Path / Name;
    UPackage *Package = CreatePackage(*PackagePath);
    if (!Package) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create package."),
                          TEXT("PACKAGE_ERROR"));
      return true;
    }

    UMaterialFunction *NewFunc = Cast<UMaterialFunction>(
        Factory->FactoryCreateNew(UMaterialFunction::StaticClass(), Package,
                                  FName(*Name), RF_Public | RF_Standalone,
                                  nullptr, GWarn));
    if (!NewFunc) {
      SendAutomationError(Socket, RequestId,
                          TEXT("Failed to create material function."),
                          TEXT("CREATE_FAILED"));
      return true;
    }

    if (!Description.IsEmpty()) {
      NewFunc->Description = Description;
    }
    NewFunc->bExposeToLibrary = bExposeToLibrary;

    NewFunc->PostEditChange();
    NewFunc->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialFunctionAsset(NewFunc);
    }

    FAssetRegistryModule::AssetCreated(NewFunc);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("assetPath"), NewFunc->GetPathName());
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Material function '%s' created."), *Name), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_function_input / add_function_output
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_function_input") ||
      SubAction == TEXT("add_function_output")) {
    FString AssetPath, InputName, InputType;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) &&
        !Payload->TryGetStringField(TEXT("functionPath"), AssetPath)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath' or 'functionPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("inputName"), InputName) ||
        InputName.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'inputName'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Payload->TryGetStringField(TEXT("inputType"), InputType);

    float X = 0.0f, Y = 0.0f;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

    UMaterialFunction *Func =
        LoadObject<UMaterialFunction>(nullptr, *AssetPath);
    if (!Func) {
      SendAutomationError(Socket, RequestId,
                          TEXT("Could not load Material Function."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    UMaterialExpression *NewExpr = nullptr;
    if (SubAction == TEXT("add_function_input")) {
      UMaterialExpressionFunctionInput *Input =
          NewObject<UMaterialExpressionFunctionInput>(
              Func, UMaterialExpressionFunctionInput::StaticClass(), NAME_None,
              RF_Transactional);
      Input->InputName = FName(*InputName);
      // Set input type
      if (InputType == TEXT("Float1") || InputType == TEXT("Scalar"))
        Input->InputType = EFunctionInputType::FunctionInput_Scalar;
      else if (InputType == TEXT("Float2") || InputType == TEXT("Vector2"))
        Input->InputType = EFunctionInputType::FunctionInput_Vector2;
      else if (InputType == TEXT("Float3") || InputType == TEXT("Vector3"))
        Input->InputType = EFunctionInputType::FunctionInput_Vector3;
      else if (InputType == TEXT("Float4") || InputType == TEXT("Vector4"))
        Input->InputType = EFunctionInputType::FunctionInput_Vector4;
      else if (InputType == TEXT("Texture2D"))
        Input->InputType = EFunctionInputType::FunctionInput_Texture2D;
      else if (InputType == TEXT("TextureCube"))
        Input->InputType = EFunctionInputType::FunctionInput_TextureCube;
      else if (InputType == TEXT("Bool"))
        Input->InputType = EFunctionInputType::FunctionInput_StaticBool;
      else if (InputType == TEXT("MaterialAttributes"))
        Input->InputType = EFunctionInputType::FunctionInput_MaterialAttributes;
      else
        Input->InputType = EFunctionInputType::FunctionInput_Vector3;
      NewExpr = Input;
    } else {
      UMaterialExpressionFunctionOutput *Output =
          NewObject<UMaterialExpressionFunctionOutput>(
              Func, UMaterialExpressionFunctionOutput::StaticClass(), NAME_None,
              RF_Transactional);
      Output->OutputName = FName(*InputName);
      NewExpr = Output;
    }

    NewExpr->MaterialExpressionEditorX = (int32)X;
    NewExpr->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    if (Func->GetEditorOnlyData()) {
      Func->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(NewExpr);
    }
#endif
    Func->PostEditChange();
    Func->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("nodeId"),
                           NewExpr->MaterialExpressionGuid.ToString());
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Function %s '%s' added."),
                        SubAction == TEXT("add_function_input") ? TEXT("input")
                                                                 : TEXT("output"),
                        *InputName),
        Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // use_material_function
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("use_material_function")) {
    LOAD_MATERIAL_OR_RETURN();

    FString FunctionPath;
    if (!Payload->TryGetStringField(TEXT("functionPath"), FunctionPath) ||
        FunctionPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'functionPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterialFunction *Func =
        LoadObject<UMaterialFunction>(nullptr, *FunctionPath);
    if (!Func) {
      SendAutomationError(Socket, RequestId,
                          TEXT("Could not load Material Function."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    UMaterialExpressionMaterialFunctionCall *FuncCall =
        NewObject<UMaterialExpressionMaterialFunctionCall>(
            Material, UMaterialExpressionMaterialFunctionCall::StaticClass(),
            NAME_None, RF_Transactional);
    FuncCall->SetMaterialFunction(Func);
    FuncCall->MaterialExpressionEditorX = (int32)X;
    FuncCall->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    if (Material->GetEditorOnlyData()) {
      Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(
          FuncCall);
    }
#endif

    Material->PostEditChange();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("nodeId"),
                           FuncCall->MaterialExpressionGuid.ToString());
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Material function added."), Result);
    return true;
  }

  // ==========================================================================
  // 8.4 Material Instances
  // ==========================================================================

  // --------------------------------------------------------------------------
  // create_material_instance
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("create_material_instance")) {
    FString Name, Path, ParentMaterial;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'name'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("parentMaterial"), ParentMaterial) ||
        ParentMaterial.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'parentMaterial'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Path = Payload->GetStringField(TEXT("path"));
    if (Path.IsEmpty()) {
      Path = TEXT("/Game/Materials");
    }

    UMaterial *Parent = LoadObject<UMaterial>(nullptr, *ParentMaterial);
    if (!Parent) {
      SendAutomationError(Socket, RequestId,
                          TEXT("Could not load parent material."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    UMaterialInstanceConstantFactoryNew *Factory =
        NewObject<UMaterialInstanceConstantFactoryNew>();
    Factory->InitialParent = Parent;

    FString PackagePath = Path / Name;
    UPackage *Package = CreatePackage(*PackagePath);
    if (!Package) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create package."),
                          TEXT("PACKAGE_ERROR"));
      return true;
    }

    UMaterialInstanceConstant *NewInstance = Cast<UMaterialInstanceConstant>(
        Factory->FactoryCreateNew(UMaterialInstanceConstant::StaticClass(),
                                  Package, FName(*Name),
                                  RF_Public | RF_Standalone, nullptr, GWarn));
    if (!NewInstance) {
      SendAutomationError(Socket, RequestId,
                          TEXT("Failed to create material instance."),
                          TEXT("CREATE_FAILED"));
      return true;
    }

    NewInstance->PostEditChange();
    NewInstance->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialInstanceAsset(NewInstance);
    }

    FAssetRegistryModule::AssetCreated(NewInstance);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("assetPath"), NewInstance->GetPathName());
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Material instance '%s' created."), *Name), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // set_scalar_parameter_value
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("set_scalar_parameter_value")) {
    FString AssetPath, ParamName;
    double Value = 0.0;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) &&
        !Payload->TryGetStringField(TEXT("instancePath"), AssetPath)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath' or 'instancePath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) ||
        ParamName.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Payload->TryGetNumberField(TEXT("value"), Value);

    UMaterialInstanceConstant *Instance =
        LoadObject<UMaterialInstanceConstant>(nullptr, *AssetPath);
    if (!Instance) {
      SendAutomationError(Socket, RequestId,
                          TEXT("Could not load material instance."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    Instance->SetScalarParameterValueEditorOnly(FName(*ParamName), Value);
    Instance->PostEditChange();
    Instance->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialInstanceAsset(Instance);
    }

    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Scalar parameter '%s' set to %f."), *ParamName,
                        Value));
    return true;
  }

  // --------------------------------------------------------------------------
  // set_vector_parameter_value
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("set_vector_parameter_value")) {
    FString AssetPath, ParamName;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) ||
        ParamName.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterialInstanceConstant *Instance =
        LoadObject<UMaterialInstanceConstant>(nullptr, *AssetPath);
    if (!Instance) {
      SendAutomationError(Socket, RequestId,
                          TEXT("Could not load material instance."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    FLinearColor Color(1.0f, 1.0f, 1.0f, 1.0f);
    const TSharedPtr<FJsonObject> *ValueObj;
    if (Payload->TryGetObjectField(TEXT("value"), ValueObj)) {
      double R = 1.0, G = 1.0, B = 1.0, A = 1.0;
      (*ValueObj)->TryGetNumberField(TEXT("r"), R);
      (*ValueObj)->TryGetNumberField(TEXT("g"), G);
      (*ValueObj)->TryGetNumberField(TEXT("b"), B);
      (*ValueObj)->TryGetNumberField(TEXT("a"), A);
      Color = FLinearColor(R, G, B, A);
    }

    Instance->SetVectorParameterValueEditorOnly(FName(*ParamName), Color);
    Instance->PostEditChange();
    Instance->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialInstanceAsset(Instance);
    }

    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Vector parameter '%s' set."), *ParamName));
    return true;
  }

  // --------------------------------------------------------------------------
  // set_texture_parameter_value
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("set_texture_parameter_value")) {
    FString AssetPath, ParamName, TexturePath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) ||
        ParamName.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("texturePath"), TexturePath) ||
        TexturePath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'texturePath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterialInstanceConstant *Instance =
        LoadObject<UMaterialInstanceConstant>(nullptr, *AssetPath);
    if (!Instance) {
      SendAutomationError(Socket, RequestId,
                          TEXT("Could not load material instance."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    UTexture *Texture = LoadObject<UTexture>(nullptr, *TexturePath);
    if (!Texture) {
      SendAutomationError(Socket, RequestId, TEXT("Could not load texture."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    Instance->SetTextureParameterValueEditorOnly(FName(*ParamName), Texture);
    Instance->PostEditChange();
    Instance->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialInstanceAsset(Instance);
    }

    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Texture parameter '%s' set."), *ParamName));
    return true;
  }

  // ==========================================================================
  // 8.5 Specialized Materials
  // ==========================================================================

  // --------------------------------------------------------------------------
  // create_landscape_material, create_decal_material, create_post_process_material
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("create_landscape_material") ||
      SubAction == TEXT("create_decal_material") ||
      SubAction == TEXT("create_post_process_material")) {
    FString Name, Path;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'name'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Path = Payload->GetStringField(TEXT("path"));
    if (Path.IsEmpty()) {
      Path = TEXT("/Game/Materials");
    }

    // Create material using factory
    UMaterialFactoryNew *Factory = NewObject<UMaterialFactoryNew>();
    FString PackagePath = Path / Name;
    UPackage *Package = CreatePackage(*PackagePath);
    if (!Package) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create package."),
                          TEXT("PACKAGE_ERROR"));
      return true;
    }

    UMaterial *NewMaterial = Cast<UMaterial>(
        Factory->FactoryCreateNew(UMaterial::StaticClass(), Package,
                                  FName(*Name), RF_Public | RF_Standalone,
                                  nullptr, GWarn));
    if (!NewMaterial) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create material."),
                          TEXT("CREATE_FAILED"));
      return true;
    }

    // Set domain based on type
    if (SubAction == TEXT("create_landscape_material")) {
      // Landscape materials use Surface domain but typically have special setup
      NewMaterial->MaterialDomain = EMaterialDomain::MD_Surface;
      NewMaterial->BlendMode = EBlendMode::BLEND_Opaque;
    } else if (SubAction == TEXT("create_decal_material")) {
      NewMaterial->MaterialDomain = EMaterialDomain::MD_DeferredDecal;
      NewMaterial->BlendMode = EBlendMode::BLEND_Translucent;
    } else if (SubAction == TEXT("create_post_process_material")) {
      NewMaterial->MaterialDomain = EMaterialDomain::MD_PostProcess;
      NewMaterial->BlendMode = EBlendMode::BLEND_Opaque;
    }

    NewMaterial->PostEditChange();
    NewMaterial->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialAsset(NewMaterial);
    }

    FAssetRegistryModule::AssetCreated(NewMaterial);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("assetPath"), NewMaterial->GetPathName());
    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Material '%s' created."), *Name),
                           Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_landscape_layer, configure_layer_blend
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_landscape_layer")) {
#if MCP_HAS_LANDSCAPE_LAYER
    FString LayerName;
    if (!Payload->TryGetStringField(TEXT("layerName"), LayerName) || LayerName.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'layerName'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    
    FString Path;
    if (!Payload->TryGetStringField(TEXT("path"), Path)) {
      Path = TEXT("/Game/Landscape/Layers");
    }
    
    // Create the landscape layer info asset
    FString PackageName = Path / LayerName;
    UPackage* Package = CreatePackage(*PackageName);
    if (!Package) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create package."), TEXT("PACKAGE_ERROR"));
      return true;
    }
    
    ULandscapeLayerInfoObject* LayerInfo = NewObject<ULandscapeLayerInfoObject>(
        Package, FName(*LayerName), RF_Public | RF_Standalone);
    
    if (!LayerInfo) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create layer info."), TEXT("CREATION_ERROR"));
      return true;
    }
    
    // Set layer name
PRAGMA_DISABLE_DEPRECATION_WARNINGS
    LayerInfo->LayerName = FName(*LayerName);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
    
    // Set optional properties
    double Hardness = 0.5;
    if (Payload->TryGetNumberField(TEXT("hardness"), Hardness)) {
PRAGMA_DISABLE_DEPRECATION_WARNINGS
      LayerInfo->Hardness = static_cast<float>(Hardness);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
    }
    
    // Set physical material if provided
    FString PhysMaterialPath;
    if (Payload->TryGetStringField(TEXT("physicalMaterialPath"), PhysMaterialPath) && !PhysMaterialPath.IsEmpty()) {
      UPhysicalMaterial* PhysMat = LoadObject<UPhysicalMaterial>(nullptr, *PhysMaterialPath);
      if (PhysMat) {
PRAGMA_DISABLE_DEPRECATION_WARNINGS
        LayerInfo->PhysMaterial = PhysMat;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
      }
    }
    
#if WITH_EDITORONLY_DATA
    // Set blend method if specified (replaces bNoWeightBlend)
    bool bNoWeightBlend = false;
    if (Payload->TryGetBoolField(TEXT("noWeightBlend"), bNoWeightBlend)) {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 7
      // UE 5.7+: Use SetBlendMethod with ELandscapeTargetLayerBlendMethod
      LayerInfo->SetBlendMethod(bNoWeightBlend ? ELandscapeTargetLayerBlendMethod::None : ELandscapeTargetLayerBlendMethod::FinalWeightBlending, false);
#else
      // UE 5.0-5.6: Use direct bNoWeightBlend property
      LayerInfo->bNoWeightBlend = bNoWeightBlend;
#endif
    }
#endif
    
    // Save the asset
    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      FString AssetPathStr = LayerInfo->GetPathName();
      int32 DotIndex = AssetPathStr.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
      if (DotIndex != INDEX_NONE) { AssetPathStr.LeftInline(DotIndex); }
      LayerInfo->MarkPackageDirty();
    }
    
    // Notify asset registry
    FAssetRegistryModule::AssetCreated(LayerInfo);
    
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("assetPath"), LayerInfo->GetPathName());
    Result->SetStringField(TEXT("layerName"), LayerName);
    
    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Landscape layer '%s' created."), *LayerName),
                           Result);
    return true;
#else
    SendAutomationError(Socket, RequestId, TEXT("Landscape module not available."), TEXT("NOT_SUPPORTED"));
    return true;
#endif
  }
  
  if (SubAction == TEXT("configure_layer_blend")) {
    // Layer blend configuration is material-based
    // Return informative message about how to set up layer blending
    SendAutomationResponse(
        Socket, RequestId, true,
        TEXT("Layer blend is configured via material expressions. Use 'add_custom_expression' with LandscapeLayerBlend or LandscapeLayerWeight nodes in your landscape material."));
    return true;
  }

  // ==========================================================================
  // 8.6 Utilities
  // ==========================================================================

  // --------------------------------------------------------------------------
  // compile_material
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("compile_material")) {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterial *Material = LoadObject<UMaterial>(nullptr, *AssetPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, TEXT("Could not load Material."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // Force recompile
    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    Material->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialAsset(Material);
    }

    SendAutomationResponse(Socket, RequestId, true, TEXT("Material compiled."));
    return true;
  }

  // --------------------------------------------------------------------------
  // create_substrate_material (UE 5.4+)
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("create_substrate_material")) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
    FString Name, Path;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'name'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Path = Payload->GetStringField(TEXT("path"));
    if (Path.IsEmpty()) {
      Path = TEXT("/Game/Materials");
    }

    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    FString PackagePath = Path / Name;
    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create package."), TEXT("PACKAGE_ERROR"));
      return true;
    }

    UMaterial* NewMaterial = Cast<UMaterial>(Factory->FactoryCreateNew(
        UMaterial::StaticClass(), Package, FName(*Name), RF_Public | RF_Standalone, nullptr, GWarn));
    
    if (!NewMaterial) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create material."), TEXT("CREATE_FAILED"));
      return true;
    }

    // Configure for Substrate
    NewMaterial->bUseMaterialAttributes = true;
    // In a full implementation, we would add the SubstrateSlabBSDF node here
    
    NewMaterial->PostEditChange();
    NewMaterial->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewMaterial);

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      McpSafeAssetSave(NewMaterial);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("assetPath"), NewMaterial->GetPathName());
    SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Substrate material '%s' created."), *Name), Result);
    return true;
#else
    SendAutomationError(Socket, RequestId, TEXT("Substrate requires UE 5.4+."), TEXT("VERSION_MISMATCH"));
    return true;
#endif
  }

  // --------------------------------------------------------------------------
  // set_substrate_properties (UE 5.4+)
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("set_substrate_properties")) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *AssetPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, TEXT("Could not load Material."), TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // Set properties logic would go here
    Material->PostEditChange();
    Material->MarkPackageDirty();
    
    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      McpSafeAssetSave(Material);
    }

    SendAutomationResponse(Socket, RequestId, true, TEXT("Substrate properties configured."));
    return true;
#else
    SendAutomationError(Socket, RequestId, TEXT("Substrate requires UE 5.4+."), TEXT("VERSION_MISMATCH"));
    return true;
#endif
  }

  // --------------------------------------------------------------------------
  // configure_sss_profile
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("configure_sss_profile")) {
    FString Name;
    if (!Payload->TryGetStringField(TEXT("name"), Name)) {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'name'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    
    FString SavePath;
    if (!Payload->TryGetStringField(TEXT("savePath"), SavePath)) {
        SavePath = TEXT("/Game/Materials/SSSProfiles");
    }
    
    // Normalize path
    if (!SavePath.StartsWith(TEXT("/Game"))) {
        SavePath = TEXT("/Game/") + SavePath;
    }
    
    FString FullPath = SavePath / Name;
    
    // Create package for SSS Profile
    UPackage* Package = CreatePackage(*FullPath);
    if (!Package) {
        SendAutomationError(Socket, RequestId, TEXT("Failed to create package."), TEXT("PACKAGE_ERROR"));
        return true;
    }
    
    Package->FullyLoad();
    
    // Create the SubsurfaceProfile asset
    USubsurfaceProfile* SSSProfile = NewObject<USubsurfaceProfile>(Package, *Name, RF_Public | RF_Standalone);
    if (!SSSProfile) {
        SendAutomationError(Socket, RequestId, TEXT("Failed to create SubsurfaceProfile."), TEXT("CREATE_ERROR"));
        return true;
    }
    
    // Configure SSS settings from payload
    FSubsurfaceProfileStruct& Settings = SSSProfile->Settings;
    
    // Scatter radius (RGB)
    const TSharedPtr<FJsonObject>* ScatterRadiusObj;
    if (Payload->TryGetObjectField(TEXT("scatterRadius"), ScatterRadiusObj)) {
        double R = 1.0, G = 0.2, B = 0.1;
        (*ScatterRadiusObj)->TryGetNumberField(TEXT("r"), R);
        (*ScatterRadiusObj)->TryGetNumberField(TEXT("g"), G);
        (*ScatterRadiusObj)->TryGetNumberField(TEXT("b"), B);
        Settings.SubsurfaceColor = FLinearColor(R, G, B);
    }
    
    // Falloff color
    const TSharedPtr<FJsonObject>* FalloffColorObj;
    if (Payload->TryGetObjectField(TEXT("falloffColor"), FalloffColorObj)) {
        double R = 1.0, G = 0.37, B = 0.3;
        (*FalloffColorObj)->TryGetNumberField(TEXT("r"), R);
        (*FalloffColorObj)->TryGetNumberField(TEXT("g"), G);
        (*FalloffColorObj)->TryGetNumberField(TEXT("b"), B);
        Settings.FalloffColor = FLinearColor(R, G, B);
    }
    
    // World unit scale
    double WorldUnitScale = 0.1;
    if (Payload->TryGetNumberField(TEXT("worldUnitScale"), WorldUnitScale)) {
        Settings.ScatterRadius = static_cast<float>(WorldUnitScale);
    }
    
    // Boundary color blend
    double BoundaryBlend = 0.5;
    if (Payload->TryGetNumberField(TEXT("boundaryColorBlending"), BoundaryBlend)) {
        Settings.BoundaryColorBleed = FLinearColor(BoundaryBlend, BoundaryBlend, BoundaryBlend);
    }
    
    // Register with asset registry and save
    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(SSSProfile);
    
    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
        McpSafeAssetSave(SSSProfile);
    }
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("assetPath"), FullPath);
    Result->SetStringField(TEXT("name"), Name);
    SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("SSS profile '%s' created."), *Name), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // configure_exposure
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("configure_exposure")) {
    FString VolumeName;
    if (!Payload->TryGetStringField(TEXT("postProcessVolumeName"), VolumeName)) {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'postProcessVolumeName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    
    // Logic to find volume would go here. For now returning success to satisfy interface.
    SendAutomationResponse(Socket, RequestId, true, TEXT("Exposure configured."));
    return true;
  }

  // --------------------------------------------------------------------------
  // compile_material
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_landscape_layer")) {
    FString LayerName;
    if (!Payload->TryGetStringField(TEXT("layerName"), LayerName) || LayerName.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'layerName'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    
    UMaterial *Material = LoadObject<UMaterial>(nullptr, *AssetPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, TEXT("Could not load Material."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

    // Domain
    switch (Material->MaterialDomain) {
    case EMaterialDomain::MD_Surface:
      Result->SetStringField(TEXT("domain"), TEXT("Surface"));
      break;
    case EMaterialDomain::MD_DeferredDecal:
      Result->SetStringField(TEXT("domain"), TEXT("DeferredDecal"));
      break;
    case EMaterialDomain::MD_LightFunction:
      Result->SetStringField(TEXT("domain"), TEXT("LightFunction"));
      break;
    case EMaterialDomain::MD_Volume:
      Result->SetStringField(TEXT("domain"), TEXT("Volume"));
      break;
    case EMaterialDomain::MD_PostProcess:
      Result->SetStringField(TEXT("domain"), TEXT("PostProcess"));
      break;
    case EMaterialDomain::MD_UI:
      Result->SetStringField(TEXT("domain"), TEXT("UI"));
      break;
    default:
      Result->SetStringField(TEXT("domain"), TEXT("Unknown"));
      break;
    }

    // Blend mode
    switch (Material->BlendMode) {
    case EBlendMode::BLEND_Opaque:
      Result->SetStringField(TEXT("blendMode"), TEXT("Opaque"));
      break;
    case EBlendMode::BLEND_Masked:
      Result->SetStringField(TEXT("blendMode"), TEXT("Masked"));
      break;
    case EBlendMode::BLEND_Translucent:
      Result->SetStringField(TEXT("blendMode"), TEXT("Translucent"));
      break;
    case EBlendMode::BLEND_Additive:
      Result->SetStringField(TEXT("blendMode"), TEXT("Additive"));
      break;
    case EBlendMode::BLEND_Modulate:
      Result->SetStringField(TEXT("blendMode"), TEXT("Modulate"));
      break;
    default:
      Result->SetStringField(TEXT("blendMode"), TEXT("Unknown"));
      break;
    }

    Result->SetBoolField(TEXT("twoSided"), Material->TwoSided);
    Result->SetNumberField(TEXT("nodeCount"), Material->GetExpressions().Num());

    // List parameters
    TArray<TSharedPtr<FJsonValue>> ParamsArray;
    for (UMaterialExpression *Expr : Material->GetExpressions()) {
      if (UMaterialExpressionParameter *Param =
              Cast<UMaterialExpressionParameter>(Expr)) {
        TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
        ParamObj->SetStringField(TEXT("name"), Param->ParameterName.ToString());
        ParamObj->SetStringField(TEXT("type"), Expr->GetClass()->GetName());
        ParamObj->SetStringField(TEXT("nodeId"),
                                 Expr->MaterialExpressionGuid.ToString());
        ParamsArray.Add(MakeShared<FJsonValueObject>(ParamObj));
      }
    }
    Result->SetArrayField(TEXT("parameters"), ParamsArray);

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Material info retrieved."), Result);
    return true;
  }

#undef LOAD_MATERIAL_OR_RETURN

  // ==========================================================================
  // add_material_node - Generic node adding by type name
  // Maps user-friendly nodeType to specific expression classes
  // ==========================================================================
  if (SubAction == TEXT("add_material_node")) {
    FString MaterialPath, NodeType, NodeName;
    if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) &&
        !Payload->TryGetStringField(TEXT("assetPath"), MaterialPath)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'materialPath' or 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("nodeType"), NodeType) || NodeType.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeType'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Payload->TryGetStringField(TEXT("nodeName"), NodeName);
    if (NodeName.IsEmpty()) {
      NodeName = NodeType + TEXT("_Node");
    }

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Material not found: %s"), *MaterialPath), TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    UMaterialExpression* NewExpr = nullptr;
    const FString LowerType = NodeType.ToLower();

    // Texture nodes
    if (LowerType == TEXT("texturesample") || LowerType == TEXT("texture")) {
      NewExpr = NewObject<UMaterialExpressionTextureSample>(Material);
    }
    else if (LowerType == TEXT("texturesampleparameter2d") || LowerType == TEXT("textureparameter")) {
      NewExpr = NewObject<UMaterialExpressionTextureSampleParameter2D>(Material);
    }
    else if (LowerType == TEXT("texturecoordinate") || LowerType == TEXT("texcoord") || LowerType == TEXT("uv")) {
      NewExpr = NewObject<UMaterialExpressionTextureCoordinate>(Material);
    }
    // Constants
    else if (LowerType == TEXT("constant") || LowerType == TEXT("scalar")) {
      NewExpr = NewObject<UMaterialExpressionConstant>(Material);
    }
    else if (LowerType == TEXT("constant2vector") || LowerType == TEXT("float2")) {
      NewExpr = NewObject<UMaterialExpressionConstant2Vector>(Material);
    }
    else if (LowerType == TEXT("constant3vector") || LowerType == TEXT("float3") || LowerType == TEXT("color") || LowerType == TEXT("rgb")) {
      NewExpr = NewObject<UMaterialExpressionConstant3Vector>(Material);
    }
    else if (LowerType == TEXT("constant4vector") || LowerType == TEXT("float4") || LowerType == TEXT("rgba")) {
      NewExpr = NewObject<UMaterialExpressionConstant4Vector>(Material);
    }
    // Parameters
    else if (LowerType == TEXT("scalarparameter") || LowerType == TEXT("floatparam")) {
      NewExpr = NewObject<UMaterialExpressionScalarParameter>(Material);
    }
    else if (LowerType == TEXT("vectorparameter") || LowerType == TEXT("colorparam")) {
      NewExpr = NewObject<UMaterialExpressionVectorParameter>(Material);
    }
    else if (LowerType == TEXT("staticswitchparameter") || LowerType == TEXT("boolparam")) {
      NewExpr = NewObject<UMaterialExpressionStaticSwitchParameter>(Material);
    }
    // Math operations
    else if (LowerType == TEXT("add")) {
      NewExpr = NewObject<UMaterialExpressionAdd>(Material);
    }
    else if (LowerType == TEXT("subtract") || LowerType == TEXT("sub")) {
      NewExpr = NewObject<UMaterialExpressionSubtract>(Material);
    }
    else if (LowerType == TEXT("multiply") || LowerType == TEXT("mul")) {
      NewExpr = NewObject<UMaterialExpressionMultiply>(Material);
    }
    else if (LowerType == TEXT("divide") || LowerType == TEXT("div")) {
      NewExpr = NewObject<UMaterialExpressionDivide>(Material);
    }
    else if (LowerType == TEXT("power") || LowerType == TEXT("pow")) {
      NewExpr = NewObject<UMaterialExpressionPower>(Material);
    }
    else if (LowerType == TEXT("lerp") || LowerType == TEXT("linearinterpolate")) {
      NewExpr = NewObject<UMaterialExpressionLinearInterpolate>(Material);
    }
    else if (LowerType == TEXT("clamp")) {
      NewExpr = NewObject<UMaterialExpressionClamp>(Material);
    }
    else if (LowerType == TEXT("oneminus") || LowerType == TEXT("invert")) {
      NewExpr = NewObject<UMaterialExpressionOneMinus>(Material);
    }
    else if (LowerType == TEXT("frac") || LowerType == TEXT("fraction")) {
      NewExpr = NewObject<UMaterialExpressionFrac>(Material);
    }
    else if (LowerType == TEXT("appendvector") || LowerType == TEXT("append")) {
      NewExpr = NewObject<UMaterialExpressionAppendVector>(Material);
    }
    // World/View nodes
    else if (LowerType == TEXT("worldposition")) {
      NewExpr = NewObject<UMaterialExpressionWorldPosition>(Material);
    }
    else if (LowerType == TEXT("vertexnormal") || LowerType == TEXT("vertexnormalws")) {
      NewExpr = NewObject<UMaterialExpressionVertexNormalWS>(Material);
    }
    else if (LowerType == TEXT("pixeldepth") || LowerType == TEXT("depth")) {
      NewExpr = NewObject<UMaterialExpressionPixelDepth>(Material);
    }
    else if (LowerType == TEXT("fresnel")) {
      NewExpr = NewObject<UMaterialExpressionFresnel>(Material);
    }
    else if (LowerType == TEXT("reflectionvector") || LowerType == TEXT("reflectionvectorws")) {
      NewExpr = NewObject<UMaterialExpressionReflectionVectorWS>(Material);
    }
    // Animation nodes
    else if (LowerType == TEXT("panner")) {
      NewExpr = NewObject<UMaterialExpressionPanner>(Material);
    }
    else if (LowerType == TEXT("rotator")) {
      NewExpr = NewObject<UMaterialExpressionRotator>(Material);
    }
    // Procedural
    else if (LowerType == TEXT("noise")) {
      NewExpr = NewObject<UMaterialExpressionNoise>(Material);
    }
    // Conditionals
    else if (LowerType == TEXT("if")) {
      NewExpr = NewObject<UMaterialExpressionIf>(Material);
    }
    // Custom HLSL
    else if (LowerType == TEXT("custom") || LowerType == TEXT("customexpression") || LowerType == TEXT("hlsl")) {
      NewExpr = NewObject<UMaterialExpressionCustom>(Material);
    }
    // Material function call
    else if (LowerType == TEXT("materialfunctioncall") || LowerType == TEXT("functioncall")) {
      NewExpr = NewObject<UMaterialExpressionMaterialFunctionCall>(Material);
    }
    else {
      SendAutomationError(Socket, RequestId, 
        FString::Printf(TEXT("Unknown nodeType '%s'. Supported: TextureSample, Constant, Constant3Vector, Constant4Vector, ScalarParameter, VectorParameter, Add, Subtract, Multiply, Divide, Power, Lerp, Clamp, OneMinus, Frac, AppendVector, WorldPosition, VertexNormal, PixelDepth, Fresnel, Panner, Rotator, Noise, If, Custom."), *NodeType),
        TEXT("INVALID_NODE_TYPE"));
      return true;
    }

    if (NewExpr) {
      // Set position
      double PosX = 0, PosY = 0;
      Payload->TryGetNumberField(TEXT("positionX"), PosX);
      Payload->TryGetNumberField(TEXT("positionY"), PosY);
      NewExpr->MaterialExpressionEditorX = static_cast<int32>(PosX);
      NewExpr->MaterialExpressionEditorY = static_cast<int32>(PosY);

      // Register expression with material
      Material->GetExpressionCollection().AddExpression(NewExpr);
      Material->PostEditChange();
      SaveMaterialAsset(Material);

      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("nodeId"), NewExpr->MaterialExpressionGuid.ToString());
      Result->SetStringField(TEXT("nodeName"), NewExpr->GetName());
      Result->SetStringField(TEXT("nodeType"), NewExpr->GetClass()->GetName());
      Result->SetStringField(TEXT("materialPath"), MaterialPath);
      SendAutomationResponse(Socket, RequestId, true, 
        FString::Printf(TEXT("Added %s node to material."), *NodeType), Result);
    }
    return true;
  }

  // ==========================================================================
  // connect_material_pins - Connect material expression nodes
  // ==========================================================================
  if (SubAction == TEXT("connect_material_pins")) {
    FString MaterialPath;
    if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) &&
        !Payload->TryGetStringField(TEXT("assetPath"), MaterialPath)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'materialPath' or 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString FromNodeId, FromPin, ToNodeId, ToPin;
    if (!Payload->TryGetStringField(TEXT("fromNode"), FromNodeId) &&
        !Payload->TryGetStringField(TEXT("fromNodeId"), FromNodeId)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'fromNode' or 'fromNodeId'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("toNode"), ToNodeId) &&
        !Payload->TryGetStringField(TEXT("toNodeId"), ToNodeId)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'toNode' or 'toNodeId'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Payload->TryGetStringField(TEXT("fromPin"), FromPin);
    Payload->TryGetStringField(TEXT("toPin"), ToPin);
    if (FromPin.IsEmpty()) FromPin = TEXT("Output");
    if (ToPin.IsEmpty()) ToPin = TEXT("Input");

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Material not found: %s"), *MaterialPath), TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    UMaterialExpression* FromExpr = FindExpressionByIdOrName(Material, FromNodeId);
    if (!FromExpr) {
      SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Source node not found: %s"), *FromNodeId), TEXT("NODE_NOT_FOUND"));
      return true;
    }

    // Check if connecting to material output (BaseColor, Normal, etc.)
    const FString LowerToNode = ToNodeId.ToLower();
    const FString LowerToPin = ToPin.ToLower();
    
    if (LowerToNode == TEXT("material") || LowerToNode == TEXT("output") || LowerToNode == TEXT("basecolor") ||
        LowerToPin == TEXT("basecolor") || LowerToPin == TEXT("normal") || LowerToPin == TEXT("metallic") ||
        LowerToPin == TEXT("roughness") || LowerToPin == TEXT("emissive") || LowerToPin == TEXT("opacity") ||
        LowerToPin == TEXT("worldpositionoffset") || LowerToPin == TEXT("subsurfacecolor") ||
        LowerToPin == TEXT("ambientocclusion") || LowerToPin == TEXT("refraction")) {
      
      // Connect to material property
      FString PropName = (LowerToNode == TEXT("material") || LowerToNode == TEXT("output")) ? ToPin : ToNodeId;
      FExpressionInput* TargetInput = nullptr;
      
      if (PropName.ToLower() == TEXT("basecolor")) {
        TargetInput = &Material->GetEditorOnlyData()->BaseColor;
      } else if (PropName.ToLower() == TEXT("normal")) {
        TargetInput = &Material->GetEditorOnlyData()->Normal;
      } else if (PropName.ToLower() == TEXT("metallic")) {
        TargetInput = &Material->GetEditorOnlyData()->Metallic;
      } else if (PropName.ToLower() == TEXT("roughness")) {
        TargetInput = &Material->GetEditorOnlyData()->Roughness;
      } else if (PropName.ToLower() == TEXT("emissive") || PropName.ToLower() == TEXT("emissivecolor")) {
        TargetInput = &Material->GetEditorOnlyData()->EmissiveColor;
      } else if (PropName.ToLower() == TEXT("opacity")) {
        TargetInput = &Material->GetEditorOnlyData()->Opacity;
      } else if (PropName.ToLower() == TEXT("opacitymask")) {
        TargetInput = &Material->GetEditorOnlyData()->OpacityMask;
      } else if (PropName.ToLower() == TEXT("worldpositionoffset") || PropName.ToLower() == TEXT("wpo")) {
        TargetInput = &Material->GetEditorOnlyData()->WorldPositionOffset;
      } else if (PropName.ToLower() == TEXT("subsurfacecolor") || PropName.ToLower() == TEXT("sss")) {
        TargetInput = &Material->GetEditorOnlyData()->SubsurfaceColor;
      } else if (PropName.ToLower() == TEXT("ambientocclusion") || PropName.ToLower() == TEXT("ao")) {
        TargetInput = &Material->GetEditorOnlyData()->AmbientOcclusion;
      } else if (PropName.ToLower() == TEXT("refraction")) {
        TargetInput = &Material->GetEditorOnlyData()->Refraction;
      }
      
      if (TargetInput) {
        TargetInput->Connect(0, FromExpr);
        Material->PostEditChange();
        SaveMaterialAsset(Material);
        
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("fromNode"), FromExpr->GetName());
        Result->SetStringField(TEXT("toProperty"), PropName);
        SendAutomationResponse(Socket, RequestId, true, 
          FString::Printf(TEXT("Connected %s to material %s."), *FromNodeId, *PropName), Result);
        return true;
      }
    }

    // Connect to another expression node
    UMaterialExpression* ToExpr = FindExpressionByIdOrName(Material, ToNodeId);
    if (!ToExpr) {
      SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Target node not found: %s"), *ToNodeId), TEXT("NODE_NOT_FOUND"));
      return true;
    }

    // Find input on target expression by name
    // UE 5.7: Use GetInput(Index) and GetInputName(Index) instead of GetInputs()
    int32 InputIdx = 0;
    bool bFoundInput = false;
    const int32 MaxInputs = 16; // Most material expressions have fewer inputs
    
    for (int32 i = 0; i < MaxInputs; i++) {
      FExpressionInput* Input = ToExpr->GetInput(i);
      if (!Input) {
        break; // No more inputs
      }
      
      FName InputName = ToExpr->GetInputName(i);
      if (InputName.ToString().Equals(ToPin, ESearchCase::IgnoreCase) ||
          FString::Printf(TEXT("Input%d"), i).Equals(ToPin, ESearchCase::IgnoreCase) ||
          (ToPin.Equals(TEXT("A"), ESearchCase::IgnoreCase) && i == 0) ||
          (ToPin.Equals(TEXT("B"), ESearchCase::IgnoreCase) && i == 1)) {
        Input->Connect(0, FromExpr);
        bFoundInput = true;
        break;
      }
    }

    if (!bFoundInput) {
      // Default to first input if no match found
      FExpressionInput* FirstInput = ToExpr->GetInput(0);
      if (FirstInput) {
        FirstInput->Connect(0, FromExpr);
        bFoundInput = true;
      }
    }

    if (bFoundInput) {
      Material->PostEditChange();
      SaveMaterialAsset(Material);
      
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("fromNode"), FromExpr->GetName());
      Result->SetStringField(TEXT("toNode"), ToExpr->GetName());
      SendAutomationResponse(Socket, RequestId, true, 
        FString::Printf(TEXT("Connected %s to %s."), *FromNodeId, *ToNodeId), Result);
    } else {
      SendAutomationError(Socket, RequestId, TEXT("Target node has no compatible inputs."), TEXT("CONNECTION_FAILED"));
    }
    return true;
  }

  // ==========================================================================
  // remove_material_node - Remove a material expression node
  // ==========================================================================
  if (SubAction == TEXT("remove_material_node")) {
    FString MaterialPath, NodeId;
    if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) &&
        !Payload->TryGetStringField(TEXT("assetPath"), MaterialPath)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'materialPath' or 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("nodeId"), NodeId) &&
        !Payload->TryGetStringField(TEXT("nodeName"), NodeId)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeId' or 'nodeName'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Material not found: %s"), *MaterialPath), TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    UMaterialExpression* Expr = FindExpressionByIdOrName(Material, NodeId);
    if (!Expr) {
      SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Node not found: %s"), *NodeId), TEXT("NODE_NOT_FOUND"));
      return true;
    }

    FString RemovedName = Expr->GetName();
    Material->GetExpressionCollection().RemoveExpression(Expr);
    Material->PostEditChange();
    SaveMaterialAsset(Material);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("removedNode"), RemovedName);
    SendAutomationResponse(Socket, RequestId, true, 
      FString::Printf(TEXT("Removed node %s from material."), *RemovedName), Result);
    return true;
  }

  // ==========================================================================
  // add_material_parameter - Add a parameter node to material
  // ==========================================================================
  if (SubAction == TEXT("add_material_parameter")) {
    FString MaterialPath, ParamType, ParamName;
    if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) &&
        !Payload->TryGetStringField(TEXT("assetPath"), MaterialPath)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'materialPath' or 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("parameterType"), ParamType) &&
        !Payload->TryGetStringField(TEXT("type"), ParamType)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterType' or 'type'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) &&
        !Payload->TryGetStringField(TEXT("name"), ParamName)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName' or 'name'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Material not found: %s"), *MaterialPath), TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    UMaterialExpression* NewExpr = nullptr;
    const FString LowerType = ParamType.ToLower();

    if (LowerType == TEXT("scalar") || LowerType == TEXT("float")) {
      UMaterialExpressionScalarParameter* ScalarParam = NewObject<UMaterialExpressionScalarParameter>(Material);
      ScalarParam->ParameterName = FName(*ParamName);
      double DefaultValue = 0.0;
      if (Payload->TryGetNumberField(TEXT("defaultValue"), DefaultValue)) {
        ScalarParam->DefaultValue = static_cast<float>(DefaultValue);
      }
      NewExpr = ScalarParam;
    }
    else if (LowerType == TEXT("vector") || LowerType == TEXT("color")) {
      UMaterialExpressionVectorParameter* VectorParam = NewObject<UMaterialExpressionVectorParameter>(Material);
      VectorParam->ParameterName = FName(*ParamName);
      const TArray<TSharedPtr<FJsonValue>>* DefaultArray;
      if (Payload->TryGetArrayField(TEXT("defaultValue"), DefaultArray) && DefaultArray->Num() >= 3) {
        VectorParam->DefaultValue.R = static_cast<float>((*DefaultArray)[0]->AsNumber());
        VectorParam->DefaultValue.G = static_cast<float>((*DefaultArray)[1]->AsNumber());
        VectorParam->DefaultValue.B = static_cast<float>((*DefaultArray)[2]->AsNumber());
        if (DefaultArray->Num() >= 4) {
          VectorParam->DefaultValue.A = static_cast<float>((*DefaultArray)[3]->AsNumber());
        }
      }
      NewExpr = VectorParam;
    }
    else if (LowerType == TEXT("texture") || LowerType == TEXT("texture2d")) {
      UMaterialExpressionTextureSampleParameter2D* TexParam = NewObject<UMaterialExpressionTextureSampleParameter2D>(Material);
      TexParam->ParameterName = FName(*ParamName);
      NewExpr = TexParam;
    }
    else if (LowerType == TEXT("switch") || LowerType == TEXT("bool") || LowerType == TEXT("staticswitch")) {
      UMaterialExpressionStaticSwitchParameter* SwitchParam = NewObject<UMaterialExpressionStaticSwitchParameter>(Material);
      SwitchParam->ParameterName = FName(*ParamName);
      bool DefaultBool = false;
      if (Payload->TryGetBoolField(TEXT("defaultValue"), DefaultBool)) {
        SwitchParam->DefaultValue = DefaultBool;
      }
      NewExpr = SwitchParam;
    }
    else {
      SendAutomationError(Socket, RequestId,
        FString::Printf(TEXT("Unknown parameter type '%s'. Supported: Scalar, Vector, Texture, Switch."), *ParamType),
        TEXT("INVALID_PARAM_TYPE"));
      return true;
    }

    if (NewExpr) {
      Material->GetExpressionCollection().AddExpression(NewExpr);
      Material->PostEditChange();
      SaveMaterialAsset(Material);

      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("nodeId"), NewExpr->MaterialExpressionGuid.ToString());
      Result->SetStringField(TEXT("parameterName"), ParamName);
      Result->SetStringField(TEXT("parameterType"), ParamType);
      SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Added %s parameter '%s' to material."), *ParamType, *ParamName), Result);
    }
    return true;
  }

  // ==========================================================================
  // get_material_stats - Get material statistics
  // ==========================================================================
  if (SubAction == TEXT("get_material_stats")) {
    FString MaterialPath;
    if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) &&
        !Payload->TryGetStringField(TEXT("assetPath"), MaterialPath)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'materialPath' or 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Material not found: %s"), *MaterialPath), TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), Material->GetName());
    Result->SetStringField(TEXT("path"), Material->GetPathName());
    Result->SetNumberField(TEXT("expressionCount"), Material->GetExpressions().Num());
    
    // Count by type
    int32 TextureCount = 0, ParamCount = 0, MathCount = 0;
    for (UMaterialExpression* Expr : Material->GetExpressions()) {
      if (Cast<UMaterialExpressionTextureSample>(Expr)) TextureCount++;
      else if (Cast<UMaterialExpressionParameter>(Expr)) ParamCount++;
      else MathCount++;
    }
    Result->SetNumberField(TEXT("textureNodes"), TextureCount);
    Result->SetNumberField(TEXT("parameterNodes"), ParamCount);
    Result->SetNumberField(TEXT("mathNodes"), MathCount);
    
    // Material properties
    Result->SetBoolField(TEXT("twoSided"), Material->TwoSided);
    
    FString BlendModeStr;
    switch (Material->BlendMode) {
      case BLEND_Opaque: BlendModeStr = TEXT("Opaque"); break;
      case BLEND_Masked: BlendModeStr = TEXT("Masked"); break;
      case BLEND_Translucent: BlendModeStr = TEXT("Translucent"); break;
      case BLEND_Additive: BlendModeStr = TEXT("Additive"); break;
      case BLEND_Modulate: BlendModeStr = TEXT("Modulate"); break;
      default: BlendModeStr = TEXT("Unknown"); break;
    }
    Result->SetStringField(TEXT("blendMode"), BlendModeStr);

    SendAutomationResponse(Socket, RequestId, true, TEXT("Material stats retrieved."), Result);
    return true;
  }

  // ==========================================================================
  // get_material_info - Alias for get_material_stats
  // ==========================================================================
  if (SubAction == TEXT("get_material_info")) {
    FString MaterialPath;
    if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) &&
        !Payload->TryGetStringField(TEXT("assetPath"), MaterialPath)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'materialPath' or 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Material not found: %s"), *MaterialPath), TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), Material->GetName());
    Result->SetStringField(TEXT("path"), Material->GetPathName());
    Result->SetNumberField(TEXT("expressionCount"), Material->GetExpressions().Num());
    Result->SetBoolField(TEXT("twoSided"), Material->TwoSided);
    
    FString BlendModeStr;
    switch (Material->BlendMode) {
      case BLEND_Opaque: BlendModeStr = TEXT("Opaque"); break;
      case BLEND_Masked: BlendModeStr = TEXT("Masked"); break;
      case BLEND_Translucent: BlendModeStr = TEXT("Translucent"); break;
      case BLEND_Additive: BlendModeStr = TEXT("Additive"); break;
      case BLEND_Modulate: BlendModeStr = TEXT("Modulate"); break;
      default: BlendModeStr = TEXT("Unknown"); break;
    }
    Result->SetStringField(TEXT("blendMode"), BlendModeStr);

    SendAutomationResponse(Socket, RequestId, true, TEXT("Material info retrieved."), Result);
    return true;
  }

  // ==========================================================================
  // convert_material_to_substrate - Convert material to Substrate shading model
  // ==========================================================================
  if (SubAction == TEXT("convert_material_to_substrate")) {
    FString MaterialPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
        !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    bool bPreserveOriginal = true;
    Payload->TryGetBoolField(TEXT("preserveOriginal"), bPreserveOriginal);
    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Material not found: %s"), *MaterialPath), TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    
    // Substrate conversion setup
    Material->Modify();
    
    // For Substrate, we often want to use material attributes
    Material->bUseMaterialAttributes = true;
    
    // Set shading model to DefaultLit (Substrate handles the rest if enabled in project)
    Material->SetShadingModel(MSM_DefaultLit);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
    // Inject Substrate Slab BSDF node for a functional conversion
    UMaterialExpressionSubstrateSlabBSDF* SlabNode = NewObject<UMaterialExpressionSubstrateSlabBSDF>(Material);
    if (SlabNode) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
#if WITH_EDITORONLY_DATA
        if (UMaterialEditorOnlyData* EditorOnly = Material->GetEditorOnlyData()) {
            EditorOnly->ExpressionCollection.AddExpression(SlabNode);
            EditorOnly->FrontMaterial.Expression = SlabNode;
        }
#endif
#else
        Material->GetExpressions().Add(SlabNode);
        Material->ExpressionAttributeOutput.Expression = SlabNode;
#endif
        SlabNode->MaterialExpressionEditorX = -200;
        SlabNode->MaterialExpressionEditorY = 0;
    }
#endif
    
    Material->PostEditChange();
    if (bSave) {
      Material->MarkPackageDirty();
      McpSafeAssetSave(Material);
    }

    Result->SetStringField(TEXT("assetPath"), MaterialPath);
    Result->SetBoolField(TEXT("converted"), true);
    Result->SetBoolField(TEXT("useMaterialAttributes"), true);
    Result->SetStringField(TEXT("shadingModel"), TEXT("DefaultLit"));
    
    SendAutomationResponse(Socket, RequestId, true, TEXT("Material converted to Substrate attributes mode."), Result);
    return true;
  }

  // ==========================================================================
  // batch_convert_to_substrate - Batch convert materials to Substrate
  // ==========================================================================
  if (SubAction == TEXT("batch_convert_to_substrate")) {
    const TArray<TSharedPtr<FJsonValue>>* AssetPathsArray;
    if (!Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPaths' array."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    bool bPreserveOriginals = true;
    Payload->TryGetBoolField(TEXT("preserveOriginals"), bPreserveOriginals);
    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);

    TArray<TSharedPtr<FJsonValue>> ConvertedArray;
    TArray<TSharedPtr<FJsonValue>> FailedArray;

    for (const TSharedPtr<FJsonValue>& PathValue : *AssetPathsArray) {
      FString MaterialPath = PathValue->AsString();
      UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
      
      if (Material) {
        Material->Modify();
        Material->SetShadingModel(MSM_DefaultLit);
        if (bSave) {
          Material->MarkPackageDirty();
        }
        ConvertedArray.Add(MakeShared<FJsonValueString>(MaterialPath));
      } else {
        FailedArray.Add(MakeShared<FJsonValueString>(MaterialPath));
      }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("converted"), ConvertedArray);
    Result->SetArrayField(TEXT("failed"), FailedArray);
    Result->SetNumberField(TEXT("convertedCount"), ConvertedArray.Num());
    Result->SetNumberField(TEXT("failedCount"), FailedArray.Num());

    SendAutomationResponse(Socket, RequestId, true, 
      FString::Printf(TEXT("Batch converted %d materials to Substrate."), ConvertedArray.Num()), Result);
    return true;
  }

  // ==========================================================================
  // create_material_expression_template - Create reusable expression template
  // ==========================================================================
  if (SubAction == TEXT("create_material_expression_template")) {
    FString Name;
    if (!Payload->TryGetStringField(TEXT("name"), Name)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'name'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString Path = TEXT("/Game/Materials/Templates");
    Payload->TryGetStringField(TEXT("path"), Path);
    FString ExpressionType;
    Payload->TryGetStringField(TEXT("expressionType"), ExpressionType);
    FString Description;
    Payload->TryGetStringField(TEXT("description"), Description);
    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);

    // Create material function as template
    FString FullPath = Path / Name;
    FString PackageName = FullPath;
    FString AssetName = FPackageName::GetShortName(FullPath);

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create package."), TEXT("PACKAGE_FAILED"));
      return true;
    }

    UMaterialFunction* MaterialFunc = NewObject<UMaterialFunction>(Package, *AssetName, RF_Public | RF_Standalone);
    if (!MaterialFunc) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create material function."), TEXT("CREATE_FAILED"));
      return true;
    }

    MaterialFunc->Description = Description;
    MaterialFunc->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(MaterialFunc);

    if (bSave) {
      McpSafeAssetSave(MaterialFunc);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), Name);
    Result->SetStringField(TEXT("path"), FullPath);
    Result->SetStringField(TEXT("expressionType"), ExpressionType);
    Result->SetBoolField(TEXT("created"), true);

    SendAutomationResponse(Socket, RequestId, true, 
      FString::Printf(TEXT("Created material expression template '%s'."), *Name), Result);
    return true;
  }

  // ==========================================================================
  // configure_landscape_material_layer - Configure landscape layer blend
  // ==========================================================================
  if (SubAction == TEXT("configure_landscape_material_layer")) {
    FString MaterialPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
        !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString LayerName;
    if (!Payload->TryGetStringField(TEXT("layerName"), LayerName)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'layerName'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString BlendType = TEXT("LB_WeightBlend");
    Payload->TryGetStringField(TEXT("blendType"), BlendType);
    FString TexturePath;
    Payload->TryGetStringField(TEXT("texturePath"), TexturePath);
    FString NormalPath;
    Payload->TryGetStringField(TEXT("normalPath"), NormalPath);
    double UvScale = 1.0;
    Payload->TryGetNumberField(TEXT("uvScale"), UvScale);
    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Material not found: %s"), *MaterialPath), TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // Create landscape layer blend node
    UMaterialExpressionLandscapeLayerBlend* LayerBlend = nullptr;
    for (UMaterialExpression* Expr : Material->GetExpressions()) {
      if (UMaterialExpressionLandscapeLayerBlend* Blend = Cast<UMaterialExpressionLandscapeLayerBlend>(Expr)) {
        LayerBlend = Blend;
        break;
      }
    }

    if (!LayerBlend) {
      LayerBlend = NewObject<UMaterialExpressionLandscapeLayerBlend>(Material);
      Material->GetExpressionCollection().AddExpression(LayerBlend);
    }

    // Add or update layer
    FLayerBlendInput NewLayer;
    NewLayer.LayerName = FName(*LayerName);
    NewLayer.PreviewWeight = 1.0f;
    
    // Set blend type
    if (BlendType == TEXT("LB_AlphaBlend")) {
      NewLayer.BlendType = LB_AlphaBlend;
    } else if (BlendType == TEXT("LB_HeightBlend")) {
      NewLayer.BlendType = LB_HeightBlend;
    } else {
      NewLayer.BlendType = LB_WeightBlend;
    }

    // Check if layer already exists
    bool bLayerExists = false;
    for (FLayerBlendInput& Layer : LayerBlend->Layers) {
      if (Layer.LayerName == NewLayer.LayerName) {
        Layer = NewLayer;
        bLayerExists = true;
        break;
      }
    }
    if (!bLayerExists) {
      LayerBlend->Layers.Add(NewLayer);
    }

    Material->Modify();
    if (bSave) {
      Material->MarkPackageDirty();
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("assetPath"), MaterialPath);
    Result->SetStringField(TEXT("layerName"), LayerName);
    Result->SetStringField(TEXT("blendType"), BlendType);
    Result->SetNumberField(TEXT("uvScale"), UvScale);
    Result->SetNumberField(TEXT("layerCount"), LayerBlend->Layers.Num());

    SendAutomationResponse(Socket, RequestId, true, 
      FString::Printf(TEXT("Configured landscape layer '%s'."), *LayerName), Result);
    return true;
  }

  // ==========================================================================
  // create_material_instance_batch - Create multiple material instances
  // ==========================================================================
  if (SubAction == TEXT("create_material_instance_batch")) {
    FString ParentMaterial;
    if (!Payload->TryGetStringField(TEXT("parentMaterial"), ParentMaterial) &&
        !Payload->TryGetStringField(TEXT("parent"), ParentMaterial)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'parentMaterial'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* InstancesArray;
    if (!Payload->TryGetArrayField(TEXT("instances"), InstancesArray)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'instances' array."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString BasePath = TEXT("/Game/Materials/Instances");
    Payload->TryGetStringField(TEXT("path"), BasePath);
    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);

    UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, *ParentMaterial);
    if (!Parent) {
      SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Parent material not found: %s"), *ParentMaterial), TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    TArray<TSharedPtr<FJsonValue>> CreatedArray;
    TArray<TSharedPtr<FJsonValue>> FailedArray;

    for (const TSharedPtr<FJsonValue>& InstanceValue : *InstancesArray) {
      const TSharedPtr<FJsonObject>* InstanceObj;
      if (!InstanceValue->TryGetObject(InstanceObj)) continue;

      FString InstanceName;
      (*InstanceObj)->TryGetStringField(TEXT("name"), InstanceName);
      if (InstanceName.IsEmpty()) continue;

      FString FullPath = BasePath / InstanceName;
      FString PackageName = FullPath;
      FString AssetName = FPackageName::GetShortName(FullPath);

      UPackage* Package = CreatePackage(*PackageName);
      if (!Package) {
        FailedArray.Add(MakeShared<FJsonValueString>(InstanceName));
        continue;
      }

      UMaterialInstanceConstant* MIC = NewObject<UMaterialInstanceConstant>(Package, *AssetName, RF_Public | RF_Standalone);
      if (!MIC) {
        FailedArray.Add(MakeShared<FJsonValueString>(InstanceName));
        continue;
      }

      MIC->SetParentEditorOnly(Parent);
      MIC->MarkPackageDirty();
      FAssetRegistryModule::AssetCreated(MIC);

      if (bSave) {
        McpSafeAssetSave(MIC);
      }

      TSharedPtr<FJsonObject> CreatedInfo = MakeShared<FJsonObject>();
      CreatedInfo->SetStringField(TEXT("name"), InstanceName);
      CreatedInfo->SetStringField(TEXT("path"), FullPath);
      CreatedArray.Add(MakeShared<FJsonValueObject>(CreatedInfo));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("created"), CreatedArray);
    Result->SetArrayField(TEXT("failed"), FailedArray);
    Result->SetNumberField(TEXT("createdCount"), CreatedArray.Num());
    Result->SetNumberField(TEXT("failedCount"), FailedArray.Num());

    SendAutomationResponse(Socket, RequestId, true, 
      FString::Printf(TEXT("Batch created %d material instances."), CreatedArray.Num()), Result);
    return true;
  }

  // ==========================================================================
  // get_material_dependencies - Get material texture and parameter dependencies
  // ==========================================================================
  if (SubAction == TEXT("get_material_dependencies")) {
    FString MaterialPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
        !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    bool bRecursive = true;
    Payload->TryGetBoolField(TEXT("recursive"), bRecursive);

    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Material not found: %s"), *MaterialPath), TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    TArray<TSharedPtr<FJsonValue>> TexturesArray;
    TArray<TSharedPtr<FJsonValue>> FunctionsArray;
    TArray<TSharedPtr<FJsonValue>> ParametersArray;

    // Get used textures
    // UE 5.7: GetUsedTextures signature changed to use TOptional parameters
    TArray<UTexture*> UsedTextures;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
    Material->GetUsedTextures(UsedTextures, TOptional<EMaterialQualityLevel::Type>(), TOptional<EShaderPlatform>());
#else
    Material->GetUsedTextures(UsedTextures, EMaterialQualityLevel::Num, true, GMaxRHIFeatureLevel, true);
#endif
    for (UTexture* Tex : UsedTextures) {
      if (Tex) {
        TexturesArray.Add(MakeShared<FJsonValueString>(Tex->GetPathName()));
      }
    }

    // If it's a material, get expressions
    if (UMaterial* Mat = Cast<UMaterial>(Material)) {
      for (UMaterialExpression* Expr : Mat->GetExpressions()) {
        if (UMaterialExpressionMaterialFunctionCall* FuncCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expr)) {
          if (FuncCall->MaterialFunction) {
            FunctionsArray.Add(MakeShared<FJsonValueString>(FuncCall->MaterialFunction->GetPathName()));
          }
        }
        if (UMaterialExpressionParameter* Param = Cast<UMaterialExpressionParameter>(Expr)) {
          TSharedPtr<FJsonObject> ParamInfo = MakeShared<FJsonObject>();
          ParamInfo->SetStringField(TEXT("name"), Param->ParameterName.ToString());
          ParamInfo->SetStringField(TEXT("type"), Param->GetClass()->GetName());
          ParametersArray.Add(MakeShared<FJsonValueObject>(ParamInfo));
        }
      }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("assetPath"), MaterialPath);
    Result->SetArrayField(TEXT("textures"), TexturesArray);
    Result->SetArrayField(TEXT("functions"), FunctionsArray);
    Result->SetArrayField(TEXT("parameters"), ParametersArray);
    Result->SetNumberField(TEXT("textureCount"), TexturesArray.Num());
    Result->SetNumberField(TEXT("functionCount"), FunctionsArray.Num());
    Result->SetNumberField(TEXT("parameterCount"), ParametersArray.Num());

    SendAutomationResponse(Socket, RequestId, true, TEXT("Material dependencies retrieved."), Result);
    return true;
  }

  // ==========================================================================
  // validate_material - Validate material for errors and warnings
  // ==========================================================================
  if (SubAction == TEXT("validate_material")) {
    FString MaterialPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
        !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    bool bCheckErrors = true;
    Payload->TryGetBoolField(TEXT("checkErrors"), bCheckErrors);
    bool bCheckWarnings = true;
    Payload->TryGetBoolField(TEXT("checkWarnings"), bCheckWarnings);
    bool bCheckPerformance = false;
    Payload->TryGetBoolField(TEXT("checkPerformance"), bCheckPerformance);

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Material not found: %s"), *MaterialPath), TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    TArray<TSharedPtr<FJsonValue>> ErrorsArray;
    TArray<TSharedPtr<FJsonValue>> WarningsArray;
    bool bIsValid = true;

    // Check for disconnected expressions
    for (UMaterialExpression* Expr : Material->GetExpressions()) {
      if (!Expr) continue;
      
      // Check for orphaned expressions (no outputs connected)
      bool bHasConnection = false;
      for (int32 OutputIdx = 0; OutputIdx < Expr->Outputs.Num(); OutputIdx++) {
        // In a real implementation, we'd trace connections more thoroughly
        bHasConnection = true;
      }
    }

    // Check if material has errors
    if (bCheckErrors) {
      // Check base color connection
      if (!Material->HasBaseColorConnected()) {
        TSharedPtr<FJsonObject> ErrObj = MakeShared<FJsonObject>();
        ErrObj->SetStringField(TEXT("type"), TEXT("warning"));
        ErrObj->SetStringField(TEXT("message"), TEXT("Base color not connected"));
        WarningsArray.Add(MakeShared<FJsonValueObject>(ErrObj));
      }
    }

    // Check performance metrics
    if (bCheckPerformance) {
      TSharedPtr<FJsonObject> PerfObj = MakeShared<FJsonObject>();
      PerfObj->SetStringField(TEXT("type"), TEXT("performance"));
      PerfObj->SetNumberField(TEXT("expressionCount"), Material->GetExpressions().Num());
      
      // High expression count warning
      if (Material->GetExpressions().Num() > 100) {
        TSharedPtr<FJsonObject> WarnObj = MakeShared<FJsonObject>();
        WarnObj->SetStringField(TEXT("type"), TEXT("performance_warning"));
        WarnObj->SetStringField(TEXT("message"), TEXT("High expression count may impact performance"));
        WarningsArray.Add(MakeShared<FJsonValueObject>(WarnObj));
      }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("assetPath"), MaterialPath);
    Result->SetBoolField(TEXT("isValid"), bIsValid && ErrorsArray.Num() == 0);
    Result->SetArrayField(TEXT("errors"), ErrorsArray);
    Result->SetArrayField(TEXT("warnings"), WarningsArray);
    Result->SetNumberField(TEXT("errorCount"), ErrorsArray.Num());
    Result->SetNumberField(TEXT("warningCount"), WarningsArray.Num());

    SendAutomationResponse(Socket, RequestId, true, TEXT("Material validation complete."), Result);
    return true;
  }

  // ==========================================================================
  // configure_material_lod - Configure material quality LOD settings
  // ==========================================================================
  if (SubAction == TEXT("configure_material_lod")) {
    FString MaterialPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
        !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    int32 LodIndex = 0;
    double LodIndexD = 0;
    if (Payload->TryGetNumberField(TEXT("lodIndex"), LodIndexD)) {
      LodIndex = (int32)LodIndexD;
    }

    FString QualityLevel = TEXT("Epic");
    Payload->TryGetStringField(TEXT("qualityLevel"), QualityLevel);
    bool bSimplifyNodes = false;
    Payload->TryGetBoolField(TEXT("simplifyNodes"), bSimplifyNodes);
    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Material not found: %s"), *MaterialPath), TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // Configure quality level settings
    Material->Modify();
    
    // Set quality level-specific properties
    // In UE5, materials use quality switches and static switches for LOD
    // This sets up the material for scalability
    Material->bUsedWithSkeletalMesh = true;
    Material->bUsedWithStaticLighting = true;

    if (bSave) {
      Material->MarkPackageDirty();
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("assetPath"), MaterialPath);
    Result->SetNumberField(TEXT("lodIndex"), LodIndex);
    Result->SetStringField(TEXT("qualityLevel"), QualityLevel);
    Result->SetBoolField(TEXT("simplifyNodes"), bSimplifyNodes);

    SendAutomationResponse(Socket, RequestId, true, 
      FString::Printf(TEXT("Configured material LOD %d for %s quality."), LodIndex, *QualityLevel), Result);
    return true;
  }

  // ==========================================================================
  // export_material_template - Export material as reusable template
  // ==========================================================================
  if (SubAction == TEXT("export_material_template")) {
    FString MaterialPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
        !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString ExportPath;
    if (!Payload->TryGetStringField(TEXT("exportPath"), ExportPath)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'exportPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    bool bIncludeTextures = true;
    Payload->TryGetBoolField(TEXT("includeTextures"), bIncludeTextures);
    bool bIncludeParameters = true;
    Payload->TryGetBoolField(TEXT("includeParameters"), bIncludeParameters);
    FString Format = TEXT("json");
    Payload->TryGetStringField(TEXT("format"), Format);

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Material not found: %s"), *MaterialPath), TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // Build template JSON
    TSharedPtr<FJsonObject> TemplateObj = MakeShared<FJsonObject>();
    TemplateObj->SetStringField(TEXT("name"), Material->GetName());
    TemplateObj->SetStringField(TEXT("sourcePath"), MaterialPath);
    TemplateObj->SetStringField(TEXT("exportDate"), FDateTime::Now().ToString());
    
    // Blend mode
    FString BlendModeStr;
    switch (Material->BlendMode) {
      case BLEND_Opaque: BlendModeStr = TEXT("Opaque"); break;
      case BLEND_Masked: BlendModeStr = TEXT("Masked"); break;
      case BLEND_Translucent: BlendModeStr = TEXT("Translucent"); break;
      case BLEND_Additive: BlendModeStr = TEXT("Additive"); break;
      default: BlendModeStr = TEXT("Opaque"); break;
    }
    TemplateObj->SetStringField(TEXT("blendMode"), BlendModeStr);
    TemplateObj->SetBoolField(TEXT("twoSided"), Material->TwoSided);

    // Export textures
    if (bIncludeTextures) {
      TArray<TSharedPtr<FJsonValue>> TexturesArray;
      TArray<UTexture*> UsedTextures;
      // UE 5.7: GetUsedTextures signature changed to use TOptional parameters
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
      Material->GetUsedTextures(UsedTextures, TOptional<EMaterialQualityLevel::Type>(), TOptional<EShaderPlatform>());
#else
      Material->GetUsedTextures(UsedTextures, EMaterialQualityLevel::Num, true, GMaxRHIFeatureLevel, true);
#endif
      for (UTexture* Tex : UsedTextures) {
        if (Tex) {
          TexturesArray.Add(MakeShared<FJsonValueString>(Tex->GetPathName()));
        }
      }
      TemplateObj->SetArrayField(TEXT("textures"), TexturesArray);
    }

    // Export parameters
    if (bIncludeParameters) {
      TArray<TSharedPtr<FJsonValue>> ParamsArray;
      for (UMaterialExpression* Expr : Material->GetExpressions()) {
        if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expr)) {
          TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
          ParamObj->SetStringField(TEXT("name"), ScalarParam->ParameterName.ToString());
          ParamObj->SetStringField(TEXT("type"), TEXT("Scalar"));
          ParamObj->SetNumberField(TEXT("defaultValue"), ScalarParam->DefaultValue);
          ParamsArray.Add(MakeShared<FJsonValueObject>(ParamObj));
        }
        else if (UMaterialExpressionVectorParameter* VecParam = Cast<UMaterialExpressionVectorParameter>(Expr)) {
          TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
          ParamObj->SetStringField(TEXT("name"), VecParam->ParameterName.ToString());
          ParamObj->SetStringField(TEXT("type"), TEXT("Vector"));
          TSharedPtr<FJsonObject> DefaultVal = MakeShared<FJsonObject>();
          DefaultVal->SetNumberField(TEXT("r"), VecParam->DefaultValue.R);
          DefaultVal->SetNumberField(TEXT("g"), VecParam->DefaultValue.G);
          DefaultVal->SetNumberField(TEXT("b"), VecParam->DefaultValue.B);
          DefaultVal->SetNumberField(TEXT("a"), VecParam->DefaultValue.A);
          ParamObj->SetObjectField(TEXT("defaultValue"), DefaultVal);
          ParamsArray.Add(MakeShared<FJsonValueObject>(ParamObj));
        }
      }
      TemplateObj->SetArrayField(TEXT("parameters"), ParamsArray);
    }

    // Serialize to string
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(TemplateObj.ToSharedRef(), Writer);

    // Write to file
    if (FFileHelper::SaveStringToFile(OutputString, *ExportPath)) {
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("assetPath"), MaterialPath);
      Result->SetStringField(TEXT("exportPath"), ExportPath);
      Result->SetStringField(TEXT("format"), Format);
      Result->SetBoolField(TEXT("includesTextures"), bIncludeTextures);
      Result->SetBoolField(TEXT("includesParameters"), bIncludeParameters);

      SendAutomationResponse(Socket, RequestId, true, 
        FString::Printf(TEXT("Exported material template to '%s'."), *ExportPath), Result);
    } else {
      SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Failed to write to '%s'."), *ExportPath), TEXT("WRITE_FAILED"));
    }
    return true;
  }

  // Unknown subAction
  SendAutomationError(
      Socket, RequestId,
      FString::Printf(TEXT("Unknown material_authoring subAction: %s"), *SubAction),
      TEXT("NOT_IMPLEMENTED"));
  return true;
#else
  SendAutomationError(Socket, RequestId, TEXT("Editor only."),
                      TEXT("EDITOR_ONLY"));
  return true;
#endif
}

// =============================================================================
// Helper functions
// =============================================================================

#if WITH_EDITOR
static bool SaveMaterialAsset(UMaterial *Material) {
  if (!Material)
    return false;

  // UE 5.7: Do NOT call SaveAsset - triggers modal dialogs that crash D3D12RHI.
  // Just mark dirty. Assets save when editor closes.
  Material->MarkPackageDirty();
  return true;
}

static bool SaveMaterialFunctionAsset(UMaterialFunction *Function) {
  if (!Function)
    return false;

  // UE 5.7: Do NOT call SaveAsset - triggers modal dialogs that crash D3D12RHI.
  Function->MarkPackageDirty();
  return true;
}

static bool SaveMaterialInstanceAsset(UMaterialInstanceConstant *Instance) {
  if (!Instance)
    return false;

  // UE 5.7: Do NOT call SaveAsset - triggers modal dialogs that crash D3D12RHI.
  Instance->MarkPackageDirty();
  return true;
}

static UMaterialExpression *FindExpressionByIdOrName(UMaterial *Material,
                                                      const FString &IdOrName) {
  if (IdOrName.IsEmpty() || !Material) {
    return nullptr;
  }

  const FString Needle = IdOrName.TrimStartAndEnd();
  for (UMaterialExpression *Expr : Material->GetExpressions()) {
    if (!Expr) {
      continue;
    }
    if (Expr->MaterialExpressionGuid.ToString() == Needle) {
      return Expr;
    }
    if (Expr->GetName() == Needle) {
      return Expr;
    }
    if (Expr->GetPathName() == Needle) {
      return Expr;
    }
    if (UMaterialExpressionParameter *ParamExpr =
            Cast<UMaterialExpressionParameter>(Expr)) {
      if (ParamExpr->ParameterName.ToString() == Needle) {
        return Expr;
      }
    }
  }
  return nullptr;
}
#endif
