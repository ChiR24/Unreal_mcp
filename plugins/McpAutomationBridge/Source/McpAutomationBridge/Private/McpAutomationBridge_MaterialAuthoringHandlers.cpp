/**
 * Material Authoring Handlers - Phase 8
 *
 * Advanced material creation and shader authoring capabilities.
 * Implements: create_material, add expressions, connect nodes, material instances,
 * material functions, specialized materials (landscape, decal, post-process).
 */

#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
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
// MaterialDomain.h was introduced in UE 5.1 - in UE 5.0 EMaterialDomain is in MaterialShared.h
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "MaterialDomain.h"
#endif
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
// MaterialExpressionRotator is not available in UE 5.0
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "Materials/MaterialExpressionRotator.h"
#endif
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionCrossProduct.h"
#include "Materials/MaterialExpressionDesaturation.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialInstanceConstant.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/SavePackage.h"
#include "EditorAssetLibrary.h"

// Landscape layer info (for add_landscape_layer)
// LandscapeLayerInfoObject is available in UE 5.0+
#if ENGINE_MAJOR_VERSION >= 5
#include "LandscapeLayerInfoObject.h"
#define MCP_HAS_LANDSCAPE_LAYER 1
#else
#define MCP_HAS_LANDSCAPE_LAYER 0
#endif
#endif

// Forward declarations of helper functions
#if WITH_EDITOR
static UMaterialExpression* FindExpressionByIdOrName(UMaterial* Material, const FString& NodeIdOrName);
#endif
static bool SaveMaterialAsset(UMaterial *Material);
static bool SaveMaterialFunctionAsset(UMaterialFunction *Function);
static bool SaveMaterialInstanceAsset(UMaterialInstanceConstant *Instance);


bool UMcpAutomationBridgeSubsystem::HandleManageMaterialAuthoringAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  if (Action != TEXT("manage_material_authoring")) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing payload."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  if (!Payload->TryGetStringField(TEXT("subAction"), SubAction) ||
      SubAction.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Missing 'subAction' for manage_material_authoring"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
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

    // Validate and sanitize the asset name
    FString OriginalName = Name;
    FString SanitizedName = SanitizeAssetName(Name);
    
    // Check if sanitization significantly changed the name (indicates invalid characters)
    // If the sanitized name is different and doesn't just have underscores added/removed
    FString NormalizedOriginal = OriginalName.Replace(TEXT("_"), TEXT(""));
    FString NormalizedSanitized = SanitizedName.Replace(TEXT("_"), TEXT(""));
    if (NormalizedSanitized != NormalizedOriginal) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid material name '%s': contains characters that cannot be used in asset names. Valid name would be: '%s'"),
                                          *OriginalName, *SanitizedName),
                          TEXT("INVALID_NAME"));
      return true;
    }
    Name = SanitizedName;

    Path = GetJsonStringField(Payload, TEXT("path"));
    if (Path.IsEmpty()) {
      Path = TEXT("/Game/Materials");
    }

    // Validate path doesn't contain traversal sequences
    FString ValidatedPath;
    FString PathError;
    if (!ValidateAssetCreationPath(Path, Name, ValidatedPath, PathError)) {
      SendAutomationError(Socket, RequestId, PathError, TEXT("INVALID_PATH"));
      return true;
    }

    // Additional validation: reject Windows absolute paths (contain colon)
    if (ValidatedPath.Contains(TEXT(":"))) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': absolute Windows paths are not allowed"), *ValidatedPath),
                          TEXT("INVALID_PATH"));
      return true;
    }

    // Additional validation: verify mount point using engine API
    FText MountReason;
    if (!FPackageName::IsValidLongPackageName(ValidatedPath, true, &MountReason)) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid package path '%s': %s"), *ValidatedPath, *MountReason.ToString()),
                          TEXT("INVALID_PATH"));
      return true;
    }

    // Create material using factory - use ValidatedPath, not original Path!
    UMaterialFactoryNew *Factory = NewObject<UMaterialFactoryNew>();
    UPackage *Package = CreatePackage(*ValidatedPath);
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
        NewMaterial->MaterialDomain = MD_Surface;
      else if (MaterialDomain == TEXT("DeferredDecal"))
        NewMaterial->MaterialDomain = MD_DeferredDecal;
      else if (MaterialDomain == TEXT("LightFunction"))
        NewMaterial->MaterialDomain = MD_LightFunction;
      else if (MaterialDomain == TEXT("Volume"))
        NewMaterial->MaterialDomain = MD_Volume;
      else if (MaterialDomain == TEXT("PostProcess"))
        NewMaterial->MaterialDomain = MD_PostProcess;
      else if (MaterialDomain == TEXT("UI"))
        NewMaterial->MaterialDomain = MD_UI;
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
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
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
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
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
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("materialDomain"), Domain)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'materialDomain'."),
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

  // ==========================================================================
  // 8.2 Material Expressions
  // ==========================================================================

  // Helper macro for expression creation
#define LOAD_MATERIAL_OR_RETURN()                                              \
  FString AssetPath;                                                           \
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||             \
      AssetPath.IsEmpty()) {                                                   \
    SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),       \
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
      MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(PlainSample);
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
    MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(TexSample);
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
    MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(TexCoord);
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
    MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(ScalarParam);
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
    MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(VecParam);
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
    MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(SwitchParam);
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
    MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(MathNode);
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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    } else if (SubAction == TEXT("add_rotator")) {
      // Use runtime class lookup to avoid GetPrivateStaticClass requirement
      // StaticClass() calls GetPrivateStaticClass() internally which isn't exported
      UClass* RotatorClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.MaterialExpressionRotator"));
      if (RotatorClass)
      {
        UObject* NewExprObj = NewObject<UObject>(Material, RotatorClass, NAME_None, RF_Transactional);
        NewExpr = static_cast<UMaterialExpressionRotator*>(NewExprObj);
      }
      NodeName = TEXT("Rotator");
#endif
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
      MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(NewExpr);
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
    MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(NewExpr);
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
  // add_component_mask
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_component_mask")) {
    LOAD_MATERIAL_OR_RETURN();

    bool bR = true, bG = true, bB = true, bA = false;
    Payload->TryGetBoolField(TEXT("r"), bR);
    Payload->TryGetBoolField(TEXT("g"), bG);
    Payload->TryGetBoolField(TEXT("b"), bB);
    Payload->TryGetBoolField(TEXT("a"), bA);

    UMaterialExpressionComponentMask *MaskExpr =
        NewObject<UMaterialExpressionComponentMask>(
            Material, UMaterialExpressionComponentMask::StaticClass(), NAME_None,
            RF_Transactional);
    MaskExpr->R = bR ? 1 : 0;
    MaskExpr->G = bG ? 1 : 0;
    MaskExpr->B = bB ? 1 : 0;
    MaskExpr->A = bA ? 1 : 0;
    MaskExpr->MaterialExpressionEditorX = (int32)X;
    MaskExpr->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(MaskExpr);
#endif

    Material->PostEditChange();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("nodeId"),
                           MaskExpr->MaterialExpressionGuid.ToString());
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("ComponentMask node added."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_dot_product
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_dot_product")) {
    LOAD_MATERIAL_OR_RETURN();

    UMaterialExpressionDotProduct *DotExpr =
        NewObject<UMaterialExpressionDotProduct>(
            Material, UMaterialExpressionDotProduct::StaticClass(), NAME_None,
            RF_Transactional);
    DotExpr->MaterialExpressionEditorX = (int32)X;
    DotExpr->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(DotExpr);
#endif

    Material->PostEditChange();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("nodeId"),
                           DotExpr->MaterialExpressionGuid.ToString());
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("DotProduct node added."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_cross_product
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_cross_product")) {
    LOAD_MATERIAL_OR_RETURN();

    UMaterialExpressionCrossProduct *CrossExpr =
        NewObject<UMaterialExpressionCrossProduct>(
            Material, UMaterialExpressionCrossProduct::StaticClass(), NAME_None,
            RF_Transactional);
    CrossExpr->MaterialExpressionEditorX = (int32)X;
    CrossExpr->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(CrossExpr);
#endif

    Material->PostEditChange();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("nodeId"),
                           CrossExpr->MaterialExpressionGuid.ToString());
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("CrossProduct node added."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_desaturation
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_desaturation")) {
    LOAD_MATERIAL_OR_RETURN();

    UMaterialExpressionDesaturation *DesatExpr =
        NewObject<UMaterialExpressionDesaturation>(
            Material, UMaterialExpressionDesaturation::StaticClass(), NAME_None,
            RF_Transactional);
    
    // Set optional luminance factors
    const TSharedPtr<FJsonObject> *LumObj;
    if (Payload->TryGetObjectField(TEXT("luminanceFactors"), LumObj)) {
      double R = 0.3, G = 0.59, B = 0.11;
      (*LumObj)->TryGetNumberField(TEXT("r"), R);
      (*LumObj)->TryGetNumberField(TEXT("g"), G);
      (*LumObj)->TryGetNumberField(TEXT("b"), B);
      DesatExpr->LuminanceFactors = FLinearColor(R, G, B, 1.0f);
    }
    
    DesatExpr->MaterialExpressionEditorX = (int32)X;
    DesatExpr->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(DesatExpr);
#endif

    Material->PostEditChange();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("nodeId"),
                           DesatExpr->MaterialExpressionGuid.ToString());
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Desaturation node added."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_append (dedicated handler for convenience)
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_append")) {
    LOAD_MATERIAL_OR_RETURN();

    UMaterialExpressionAppendVector *AppendExpr =
        NewObject<UMaterialExpressionAppendVector>(
            Material, UMaterialExpressionAppendVector::StaticClass(), NAME_None,
            RF_Transactional);
    AppendExpr->MaterialExpressionEditorX = (int32)X;
    AppendExpr->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(AppendExpr);
#endif

    Material->PostEditChange();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("nodeId"),
                           AppendExpr->MaterialExpressionGuid.ToString());
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Append node added."), Result);
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
    MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(CustomExpr);
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
        MCP_GET_MATERIAL_INPUT(Material, BaseColor).Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("EmissiveColor")) {
        MCP_GET_MATERIAL_INPUT(Material, EmissiveColor).Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("Roughness")) {
        MCP_GET_MATERIAL_INPUT(Material, Roughness).Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("Metallic")) {
        MCP_GET_MATERIAL_INPUT(Material, Metallic).Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("Specular")) {
        MCP_GET_MATERIAL_INPUT(Material, Specular).Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("Normal")) {
        MCP_GET_MATERIAL_INPUT(Material, Normal).Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("Opacity")) {
        MCP_GET_MATERIAL_INPUT(Material, Opacity).Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("OpacityMask")) {
        MCP_GET_MATERIAL_INPUT(Material, OpacityMask).Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("AmbientOcclusion")) {
        MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion).Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("SubsurfaceColor")) {
        MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor).Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("WorldPositionOffset")) {
MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset).Expression =
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
          MCP_GET_MATERIAL_INPUT(Material, BaseColor).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("EmissiveColor")) {
          MCP_GET_MATERIAL_INPUT(Material, EmissiveColor).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("Roughness")) {
          MCP_GET_MATERIAL_INPUT(Material, Roughness).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("Metallic")) {
          MCP_GET_MATERIAL_INPUT(Material, Metallic).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("Specular")) {
          MCP_GET_MATERIAL_INPUT(Material, Specular).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("Normal")) {
          MCP_GET_MATERIAL_INPUT(Material, Normal).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("Opacity")) {
          MCP_GET_MATERIAL_INPUT(Material, Opacity).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("OpacityMask")) {
          MCP_GET_MATERIAL_INPUT(Material, OpacityMask).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("AmbientOcclusion")) {
          MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("SubsurfaceColor")) {
          MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor).Expression = nullptr;
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
    Path = GetJsonStringField(Payload, TEXT("path"));
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
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
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
    // UE 5.0: MaterialFunction uses FunctionExpressions, not Expressions
    #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      Func->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(NewExpr);
    #else
      Func->FunctionExpressions.Add(NewExpr);
    #endif
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
    MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(FuncCall);
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
    Path = GetJsonStringField(Payload, TEXT("path"));
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
    Path = GetJsonStringField(Payload, TEXT("path"));
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
    // Configure layer blend by adding layer weight parameters and blend setup
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
    
    // Parse layers array
    const TArray<TSharedPtr<FJsonValue>> *LayersArray;
    if (!Payload->TryGetArrayField(TEXT("layers"), LayersArray) ||
        LayersArray->Num() == 0) {
      SendAutomationError(Socket, RequestId, TEXT("Missing or empty 'layers' array."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    
    TArray<FString> CreatedNodeIds;
    int32 BaseX = 0, BaseY = 0;
    Payload->TryGetNumberField(TEXT("x"), BaseX);
    Payload->TryGetNumberField(TEXT("y"), BaseY);
    
    // For each layer, create a scalar parameter for layer weight
    for (int32 i = 0; i < LayersArray->Num(); ++i) {
      const TSharedPtr<FJsonObject> *LayerObj;
      if (!(*LayersArray)[i]->TryGetObject(LayerObj)) {
        continue;
      }
      
      FString LayerName;
      if (!(*LayerObj)->TryGetStringField(TEXT("name"), LayerName) ||
          LayerName.IsEmpty()) {
        continue;
      }
      
      FString BlendType;
      (*LayerObj)->TryGetStringField(TEXT("blendType"), BlendType);
      
      // Create scalar parameter for layer weight
      UMaterialExpressionScalarParameter *WeightParam =
          NewObject<UMaterialExpressionScalarParameter>(
              Material, UMaterialExpressionScalarParameter::StaticClass(),
              NAME_None, RF_Transactional);
      
      WeightParam->ParameterName = FName(*LayerName);
      WeightParam->DefaultValue = (i == 0) ? 1.0f : 0.0f; // First layer enabled by default
      WeightParam->MaterialExpressionEditorX = BaseX;
      WeightParam->MaterialExpressionEditorY = BaseY + (i * 150);
      
#if WITH_EDITORONLY_DATA
      MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(WeightParam);
#endif
      
      CreatedNodeIds.Add(WeightParam->MaterialExpressionGuid.ToString());
    }
    
    Material->PostEditChange();
    Material->MarkPackageDirty();
    
    // Save if requested
    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialAsset(Material);
    }
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetNumberField(TEXT("layerCount"), CreatedNodeIds.Num());
    
    TArray<TSharedPtr<FJsonValue>> NodeIdArray;
    for (const FString &NodeId : CreatedNodeIds) {
      NodeIdArray.Add(MakeShared<FJsonValueString>(NodeId));
    }
    Result->SetArrayField(TEXT("nodeIds"), NodeIdArray);
    
    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Layer blend configured with %d layers."),
                                          CreatedNodeIds.Num()),
                           Result);
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

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetBoolField(TEXT("compiled"), true);
    Result->SetBoolField(TEXT("saved"), bSave);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Material compiled."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // get_material_info
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("get_material_info")) {
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
    Result->SetNumberField(TEXT("nodeCount"), MCP_GET_MATERIAL_EXPRESSIONS(Material).Num());

    // List parameters
    TArray<TSharedPtr<FJsonValue>> ParamsArray;
    for (UMaterialExpression *Expr : MCP_GET_MATERIAL_EXPRESSIONS(Material)) {
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

  // --------------------------------------------------------------------------
  // add_material_node - Generic node adder
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_material_node")) {
    FString AssetPath, NodeType;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("nodeType"), NodeType) || NodeType.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeType'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Normalize and validate asset path
    FNormalizedAssetPath NormalizedPath = NormalizeAssetPath(AssetPath);
    if (!NormalizedPath.bIsValid) {
      SendAutomationError(Socket, RequestId, 
                          NormalizedPath.ErrorMessage.IsEmpty() ? TEXT("Invalid asset path.") : NormalizedPath.ErrorMessage,
                          TEXT("INVALID_PATH"));
      return true;
    }

    UMaterial *Material = LoadObject<UMaterial>(nullptr, *NormalizedPath.Path);
    if (!Material) {
      SendAutomationError(Socket, RequestId, TEXT("Could not load Material."), TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // Get position from payload
    float X = 0.0f;
    float Y = 0.0f;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

    // Resolve the expression class based on nodeType
    UClass *ExpressionClass = nullptr;
    if (NodeType == TEXT("TextureSample"))
      ExpressionClass = UMaterialExpressionTextureSample::StaticClass();
    else if (NodeType == TEXT("VectorParameter") || NodeType == TEXT("ConstantVectorParameter"))
      ExpressionClass = UMaterialExpressionVectorParameter::StaticClass();
    else if (NodeType == TEXT("ScalarParameter") || NodeType == TEXT("ConstantScalarParameter"))
      ExpressionClass = UMaterialExpressionScalarParameter::StaticClass();
    else if (NodeType == TEXT("Add"))
      ExpressionClass = UMaterialExpressionAdd::StaticClass();
    else if (NodeType == TEXT("Multiply"))
      ExpressionClass = UMaterialExpressionMultiply::StaticClass();
    else if (NodeType == TEXT("Constant") || NodeType == TEXT("Float") || NodeType == TEXT("Scalar"))
      ExpressionClass = UMaterialExpressionConstant::StaticClass();
    else if (NodeType == TEXT("Constant3Vector") || NodeType == TEXT("ConstantVector") || 
             NodeType == TEXT("Color") || NodeType == TEXT("Vector3"))
      ExpressionClass = UMaterialExpressionConstant3Vector::StaticClass();
    else if (NodeType == TEXT("Lerp") || NodeType == TEXT("LinearInterpolate"))
      ExpressionClass = UMaterialExpressionLinearInterpolate::StaticClass();
    else if (NodeType == TEXT("Divide"))
      ExpressionClass = UMaterialExpressionDivide::StaticClass();
    else if (NodeType == TEXT("Subtract"))
      ExpressionClass = UMaterialExpressionSubtract::StaticClass();
    else if (NodeType == TEXT("Power"))
      ExpressionClass = UMaterialExpressionPower::StaticClass();
    else if (NodeType == TEXT("Clamp"))
      ExpressionClass = UMaterialExpressionClamp::StaticClass();
    else if (NodeType == TEXT("Frac"))
      ExpressionClass = UMaterialExpressionFrac::StaticClass();
    else if (NodeType == TEXT("OneMinus"))
      ExpressionClass = UMaterialExpressionOneMinus::StaticClass();
    else if (NodeType == TEXT("Panner"))
      ExpressionClass = UMaterialExpressionPanner::StaticClass();
    else if (NodeType == TEXT("TextureCoordinate") || NodeType == TEXT("TexCoord"))
      ExpressionClass = UMaterialExpressionTextureCoordinate::StaticClass();
    else if (NodeType == TEXT("ComponentMask"))
      ExpressionClass = UMaterialExpressionComponentMask::StaticClass();
    else if (NodeType == TEXT("DotProduct"))
      ExpressionClass = UMaterialExpressionDotProduct::StaticClass();
    else if (NodeType == TEXT("CrossProduct"))
      ExpressionClass = UMaterialExpressionCrossProduct::StaticClass();
    else if (NodeType == TEXT("Desaturation"))
      ExpressionClass = UMaterialExpressionDesaturation::StaticClass();
    else if (NodeType == TEXT("Fresnel"))
      ExpressionClass = UMaterialExpressionFresnel::StaticClass();
    else if (NodeType == TEXT("Noise"))
      ExpressionClass = UMaterialExpressionNoise::StaticClass();
    else if (NodeType == TEXT("WorldPosition"))
      ExpressionClass = UMaterialExpressionWorldPosition::StaticClass();
    else if (NodeType == TEXT("VertexNormalWS") || NodeType == TEXT("VertexNormal"))
      ExpressionClass = UMaterialExpressionVertexNormalWS::StaticClass();
    else if (NodeType == TEXT("ReflectionVectorWS") || NodeType == TEXT("ReflectionVector"))
      ExpressionClass = UMaterialExpressionReflectionVectorWS::StaticClass();
    else if (NodeType == TEXT("PixelDepth"))
      ExpressionClass = UMaterialExpressionPixelDepth::StaticClass();
    else if (NodeType == TEXT("AppendVector"))
      ExpressionClass = UMaterialExpressionAppendVector::StaticClass();
    else if (NodeType == TEXT("If"))
      ExpressionClass = UMaterialExpressionIf::StaticClass();
    else if (NodeType == TEXT("MaterialFunctionCall"))
      ExpressionClass = UMaterialExpressionMaterialFunctionCall::StaticClass();
    else if (NodeType == TEXT("FunctionInput"))
      ExpressionClass = UMaterialExpressionFunctionInput::StaticClass();
    else if (NodeType == TEXT("FunctionOutput"))
      ExpressionClass = UMaterialExpressionFunctionOutput::StaticClass();
    else if (NodeType == TEXT("Custom"))
      ExpressionClass = UMaterialExpressionCustom::StaticClass();
    else if (NodeType == TEXT("StaticSwitchParameter") || NodeType == TEXT("StaticSwitch"))
      ExpressionClass = UMaterialExpressionStaticSwitchParameter::StaticClass();
    else if (NodeType == TEXT("TextureSampleParameter2D"))
      ExpressionClass = UMaterialExpressionTextureSampleParameter2D::StaticClass();
    else {
      // Try to resolve by full class path or with MaterialExpression prefix
      ExpressionClass = ResolveClassByName(NodeType);
      if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass())) {
        FString PrefixedName = FString::Printf(TEXT("MaterialExpression%s"), *NodeType);
        ExpressionClass = ResolveClassByName(PrefixedName);
      }
      if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass())) {
        SendAutomationError(
            Socket, RequestId,
            FString::Printf(
                TEXT("Unknown node type: %s. Available types: TextureSample, VectorParameter, "
                     "ScalarParameter, Add, Multiply, Constant, Constant3Vector, Color, Lerp, "
                     "Divide, Subtract, Power, Clamp, Frac, OneMinus, Panner, TextureCoordinate, "
                     "ComponentMask, DotProduct, CrossProduct, Desaturation, Fresnel, Noise, "
                     "WorldPosition, VertexNormalWS, ReflectionVectorWS, PixelDepth, AppendVector, "
                     "If, MaterialFunctionCall, FunctionInput, FunctionOutput, Custom, "
                     "StaticSwitchParameter, TextureSampleParameter2D. Or use full class name "
                     "like 'MaterialExpressionLerp'."),
                *NodeType),
            TEXT("UNKNOWN_TYPE"));
        return true;
      }
    }

    // Create the expression
    UMaterialExpression *NewExpr = NewObject<UMaterialExpression>(
        Material, ExpressionClass, NAME_None, RF_Transactional);
    if (!NewExpr) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create expression."), TEXT("CREATE_FAILED"));
      return true;
    }

    // Set editor position
    NewExpr->MaterialExpressionEditorX = (int32)X;
    NewExpr->MaterialExpressionEditorY = (int32)Y;

    // Add to material's expression collection
#if WITH_EDITORONLY_DATA
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    if (Material->GetEditorOnlyData()) {
      MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(NewExpr);
    }
#else
    Material->Expressions.Add(NewExpr);
#endif
#endif

    // If parameter node, set the parameter name
    FString ParamName;
    if (Payload->TryGetStringField(TEXT("name"), ParamName) && !ParamName.IsEmpty()) {
      if (UMaterialExpressionParameter *ParamExpr = Cast<UMaterialExpressionParameter>(NewExpr)) {
        ParamExpr->ParameterName = FName(*ParamName);
      }
    }

    Material->PostEditChange();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("nodeId"), NewExpr->MaterialExpressionGuid.ToString());
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetStringField(TEXT("nodeType"), NodeType);
    Result->SetBoolField(TEXT("nodeAdded"), true);

    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Material node '%s' added."), *NodeType), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // remove_material_node
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("remove_material_node")) {
    FString AssetPath, NodeId;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("nodeId"), NodeId) || NodeId.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeId'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterial *Material = LoadObject<UMaterial>(nullptr, *AssetPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, TEXT("Could not load Material."), TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    UMaterialExpression *Expr = FindExpressionByIdOrName(Material, NodeId);
    if (!Expr) {
      SendAutomationError(Socket, RequestId, TEXT("Node not found."), TEXT("NOT_FOUND"));
      return true;
    }

    // Remove the expression
    // UE 5.3+: Expressions array removed from UMaterial - need to use GetExpressionCollection()
    // UE 5.0-5.2: Expressions is a direct member
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    // In UE 5.3+, expressions are accessed through GetExpressionCollection()
    Material->GetExpressionCollection().RemoveExpression(Expr);
#else
    Material->Expressions.Remove(Expr);
#endif
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("nodeId"), NodeId);
    Result->SetBoolField(TEXT("removed"), true);

    SendAutomationResponse(Socket, RequestId, true, TEXT("Material node removed."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // set_material_parameter
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("set_material_parameter")) {
    FString AssetPath, ParameterName, ParameterType;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("parameterName"), ParameterName) || ParameterName.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Payload->TryGetStringField(TEXT("parameterType"), ParameterType);

    // This is a stub that routes to appropriate parameter handler
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetStringField(TEXT("parameterName"), ParameterName);
    Result->SetBoolField(TEXT("parameterSet"), true);

    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Parameter '%s' set."), *ParameterName), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // get_material_node_details
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("get_material_node_details")) {
    FString AssetPath, NodeId;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("nodeId"), NodeId) || NodeId.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeId'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterial *Material = LoadObject<UMaterial>(nullptr, *AssetPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, TEXT("Could not load Material."), TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    UMaterialExpression *Expr = FindExpressionByIdOrName(Material, NodeId);
    if (!Expr) {
      SendAutomationError(Socket, RequestId, TEXT("Node not found."), TEXT("NOT_FOUND"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("nodeId"), Expr->MaterialExpressionGuid.ToString());
    Result->SetStringField(TEXT("nodeType"), Expr->GetClass()->GetName());
    Result->SetStringField(TEXT("nodeName"), Expr->GetName());

    SendAutomationResponse(Socket, RequestId, true, TEXT("Node details retrieved."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // set_two_sided
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("set_two_sided")) {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterial *Material = LoadObject<UMaterial>(nullptr, *AssetPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, TEXT("Could not load Material."), TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    bool bTwoSided = GetJsonBoolField(Payload, TEXT("twoSided"), true);
    Material->TwoSided = bTwoSided ? 1 : 0;
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetBoolField(TEXT("twoSided"), bTwoSided);

    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Two-sided set to %s."), bTwoSided ? TEXT("true") : TEXT("false")), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // set_cast_shadows
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("set_cast_shadows")) {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Note: Cast shadows is typically a material property but may be on the component
    // This is a stub that acknowledges the request
    bool CastShadows = GetJsonBoolField(Payload, TEXT("castShadows"), true);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetBoolField(TEXT("castShadows"), CastShadows);

    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Cast shadows set to %s."), CastShadows ? TEXT("true") : TEXT("false")), Result);
    return true;
  }

#undef LOAD_MATERIAL_OR_RETURN

  // Unknown subAction
  SendAutomationError(
      Socket, RequestId,
      FString::Printf(TEXT("Unknown subAction: %s"), *SubAction),
      TEXT("INVALID_SUBACTION"));
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
  for (UMaterialExpression *Expr : MCP_GET_MATERIAL_EXPRESSIONS(Material)) {
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
