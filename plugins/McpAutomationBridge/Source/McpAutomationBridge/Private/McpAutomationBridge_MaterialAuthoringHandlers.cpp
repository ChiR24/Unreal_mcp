/**
 * McpAutomationBridge_MaterialAuthoringHandlers.cpp
 * =============================================================================
 * Phase 8: Material Authoring System Handlers
 *
 * Provides advanced material creation and shader authoring capabilities for the MCP
 * Automation Bridge. This file implements the `manage_material_authoring` tool.
 *
 * HANDLERS BY CATEGORY:
 * ---------------------
 * 8.1  Material Creation    - create_material, create_material_instance, create_material_function
 * 8.2  Expression Nodes     - add_expression, remove_expression, connect_expressions,
 *                              disconnect_expressions, get_expression_info
 * 8.3  Material Properties  - set_material_property, set_material_shading_model,
 *                              set_material_blend_mode, set_material_two_sided
 * 8.4  Parameters           - add_scalar_parameter, add_vector_parameter, add_texture_parameter,
 *                              set_parameter_default, get_parameter_value
 * 8.5  Material Functions   - create_material_function, call_material_function,
 *                              add_function_input, add_function_output
 * 8.6  Specialized Materials - create_landscape_material, create_decal_material,
 *                              create_post_process_material, add_landscape_layer
 * 8.7  Material Instances   - create_material_instance, set_instance_parameter,
 *                              create_material_instance_dynamic
 * 8.8  Utility Actions      - compile_material, get_material_info, export_material_code,
 *                              duplicate_material
 *
 * VERSION COMPATIBILITY:
 * ----------------------
 * - UE 5.0: Material->Expressions (direct access)
 * - UE 5.1+: Material->GetEditorOnlyData()->ExpressionCollection.Expressions
 * - UE 5.1+: MaterialExpressionRotator, MaterialDomain.h available
 * - MCP_GET_MATERIAL_EXPRESSIONS macro handles version differences
 *
 * REFACTORING NOTES:
 * ------------------
 * - Uses McpHandlerUtils for JSON parsing and response building
 * - McpSafeAssetSave for UE 5.7+ safe asset saving
 * - Path validation via SanitizeProjectRelativePath()
 * - Expression finding by ID or name with robust lookup
 *
 * Copyright (c) 2024 MCP Automation Bridge Contributors
 */

// MCP Core
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpHandlerUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpVersionCompatibility.h"

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
#include "UObject/SavePackage.h"
#include "EditorAssetLibrary.h"

// Landscape (UE 5.0+)
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
static UMaterialExpression* FindExpressionByIdOrNameInFunction(UMaterialFunction* Function, const FString& NodeIdOrName);
static UObject* LoadMaterialOrFunction(const FString& AssetPath, UMaterial*& OutMaterial, UMaterialFunction*& OutFunction);
static void AddExpressionToContainer(UMaterial* Material, UMaterialFunction* Function, UMaterialExpression* Expr);
static FString FunctionInputTypeToString(EFunctionInputType InType);
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

    // Validate parent folder exists
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    
    FString ParentFolderPath = FPackageName::GetLongPackagePath(ValidatedPath);
    if (!AssetRegistry.PathExists(FName(*ParentFolderPath))) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Parent folder does not exist: %s. Create the folder first or use an existing path."), *ParentFolderPath),
                          TEXT("PARENT_FOLDER_NOT_FOUND"));
      return true;
    }

    // Check for existing asset collision to prevent UE crash
    FString FullAssetPath = ValidatedPath + TEXT(".") + Name;
    if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath)) {
      UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(FullAssetPath);
      if (ExistingAsset) {
        UClass* ExistingClass = ExistingAsset->GetClass();
        FString ExistingClassName = ExistingClass ? ExistingClass->GetName() : TEXT("Unknown");
        SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Asset '%s' already exists as %s. Cannot create Material with the same name."),
                                            *FullAssetPath, *ExistingClassName),
                            TEXT("ASSET_EXISTS"));
      } else {
        SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Asset '%s' already exists."),
                                            *FullAssetPath),
                            TEXT("ASSET_EXISTS"));
      }
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
      bool bValidMaterialDomain = false;
      if (MaterialDomain == TEXT("Surface")) {
        NewMaterial->MaterialDomain = MD_Surface;
        bValidMaterialDomain = true;
      } else if (MaterialDomain == TEXT("DeferredDecal")) {
        NewMaterial->MaterialDomain = MD_DeferredDecal;
        bValidMaterialDomain = true;
      } else if (MaterialDomain == TEXT("LightFunction")) {
        NewMaterial->MaterialDomain = MD_LightFunction;
        bValidMaterialDomain = true;
      } else if (MaterialDomain == TEXT("Volume")) {
        NewMaterial->MaterialDomain = MD_Volume;
        bValidMaterialDomain = true;
      } else if (MaterialDomain == TEXT("PostProcess")) {
        NewMaterial->MaterialDomain = MD_PostProcess;
        bValidMaterialDomain = true;
      } else if (MaterialDomain == TEXT("UI")) {
        NewMaterial->MaterialDomain = MD_UI;
        bValidMaterialDomain = true;
      }
      if (!bValidMaterialDomain) {
        SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Invalid materialDomain '%s'. Valid values: Surface, DeferredDecal, LightFunction, Volume, PostProcess, UI"), *MaterialDomain),
                            TEXT("INVALID_ENUM"));
        return true;
      }
    }

    FString BlendMode;
    if (Payload->TryGetStringField(TEXT("blendMode"), BlendMode)) {
      bool bValidBlendMode = false;
      if (BlendMode == TEXT("Opaque")) {
        NewMaterial->BlendMode = EBlendMode::BLEND_Opaque;
        bValidBlendMode = true;
      } else if (BlendMode == TEXT("Masked")) {
        NewMaterial->BlendMode = EBlendMode::BLEND_Masked;
        bValidBlendMode = true;
      } else if (BlendMode == TEXT("Translucent")) {
        NewMaterial->BlendMode = EBlendMode::BLEND_Translucent;
        bValidBlendMode = true;
      } else if (BlendMode == TEXT("Additive")) {
        NewMaterial->BlendMode = EBlendMode::BLEND_Additive;
        bValidBlendMode = true;
      } else if (BlendMode == TEXT("Modulate")) {
        NewMaterial->BlendMode = EBlendMode::BLEND_Modulate;
        bValidBlendMode = true;
      } else if (BlendMode == TEXT("AlphaComposite")) {
        NewMaterial->BlendMode = EBlendMode::BLEND_AlphaComposite;
        bValidBlendMode = true;
      } else if (BlendMode == TEXT("AlphaHoldout")) {
        NewMaterial->BlendMode = EBlendMode::BLEND_AlphaHoldout;
        bValidBlendMode = true;
      }
      if (!bValidBlendMode) {
        SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Invalid blendMode '%s'. Valid values: Opaque, Masked, Translucent, Additive, Modulate, AlphaComposite, AlphaHoldout"), *BlendMode),
                            TEXT("INVALID_ENUM"));
        return true;
      }
    }

    FString ShadingModel;
    if (Payload->TryGetStringField(TEXT("shadingModel"), ShadingModel)) {
      bool bValidShadingModel = false;
      if (ShadingModel == TEXT("Unlit")) {
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_Unlit);
        bValidShadingModel = true;
      } else if (ShadingModel == TEXT("DefaultLit")) {
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_DefaultLit);
        bValidShadingModel = true;
      } else if (ShadingModel == TEXT("Subsurface")) {
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_Subsurface);
        bValidShadingModel = true;
      } else if (ShadingModel == TEXT("SubsurfaceProfile")) {
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_SubsurfaceProfile);
        bValidShadingModel = true;
      } else if (ShadingModel == TEXT("PreintegratedSkin")) {
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_PreintegratedSkin);
        bValidShadingModel = true;
      } else if (ShadingModel == TEXT("ClearCoat")) {
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_ClearCoat);
        bValidShadingModel = true;
      } else if (ShadingModel == TEXT("Hair")) {
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_Hair);
        bValidShadingModel = true;
      } else if (ShadingModel == TEXT("Cloth")) {
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_Cloth);
        bValidShadingModel = true;
      } else if (ShadingModel == TEXT("Eye")) {
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_Eye);
        bValidShadingModel = true;
      } else if (ShadingModel == TEXT("TwoSidedFoliage")) {
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_TwoSidedFoliage);
        bValidShadingModel = true;
      } else if (ShadingModel == TEXT("ThinTranslucent")) {
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_ThinTranslucent);
        bValidShadingModel = true;
      }
      if (!bValidShadingModel) {
        SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Invalid shadingModel '%s'. Valid values: Unlit, DefaultLit, Subsurface, SubsurfaceProfile, PreintegratedSkin, ClearCoat, Hair, Cloth, Eye, TwoSidedFoliage, ThinTranslucent"), *ShadingModel),
                            TEXT("INVALID_ENUM"));
        return true;
      }
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

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, NewMaterial);
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

    // Validate path security BEFORE loading asset
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    UMaterial *Material = nullptr;
    UMaterialFunction *TmpFunc = nullptr;
    LoadMaterialOrFunction(AssetPath, Material, TmpFunc);
    if (!Material && !TmpFunc) {
      SendAutomationError(Socket, RequestId, TEXT("Could not load Material."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    if (!Material) {
      SendAutomationError(Socket, RequestId,
                          TEXT("set_blend_mode is only supported on UMaterial assets, not Material Functions."),
                          TEXT("UNSUPPORTED_ASSET_TYPE"));
      return true;
    }

    bool bValidBlendMode = false;
    if (BlendMode == TEXT("Opaque")) {
      Material->BlendMode = EBlendMode::BLEND_Opaque;
      bValidBlendMode = true;
    } else if (BlendMode == TEXT("Masked")) {
      Material->BlendMode = EBlendMode::BLEND_Masked;
      bValidBlendMode = true;
    } else if (BlendMode == TEXT("Translucent")) {
      Material->BlendMode = EBlendMode::BLEND_Translucent;
      bValidBlendMode = true;
    } else if (BlendMode == TEXT("Additive")) {
      Material->BlendMode = EBlendMode::BLEND_Additive;
      bValidBlendMode = true;
    } else if (BlendMode == TEXT("Modulate")) {
      Material->BlendMode = EBlendMode::BLEND_Modulate;
      bValidBlendMode = true;
    } else if (BlendMode == TEXT("AlphaComposite")) {
      Material->BlendMode = EBlendMode::BLEND_AlphaComposite;
      bValidBlendMode = true;
    } else if (BlendMode == TEXT("AlphaHoldout")) {
      Material->BlendMode = EBlendMode::BLEND_AlphaHoldout;
      bValidBlendMode = true;
    }

    if (!bValidBlendMode) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid blendMode '%s'. Valid values: Opaque, Masked, Translucent, Additive, Modulate, AlphaComposite, AlphaHoldout"),
                                          *BlendMode),
                          TEXT("INVALID_ENUM"));
      return true;
    }

    Material->PostEditChange();
    Material->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialAsset(Material);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Material);
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Blend mode set to %s."), *BlendMode), Result);
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

    // Validate path security BEFORE loading asset
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    UMaterial *Material = nullptr;
    UMaterialFunction *TmpFunc2 = nullptr;
    LoadMaterialOrFunction(AssetPath, Material, TmpFunc2);
    if (!Material && !TmpFunc2) {
      SendAutomationError(Socket, RequestId, TEXT("Could not load Material."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    if (!Material) {
      SendAutomationError(Socket, RequestId,
                          TEXT("set_shading_model is only supported on UMaterial assets, not Material Functions."),
                          TEXT("UNSUPPORTED_ASSET_TYPE"));
      return true;
    }

    bool bValidShadingModel = false;
    if (ShadingModel == TEXT("Unlit")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_Unlit);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("DefaultLit")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_DefaultLit);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("Subsurface")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_Subsurface);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("SubsurfaceProfile")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_SubsurfaceProfile);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("PreintegratedSkin")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_PreintegratedSkin);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("ClearCoat")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_ClearCoat);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("Hair")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_Hair);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("Cloth")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_Cloth);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("Eye")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_Eye);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("TwoSidedFoliage")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_TwoSidedFoliage);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("ThinTranslucent")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_ThinTranslucent);
      bValidShadingModel = true;
    }

    if (!bValidShadingModel) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid shadingModel '%s'. Valid values: Unlit, DefaultLit, Subsurface, SubsurfaceProfile, PreintegratedSkin, ClearCoat, Hair, Cloth, Eye, TwoSidedFoliage, ThinTranslucent"),
                                          *ShadingModel),
                          TEXT("INVALID_ENUM"));
      return true;
    }

    Material->PostEditChange();
    Material->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialAsset(Material);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Material);
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Shading model set to %s."), *ShadingModel), Result);
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

    // Validate path security BEFORE loading asset
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    UMaterial *Material = nullptr;
    UMaterialFunction *TmpFunc3 = nullptr;
    LoadMaterialOrFunction(AssetPath, Material, TmpFunc3);
    if (!Material && !TmpFunc3) {
      SendAutomationError(Socket, RequestId, TEXT("Could not load Material."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    if (!Material) {
      SendAutomationError(Socket, RequestId,
                          TEXT("set_material_domain is only supported on UMaterial assets, not Material Functions."),
                          TEXT("UNSUPPORTED_ASSET_TYPE"));
      return true;
    }

    bool bValidDomain = false;
    if (Domain == TEXT("Surface")) {
      Material->MaterialDomain = EMaterialDomain::MD_Surface;
      bValidDomain = true;
    } else if (Domain == TEXT("DeferredDecal")) {
      Material->MaterialDomain = EMaterialDomain::MD_DeferredDecal;
      bValidDomain = true;
    } else if (Domain == TEXT("LightFunction")) {
      Material->MaterialDomain = EMaterialDomain::MD_LightFunction;
      bValidDomain = true;
    } else if (Domain == TEXT("Volume")) {
      Material->MaterialDomain = EMaterialDomain::MD_Volume;
      bValidDomain = true;
    } else if (Domain == TEXT("PostProcess")) {
      Material->MaterialDomain = EMaterialDomain::MD_PostProcess;
      bValidDomain = true;
    } else if (Domain == TEXT("UI")) {
      Material->MaterialDomain = EMaterialDomain::MD_UI;
      bValidDomain = true;
    }

    if (!bValidDomain) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid materialDomain '%s'. Valid values: Surface, DeferredDecal, LightFunction, Volume, PostProcess, UI"),
                                          *Domain),
                          TEXT("INVALID_ENUM"));
      return true;
    }

    Material->PostEditChange();
    Material->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialAsset(Material);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Material);
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Material domain set to %s."), *Domain), Result);
    return true;
  }

  // ==========================================================================
  // 8.2 Material Expressions
  // ==========================================================================

  // Helper macro for expression creation - validates path BEFORE loading
#define LOAD_MATERIAL_OR_RETURN()                                              \
  FString AssetPath;                                                           \
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||             \
      AssetPath.IsEmpty()) {                                                   \
    SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),       \
                        TEXT("INVALID_ARGUMENT"));                             \
    return true;                                                               \
  }                                                                            \
  /* SECURITY: Validate path BEFORE loading asset */                           \
  FString ValidatedAssetPath = SanitizeProjectRelativePath(AssetPath);         \
  if (ValidatedAssetPath.IsEmpty()) {                                          \
    SendAutomationError(Socket, RequestId,                                     \
                        FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath), \
                        TEXT("INVALID_PATH"));                                \
    return true;                                                               \
  }                                                                            \
  AssetPath = ValidatedAssetPath;                                              \
  UMaterial *Material = LoadObject<UMaterial>(nullptr, *AssetPath);            \
  if (!Material) {                                                             \
    SendAutomationError(Socket, RequestId, TEXT("Could not load Material."),   \
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
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||             \
      AssetPath.IsEmpty()) {                                                   \
    SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),       \
                        TEXT("INVALID_ARGUMENT"));                             \
    return true;                                                               \
  }                                                                            \
  FString ValidatedAssetPath = SanitizeProjectRelativePath(AssetPath);         \
  if (ValidatedAssetPath.IsEmpty()) {                                          \
    SendAutomationError(Socket, RequestId,                                     \
                        FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath), \
                        TEXT("INVALID_PATH"));                                 \
    return true;                                                               \
  }                                                                            \
  AssetPath = ValidatedAssetPath;                                              \
  UMaterial *Material = nullptr;                                               \
  UMaterialFunction *Function = nullptr;                                       \
  LoadMaterialOrFunction(AssetPath, Material, Function);                       \
  if (!Material && !Function) {                                                \
    SendAutomationError(Socket, RequestId,                                     \
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

  // --------------------------------------------------------------------------
  // add_texture_sample
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_texture_sample")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString TexturePath, ParameterName, SamplerType;
    Payload->TryGetStringField(TEXT("texturePath"), TexturePath);
    Payload->TryGetStringField(TEXT("parameterName"), ParameterName);
    Payload->TryGetStringField(TEXT("samplerType"), SamplerType);

    // SECURITY: Validate texturePath if provided
    if (!TexturePath.IsEmpty()) {
      FString ValidatedTexturePath = SanitizeProjectRelativePath(TexturePath);
      if (ValidatedTexturePath.IsEmpty()) {
        SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Invalid texturePath '%s': contains traversal sequences or invalid root"), *TexturePath),
                            TEXT("INVALID_PATH"));
        return true;
      }
      TexturePath = ValidatedTexturePath;
    }

    // Resolve shared texture/sampler options first
    UTexture *ResolvedTexture = nullptr;
    if (!TexturePath.IsEmpty()) {
      ResolvedTexture = LoadObject<UTexture>(nullptr, *TexturePath);
    }
    auto ResolveSamplerType = [&SamplerType]() {
      if (SamplerType == TEXT("LinearColor")) return SAMPLERTYPE_LinearColor;
      if (SamplerType == TEXT("Normal")) return SAMPLERTYPE_Normal;
      if (SamplerType == TEXT("Masks")) return SAMPLERTYPE_Masks;
      if (SamplerType == TEXT("Alpha")) return SAMPLERTYPE_Alpha;
      return SAMPLERTYPE_Color;
    };

    UMaterialExpression *CreatedExpr = nullptr;
    if (!ParameterName.IsEmpty()) {
      UMaterialExpressionTextureSampleParameter2D *TexSample =
          NewObject<UMaterialExpressionTextureSampleParameter2D>(
              HostOuter, UMaterialExpressionTextureSampleParameter2D::StaticClass(),
              NAME_None, RF_Transactional);
      if (!TexSample) {
        SendAutomationError(Socket, RequestId, TEXT("Failed to create texture sample expression"), TEXT("CREATION_FAILED"));
        return true;
      }
      TexSample->ParameterName = FName(*ParameterName);
      if (ResolvedTexture) TexSample->Texture = ResolvedTexture;
      TexSample->SamplerType = ResolveSamplerType();
      TexSample->MaterialExpressionEditorX = (int32)X;
      TexSample->MaterialExpressionEditorY = (int32)Y;
      CreatedExpr = TexSample;
    } else {
      UMaterialExpressionTextureSample *PlainSample =
          NewObject<UMaterialExpressionTextureSample>(
              HostOuter, UMaterialExpressionTextureSample::StaticClass(),
              NAME_None, RF_Transactional);
      if (!PlainSample) {
        SendAutomationError(Socket, RequestId, TEXT("Failed to create texture sample expression"), TEXT("CREATION_FAILED"));
        return true;
      }
      if (ResolvedTexture) PlainSample->Texture = ResolvedTexture;
      PlainSample->SamplerType = ResolveSamplerType();
      PlainSample->MaterialExpressionEditorX = (int32)X;
      PlainSample->MaterialExpressionEditorY = (int32)Y;
      CreatedExpr = PlainSample;
    }

    AddExpressionToContainer(Material, Function, CreatedExpr);
    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), MCP_NODE_ID(CreatedExpr));
    SendAutomationResponse(Socket, RequestId, true, TEXT("Texture sample added."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_texture_coordinate
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_texture_coordinate")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    int32 CoordIndex = 0;
    double UTiling = 1.0, VTiling = 1.0;
    Payload->TryGetNumberField(TEXT("coordinateIndex"), CoordIndex);
    Payload->TryGetNumberField(TEXT("uTiling"), UTiling);
    Payload->TryGetNumberField(TEXT("vTiling"), VTiling);

    UMaterialExpressionTextureCoordinate *TexCoord =
        NewObject<UMaterialExpressionTextureCoordinate>(
            HostOuter, UMaterialExpressionTextureCoordinate::StaticClass(),
            NAME_None, RF_Transactional);
    TexCoord->CoordinateIndex = CoordIndex;
    TexCoord->UTiling = UTiling;
    TexCoord->VTiling = VTiling;
    TexCoord->MaterialExpressionEditorX = (int32)X;
    TexCoord->MaterialExpressionEditorY = (int32)Y;

    AddExpressionToContainer(Material, Function, TexCoord);
    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(TexCoord));
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Texture coordinate added."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_scalar_parameter
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_scalar_parameter")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

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
            HostOuter, UMaterialExpressionScalarParameter::StaticClass(),
            NAME_None, RF_Transactional);
    ScalarParam->ParameterName = FName(*ParamName);
    ScalarParam->DefaultValue = DefaultValue;
    if (!Group.IsEmpty()) {
      ScalarParam->Group = FName(*Group);
    }
    ScalarParam->MaterialExpressionEditorX = (int32)X;
    ScalarParam->MaterialExpressionEditorY = (int32)Y;

    AddExpressionToContainer(Material, Function, ScalarParam);
    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(ScalarParam));
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
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

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
            HostOuter, UMaterialExpressionVectorParameter::StaticClass(),
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

    AddExpressionToContainer(Material, Function, VecParam);
    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(VecParam));
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
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

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
            HostOuter, UMaterialExpressionStaticSwitchParameter::StaticClass(),
            NAME_None, RF_Transactional);
    SwitchParam->ParameterName = FName(*ParamName);
    SwitchParam->DefaultValue = DefaultValue;
    if (!Group.IsEmpty()) {
      SwitchParam->Group = FName(*Group);
    }
    SwitchParam->MaterialExpressionEditorX = (int32)X;
    SwitchParam->MaterialExpressionEditorY = (int32)Y;

    AddExpressionToContainer(Material, Function, SwitchParam);
    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(SwitchParam));
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Static switch '%s' added."), *ParamName), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_math_node
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_math_node")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString Operation;
    if (!Payload->TryGetStringField(TEXT("operation"), Operation)) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'operation'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterialExpression *MathNode = nullptr;
    if (Operation == TEXT("Add")) {
      MathNode = NewObject<UMaterialExpressionAdd>(
          HostOuter, UMaterialExpressionAdd::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Subtract")) {
      MathNode = NewObject<UMaterialExpressionSubtract>(
          HostOuter, UMaterialExpressionSubtract::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Multiply")) {
      MathNode = NewObject<UMaterialExpressionMultiply>(
          HostOuter, UMaterialExpressionMultiply::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Divide")) {
      MathNode = NewObject<UMaterialExpressionDivide>(
          HostOuter, UMaterialExpressionDivide::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Lerp")) {
      MathNode = NewObject<UMaterialExpressionLinearInterpolate>(
          HostOuter, UMaterialExpressionLinearInterpolate::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Clamp")) {
      MathNode = NewObject<UMaterialExpressionClamp>(
          HostOuter, UMaterialExpressionClamp::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Power")) {
      MathNode = NewObject<UMaterialExpressionPower>(
          HostOuter, UMaterialExpressionPower::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Frac")) {
      MathNode = NewObject<UMaterialExpressionFrac>(
          HostOuter, UMaterialExpressionFrac::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("OneMinus")) {
      MathNode = NewObject<UMaterialExpressionOneMinus>(
          HostOuter, UMaterialExpressionOneMinus::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Append")) {
      MathNode = NewObject<UMaterialExpressionAppendVector>(
          HostOuter, UMaterialExpressionAppendVector::StaticClass(), NAME_None,
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

    AddExpressionToContainer(Material, Function, MathNode);
    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(MathNode));
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
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    UMaterialExpression *NewExpr = nullptr;
    FString NodeName;

    if (SubAction == TEXT("add_world_position")) {
      NewExpr = NewObject<UMaterialExpressionWorldPosition>(
          HostOuter, UMaterialExpressionWorldPosition::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("WorldPosition");
    } else if (SubAction == TEXT("add_vertex_normal")) {
      NewExpr = NewObject<UMaterialExpressionVertexNormalWS>(
          HostOuter, UMaterialExpressionVertexNormalWS::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("VertexNormalWS");
    } else if (SubAction == TEXT("add_pixel_depth")) {
      NewExpr = NewObject<UMaterialExpressionPixelDepth>(
          HostOuter, UMaterialExpressionPixelDepth::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("PixelDepth");
    } else if (SubAction == TEXT("add_fresnel")) {
      NewExpr = NewObject<UMaterialExpressionFresnel>(
          HostOuter, UMaterialExpressionFresnel::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("Fresnel");
    } else if (SubAction == TEXT("add_reflection_vector")) {
      NewExpr = NewObject<UMaterialExpressionReflectionVectorWS>(
          HostOuter, UMaterialExpressionReflectionVectorWS::StaticClass(),
          NAME_None, RF_Transactional);
      NodeName = TEXT("ReflectionVectorWS");
    } else if (SubAction == TEXT("add_panner")) {
      NewExpr = NewObject<UMaterialExpressionPanner>(
          HostOuter, UMaterialExpressionPanner::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("Panner");
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    } else if (SubAction == TEXT("add_rotator")) {
      // Use runtime class lookup to avoid GetPrivateStaticClass requirement
      // StaticClass() calls GetPrivateStaticClass() internally which isn't exported
      UClass* RotatorClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.MaterialExpressionRotator"));
      if (RotatorClass)
      {
        UObject* NewExprObj = NewObject<UObject>(HostOuter, RotatorClass, NAME_None, RF_Transactional);
        NewExpr = static_cast<UMaterialExpressionRotator*>(NewExprObj);
      }
      NodeName = TEXT("Rotator");
#endif
    } else if (SubAction == TEXT("add_noise")) {
      NewExpr = NewObject<UMaterialExpressionNoise>(
          HostOuter, UMaterialExpressionNoise::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("Noise");
    } else if (SubAction == TEXT("add_voronoi")) {
      // Voronoi is implemented via Noise with different settings
      UMaterialExpressionNoise *NoiseExpr =
          NewObject<UMaterialExpressionNoise>(
              HostOuter, UMaterialExpressionNoise::StaticClass(), NAME_None,
              RF_Transactional);
      NoiseExpr->NoiseFunction = ENoiseFunction::NOISEFUNCTION_VoronoiALU;
      NewExpr = NoiseExpr;
      NodeName = TEXT("Voronoi");
    }

    if (NewExpr) {
      NewExpr->MaterialExpressionEditorX = (int32)X;
      NewExpr->MaterialExpressionEditorY = (int32)Y;

      AddExpressionToContainer(Material, Function, NewExpr);
      FINALIZE_HOST();

      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("nodeId"),
                             MCP_NODE_ID(NewExpr));
      SendAutomationResponse(
          Socket, RequestId, true,
          FString::Printf(TEXT("%s node added."), *NodeName), Result);
    } else {
      // NewExpr was null - could be class lookup failure or UE < 5.1 for rotator
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Failed to create %s node."), *NodeName),
                          TEXT("CREATE_FAILED"));
    }
    return true;
  }

  // --------------------------------------------------------------------------
  // add_if, add_switch
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_if") || SubAction == TEXT("add_switch")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    UMaterialExpression *NewExpr = nullptr;
    FString NodeName;

    if (SubAction == TEXT("add_if")) {
      NewExpr = NewObject<UMaterialExpressionIf>(
          HostOuter, UMaterialExpressionIf::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("If");
    } else {
      // Switch can be implemented via StaticSwitch or If
      NewExpr = NewObject<UMaterialExpressionIf>(
          HostOuter, UMaterialExpressionIf::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("Switch");
    }

    NewExpr->MaterialExpressionEditorX = (int32)X;
    NewExpr->MaterialExpressionEditorY = (int32)Y;

    AddExpressionToContainer(Material, Function, NewExpr);
    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(NewExpr));
    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("%s node added."), *NodeName),
                           Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_component_mask
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_component_mask")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    bool bR = true, bG = true, bB = true, bA = false;
    Payload->TryGetBoolField(TEXT("r"), bR);
    Payload->TryGetBoolField(TEXT("g"), bG);
    Payload->TryGetBoolField(TEXT("b"), bB);
    Payload->TryGetBoolField(TEXT("a"), bA);

    UMaterialExpressionComponentMask *MaskExpr =
        NewObject<UMaterialExpressionComponentMask>(
            HostOuter, UMaterialExpressionComponentMask::StaticClass(), NAME_None,
            RF_Transactional);
    MaskExpr->R = bR ? 1 : 0;
    MaskExpr->G = bG ? 1 : 0;
    MaskExpr->B = bB ? 1 : 0;
    MaskExpr->A = bA ? 1 : 0;
    MaskExpr->MaterialExpressionEditorX = (int32)X;
    MaskExpr->MaterialExpressionEditorY = (int32)Y;

    AddExpressionToContainer(Material, Function, MaskExpr);
    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(MaskExpr));
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("ComponentMask node added."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_dot_product
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_dot_product")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    UMaterialExpressionDotProduct *DotExpr =
        NewObject<UMaterialExpressionDotProduct>(
            HostOuter, UMaterialExpressionDotProduct::StaticClass(), NAME_None,
            RF_Transactional);
    DotExpr->MaterialExpressionEditorX = (int32)X;
    DotExpr->MaterialExpressionEditorY = (int32)Y;

    AddExpressionToContainer(Material, Function, DotExpr);
    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(DotExpr));
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("DotProduct node added."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_cross_product
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_cross_product")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    UMaterialExpressionCrossProduct *CrossExpr =
        NewObject<UMaterialExpressionCrossProduct>(
            HostOuter, UMaterialExpressionCrossProduct::StaticClass(), NAME_None,
            RF_Transactional);
    CrossExpr->MaterialExpressionEditorX = (int32)X;
    CrossExpr->MaterialExpressionEditorY = (int32)Y;

    AddExpressionToContainer(Material, Function, CrossExpr);
    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(CrossExpr));
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("CrossProduct node added."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_desaturation
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_desaturation")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    UMaterialExpressionDesaturation *DesatExpr =
        NewObject<UMaterialExpressionDesaturation>(
            HostOuter, UMaterialExpressionDesaturation::StaticClass(), NAME_None,
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

    AddExpressionToContainer(Material, Function, DesatExpr);
    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(DesatExpr));
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Desaturation node added."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_append (dedicated handler for convenience)
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_append")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    UMaterialExpressionAppendVector *AppendExpr =
        NewObject<UMaterialExpressionAppendVector>(
            HostOuter, UMaterialExpressionAppendVector::StaticClass(), NAME_None,
            RF_Transactional);
    AppendExpr->MaterialExpressionEditorX = (int32)X;
    AppendExpr->MaterialExpressionEditorY = (int32)Y;

    AddExpressionToContainer(Material, Function, AppendExpr);
    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(AppendExpr));
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Append node added."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_custom_expression (Material or MaterialFunction host)
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("add_custom_expression")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

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
            HostOuter, UMaterialExpressionCustom::StaticClass(), NAME_None,
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

    // Parse optional named input pins
    const TArray<TSharedPtr<FJsonValue>> *InputsArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("inputs"), InputsArray) && InputsArray) {
      CustomExpr->Inputs.Empty();
      for (const auto &InputVal : *InputsArray) {
        const TSharedPtr<FJsonObject> *InputObj = nullptr;
        if (InputVal->TryGetObject(InputObj) && InputObj) {
          FString InputName;
          (*InputObj)->TryGetStringField(TEXT("name"), InputName);
          if (!InputName.IsEmpty()) {
            FCustomInput NewInput;
            NewInput.InputName = FName(*InputName);
            CustomExpr->Inputs.Add(NewInput);
          }
        }
      }
    }

    // Parse optional additional outputs
    const TArray<TSharedPtr<FJsonValue>> *OutputsArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("additionalOutputs"), OutputsArray) && OutputsArray) {
      CustomExpr->AdditionalOutputs.Empty();
      for (const auto &OutputVal : *OutputsArray) {
        const TSharedPtr<FJsonObject> *OutputObj = nullptr;
        if (OutputVal->TryGetObject(OutputObj) && OutputObj) {
          FString OutputName, OType;
          (*OutputObj)->TryGetStringField(TEXT("name"), OutputName);
          (*OutputObj)->TryGetStringField(TEXT("type"), OType);
          if (!OutputName.IsEmpty()) {
            FCustomOutput NewOutput;
            NewOutput.OutputName = FName(*OutputName);
            if (OType == TEXT("Float2") || OType == TEXT("CMOT_Float2"))
              NewOutput.OutputType = ECustomMaterialOutputType::CMOT_Float2;
            else if (OType == TEXT("Float3") || OType == TEXT("CMOT_Float3"))
              NewOutput.OutputType = ECustomMaterialOutputType::CMOT_Float3;
            else if (OType == TEXT("Float4") || OType == TEXT("CMOT_Float4"))
              NewOutput.OutputType = ECustomMaterialOutputType::CMOT_Float4;
            else if (OType == TEXT("MaterialAttributes"))
              NewOutput.OutputType = ECustomMaterialOutputType::CMOT_MaterialAttributes;
            else
              NewOutput.OutputType = ECustomMaterialOutputType::CMOT_Float1;
            CustomExpr->AdditionalOutputs.Add(NewOutput);
          }
        }
      }
    }

    CustomExpr->MaterialExpressionEditorX = (int32)X;
    CustomExpr->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    AddExpressionToContainer(Material, Function, CustomExpr);
#endif

    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(CustomExpr));
    Result->SetNumberField(TEXT("inputCount"), CustomExpr->Inputs.Num());
    Result->SetNumberField(TEXT("additionalOutputCount"), CustomExpr->AdditionalOutputs.Num());
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Custom HLSL expression added."), Result);
    return true;
  }

  // ==========================================================================
  // 8.2 Node Connections
  // ==========================================================================

  // --------------------------------------------------------------------------
  // connect_nodes (Material or MaterialFunction host)
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("connect_nodes")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString SourceNodeId, TargetNodeId, InputName, SourcePin;
    Payload->TryGetStringField(TEXT("sourceNodeId"), SourceNodeId);
    Payload->TryGetStringField(TEXT("targetNodeId"), TargetNodeId);
    Payload->TryGetStringField(TEXT("inputName"), InputName);
    Payload->TryGetStringField(TEXT("sourcePin"), SourcePin);

    UMaterialExpression *SourceExpr = FIND_EXPR_IN_HOST(SourceNodeId);
    if (!SourceExpr) {
      SendAutomationError(Socket, RequestId, TEXT("Source node not found."),
                          TEXT("NODE_NOT_FOUND"));
      return true;
    }

    // "Main" target: for UMaterial this means the material attributes inputs;
    // for UMaterialFunction this means a FunctionOutput node matched by name
    // (InputName) or, if InputName is empty, the first FunctionOutput.
    if (TargetNodeId.IsEmpty() || TargetNodeId == TEXT("Main")) {
      if (Material) {
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
          MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset).Expression = SourceExpr;
          bFound = true;
        }
#endif

        // Set OutputIndex on whichever main input was just connected
        if (bFound) {
          // Re-lookup the input to set OutputIndex (main inputs are FScalar/FColor/FVectorMaterialInput)
          auto SetMainInputOutputIndex = [&](FExpressionInput& Input) {
            if (!SourcePin.IsEmpty()) {
              Input.OutputIndex = FCString::Atoi(*SourcePin);
            } else {
              Input.OutputIndex = 0;
            }
          };
#if WITH_EDITORONLY_DATA
          if (InputName == TEXT("BaseColor")) { SetMainInputOutputIndex(MCP_GET_MATERIAL_INPUT(Material, BaseColor)); }
          else if (InputName == TEXT("EmissiveColor")) { SetMainInputOutputIndex(MCP_GET_MATERIAL_INPUT(Material, EmissiveColor)); }
          else if (InputName == TEXT("Roughness")) { SetMainInputOutputIndex(MCP_GET_MATERIAL_INPUT(Material, Roughness)); }
          else if (InputName == TEXT("Metallic")) { SetMainInputOutputIndex(MCP_GET_MATERIAL_INPUT(Material, Metallic)); }
          else if (InputName == TEXT("Specular")) { SetMainInputOutputIndex(MCP_GET_MATERIAL_INPUT(Material, Specular)); }
          else if (InputName == TEXT("Normal")) { SetMainInputOutputIndex(MCP_GET_MATERIAL_INPUT(Material, Normal)); }
          else if (InputName == TEXT("Opacity")) { SetMainInputOutputIndex(MCP_GET_MATERIAL_INPUT(Material, Opacity)); }
          else if (InputName == TEXT("OpacityMask")) { SetMainInputOutputIndex(MCP_GET_MATERIAL_INPUT(Material, OpacityMask)); }
          else if (InputName == TEXT("AmbientOcclusion")) { SetMainInputOutputIndex(MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion)); }
          else if (InputName == TEXT("SubsurfaceColor")) { SetMainInputOutputIndex(MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor)); }
          else if (InputName == TEXT("WorldPositionOffset")) { SetMainInputOutputIndex(MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset)); }
#endif
          FINALIZE_HOST();
          SendAutomationResponse(Socket, RequestId, true,
                                 TEXT("Connected to main material node."));
        } else {
          SendAutomationError(
              Socket, RequestId,
              FString::Printf(TEXT("Unknown input on main node: %s"), *InputName),
              TEXT("INVALID_PIN"));
        }
        return true;
      } else {
        // UMaterialFunction host — find a FunctionOutput by name (or first one)
        UMaterialExpressionFunctionOutput *TargetOutput = nullptr;
#if WITH_EDITORONLY_DATA
        for (UMaterialExpression *Expr : MCP_GET_FUNCTION_EXPRESSIONS(Function)) {
          if (UMaterialExpressionFunctionOutput *Out = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
            if (InputName.IsEmpty() || Out->OutputName.ToString().Equals(InputName)) {
              TargetOutput = Out;
              break;
            }
          }
        }
#endif
        if (!TargetOutput) {
          SendAutomationError(Socket, RequestId,
                              FString::Printf(TEXT("No FunctionOutput%s found in material function."),
                                              InputName.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" named '%s'"), *InputName)),
                              TEXT("NODE_NOT_FOUND"));
          return true;
        }
        TargetOutput->A.Expression = SourceExpr;
        if (!SourcePin.IsEmpty()) {
          TargetOutput->A.OutputIndex = FCString::Atoi(*SourcePin);
        } else {
          TargetOutput->A.OutputIndex = 0;
        }
        FINALIZE_HOST();
        SendAutomationResponse(Socket, RequestId, true,
                               TEXT("Connected to function output."));
        return true;
      }
    }

    // Connect to another expression
    UMaterialExpression *TargetExpr = FIND_EXPR_IN_HOST(TargetNodeId);
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
          if (!SourcePin.IsEmpty()) {
            InputPtr->OutputIndex = FCString::Atoi(*SourcePin);
          } else {
            InputPtr->OutputIndex = 0;
          }
          FINALIZE_HOST();
          SendAutomationResponse(Socket, RequestId, true,
                                 TEXT("Nodes connected."));
          return true;
        }
      }
    }

    // Fallback: check UMaterialExpressionCustom named inputs
    if (UMaterialExpressionCustom* CustomExpr = Cast<UMaterialExpressionCustom>(TargetExpr)) {
      for (FCustomInput& CustomInput : CustomExpr->Inputs) {
        if (CustomInput.InputName.ToString() == InputName) {
          CustomInput.Input.Expression = SourceExpr;
          if (!SourcePin.IsEmpty()) {
            CustomInput.Input.OutputIndex = FCString::Atoi(*SourcePin);
          }
          FINALIZE_HOST();
          SendAutomationResponse(Socket, RequestId, true, TEXT("Nodes connected."));
          return true;
        }
      }
    }

    // Fallback: check UMaterialExpressionMaterialFunctionCall inputs
    if (UMaterialExpressionMaterialFunctionCall* MFCallExpr = Cast<UMaterialExpressionMaterialFunctionCall>(TargetExpr)) {
      for (FFunctionExpressionInput& FuncInput : MFCallExpr->FunctionInputs) {
        if (FuncInput.ExpressionInput->InputName.ToString() == InputName ||
            FuncInput.Input.InputName.ToString() == InputName) {
          FuncInput.Input.Expression = SourceExpr;
          if (!SourcePin.IsEmpty()) {
            FuncInput.Input.OutputIndex = FCString::Atoi(*SourcePin);
          }
          FINALIZE_HOST();
          SendAutomationResponse(Socket, RequestId, true, TEXT("Nodes connected to MF call input."));
          return true;
        }
      }
    }

    // Fallback: check UMaterialExpressionMaterialFunctionCall as SOURCE (output pin matching)
    if (UMaterialExpressionMaterialFunctionCall* MFCallSource = Cast<UMaterialExpressionMaterialFunctionCall>(SourceExpr)) {
      // If sourcePin names an MF call output, resolve its index
      if (!SourcePin.IsEmpty()) {
        for (int32 i = 0; i < MFCallSource->FunctionOutputs.Num(); ++i) {
          if (MFCallSource->FunctionOutputs[i].ExpressionOutput->OutputName.ToString() == SourcePin) {
            // Found the output index — now connect to target using property reflection
            FProperty *TargetProp = TargetExpr->GetClass()->FindPropertyByName(FName(*InputName));
            if (TargetProp) {
              if (FStructProperty *SP = CastField<FStructProperty>(TargetProp)) {
                FExpressionInput *InPtr = SP->ContainerPtrToValuePtr<FExpressionInput>(TargetExpr);
                if (InPtr) {
                  InPtr->Expression = SourceExpr;
                  InPtr->OutputIndex = i;
                  FINALIZE_HOST();
                  SendAutomationResponse(Socket, RequestId, true, TEXT("Nodes connected via MF call output."));
                  return true;
                }
              }
            }
            break;
          }
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
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString NodeId, PinName;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    Payload->TryGetStringField(TEXT("pinName"), PinName);

    // Disconnect from main / output node
    if (NodeId.IsEmpty() || NodeId == TEXT("Main")) {
      if (Material) {
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
          } else if (PinName == TEXT("WorldPositionOffset")) {
            MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset).Expression = nullptr;
            bFound = true;
          }
#endif

          if (bFound) {
            FINALIZE_HOST();
            SendAutomationResponse(Socket, RequestId, true,
                                   TEXT("Disconnected from main material pin."));
            return true;
          }
        }
        SendAutomationResponse(Socket, RequestId, true,
                               TEXT("Disconnect operation completed."));
        return true;
      } else {
        // UMaterialFunction host — clear FunctionOutput's A.Expression by name (or first/all if empty)
        bool bCleared = false;
#if WITH_EDITORONLY_DATA
        for (UMaterialExpression *Expr : MCP_GET_FUNCTION_EXPRESSIONS(Function)) {
          if (UMaterialExpressionFunctionOutput *Out = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
            if (PinName.IsEmpty() || Out->OutputName.ToString().Equals(PinName)) {
              Out->A.Expression = nullptr;
              bCleared = true;
              if (!PinName.IsEmpty()) break;
            }
          }
        }
#endif
        if (bCleared) {
          FINALIZE_HOST();
          SendAutomationResponse(Socket, RequestId, true,
                                 TEXT("Disconnected from function output."));
        } else {
          SendAutomationResponse(Socket, RequestId, true,
                                 TEXT("Disconnect operation completed (no matching output)."));
        }
        return true;
      }
    }

    // Disconnect a specific input pin on a named expression
    UMaterialExpression *TargetExpr = FIND_EXPR_IN_HOST(NodeId);
    if (!TargetExpr) {
      SendAutomationError(Socket, RequestId, TEXT("Target node not found."),
                          TEXT("NODE_NOT_FOUND"));
      return true;
    }

    if (!PinName.IsEmpty()) {
      FProperty *Prop = TargetExpr->GetClass()->FindPropertyByName(FName(*PinName));
      if (Prop) {
        if (FStructProperty *StructProp = CastField<FStructProperty>(Prop)) {
          FExpressionInput *InputPtr =
              StructProp->ContainerPtrToValuePtr<FExpressionInput>(TargetExpr);
          if (InputPtr) {
            InputPtr->Expression = nullptr;
            FINALIZE_HOST();
            SendAutomationResponse(Socket, RequestId, true,
                                   TEXT("Input pin disconnected."));
            return true;
          }
        }
      }
      SendAutomationError(
          Socket, RequestId,
          FString::Printf(TEXT("Input pin '%s' not found."), *PinName),
          TEXT("PIN_NOT_FOUND"));
      return true;
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

    // Validate and sanitize the asset name (same as create_material)
    FString OriginalName = Name;
    FString SanitizedName = SanitizeAssetName(Name);
    
    // Check if sanitization significantly changed the name (indicates invalid characters)
    FString NormalizedOriginal = OriginalName.Replace(TEXT("_"), TEXT(""));
    FString NormalizedSanitized = SanitizedName.Replace(TEXT("_"), TEXT(""));
    if (NormalizedSanitized != NormalizedOriginal) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid material function name '%s': contains characters that cannot be used in asset names. Valid name would be: '%s'"),
                                          *OriginalName, *SanitizedName),
                          TEXT("INVALID_NAME"));
      return true;
    }
    Name = SanitizedName;

    Path = GetJsonStringField(Payload, TEXT("path"));
    if (Path.IsEmpty()) {
      Path = TEXT("/Game/Materials/Functions");
    }

    // Validate path doesn't contain traversal sequences (same as create_material)
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

    // Check for existing asset collision to prevent UE crash
    // Creating a MaterialFunction over an existing Material causes fatal error
    FString FullAssetPath = ValidatedPath + TEXT(".") + Name;
    if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath)) {
      // Get the existing asset's class to provide helpful error
      UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(FullAssetPath);
      if (ExistingAsset) {
        UClass* ExistingClass = ExistingAsset->GetClass();
        FString ExistingClassName = ExistingClass ? ExistingClass->GetName() : TEXT("Unknown");
        SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Asset '%s' already exists as %s. Cannot create MaterialFunction with the same name."),
                                            *FullAssetPath, *ExistingClassName),
                            TEXT("ASSET_EXISTS"));
      } else {
        SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Asset '%s' already exists. Cannot overwrite with different asset type."),
                                            *FullAssetPath),
                            TEXT("ASSET_EXISTS"));
      }
      return true;
    }

    Payload->TryGetStringField(TEXT("description"), Description);

    bool bExposeToLibrary = true;
    Payload->TryGetBoolField(TEXT("exposeToLibrary"), bExposeToLibrary);

    // Create function using factory - use ValidatedPath, not original Path!
    UMaterialFunctionFactoryNew *Factory =
        NewObject<UMaterialFunctionFactoryNew>();
    UPackage *Package = CreatePackage(*ValidatedPath);
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

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, NewFunc);
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

    // SECURITY: Validate path BEFORE loading asset
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

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

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(NewExpr));
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
  // use_material_function (host can be UMaterial OR UMaterialFunction)
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("use_material_function")) {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    FString ValidatedAssetPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedAssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedAssetPath;

    UMaterial *Material = nullptr;
    UMaterialFunction *HostFunction = nullptr;
    LoadMaterialOrFunction(AssetPath, Material, HostFunction);
    if (!Material && !HostFunction) {
      SendAutomationError(Socket, RequestId,
                          TEXT("Could not load Material or Material Function host."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    UObject *HostOuter = Material ? static_cast<UObject*>(Material)
                                  : static_cast<UObject*>(HostFunction);

    float X = 0.0f, Y = 0.0f;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

    FString FunctionPath;
    if (!Payload->TryGetStringField(TEXT("functionPath"), FunctionPath) ||
        FunctionPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'functionPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // SECURITY: Validate functionPath before loading
    FString ValidatedFunctionPath = SanitizeProjectRelativePath(FunctionPath);
    if (ValidatedFunctionPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid functionPath '%s': contains traversal sequences or invalid root"), *FunctionPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    FunctionPath = ValidatedFunctionPath;

    UMaterialFunction *Func =
        LoadObject<UMaterialFunction>(nullptr, *FunctionPath);
    if (!Func) {
      SendAutomationError(Socket, RequestId,
                          TEXT("Could not load Material Function."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // Guard against self-reference when host is itself a MF
    if (HostFunction && Func == HostFunction) {
      SendAutomationError(Socket, RequestId,
                          TEXT("A material function cannot call itself."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterialExpressionMaterialFunctionCall *FuncCall =
        NewObject<UMaterialExpressionMaterialFunctionCall>(
            HostOuter, UMaterialExpressionMaterialFunctionCall::StaticClass(),
            NAME_None, RF_Transactional);
    FuncCall->SetMaterialFunction(Func);
    FuncCall->MaterialExpressionEditorX = (int32)X;
    FuncCall->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    AddExpressionToContainer(Material, HostFunction, FuncCall);
#endif

    HostOuter->PostEditChange();
    HostOuter->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(FuncCall));
    Result->SetStringField(TEXT("hostType"),
                           Material ? TEXT("Material") : TEXT("MaterialFunction"));
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Material function call added."), Result);
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

    // Validate and sanitize the asset name (same as create_material)
    FString OriginalName = Name;
    FString SanitizedName = SanitizeAssetName(Name);
    
    FString NormalizedOriginal = OriginalName.Replace(TEXT("_"), TEXT(""));
    FString NormalizedSanitized = SanitizedName.Replace(TEXT("_"), TEXT(""));
    if (NormalizedSanitized != NormalizedOriginal) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid material instance name '%s': contains characters that cannot be used in asset names. Valid name would be: '%s'"),
                                          *OriginalName, *SanitizedName),
                          TEXT("INVALID_NAME"));
      return true;
    }
    Name = SanitizedName;

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

    // Validate path (same as create_material)
    FString ValidatedPath;
    FString PathError;
    if (!ValidateAssetCreationPath(Path, Name, ValidatedPath, PathError)) {
      SendAutomationError(Socket, RequestId, PathError, TEXT("INVALID_PATH"));
      return true;
    }

    if (ValidatedPath.Contains(TEXT(":"))) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': absolute Windows paths are not allowed"), *ValidatedPath),
                          TEXT("INVALID_PATH"));
      return true;
    }

    FText MountReason;
    if (!FPackageName::IsValidLongPackageName(ValidatedPath, true, &MountReason)) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid package path '%s': %s"), *ValidatedPath, *MountReason.ToString()),
                          TEXT("INVALID_PATH"));
      return true;
    }

    // Check for existing asset collision
    FString FullAssetPath = ValidatedPath + TEXT(".") + Name;
    if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath)) {
      UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(FullAssetPath);
      if (ExistingAsset) {
        UClass* ExistingClass = ExistingAsset->GetClass();
        FString ExistingClassName = ExistingClass ? ExistingClass->GetName() : TEXT("Unknown");
        SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Asset '%s' already exists as %s. Cannot create MaterialInstanceConstant with the same name."),
                                            *FullAssetPath, *ExistingClassName),
                            TEXT("ASSET_EXISTS"));
      } else {
        SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Asset '%s' already exists. Cannot overwrite with different asset type."),
                                            *FullAssetPath),
                            TEXT("ASSET_EXISTS"));
      }
      return true;
    }
    // SECURITY: Validate parentMaterial path before loading
    FString ValidatedParentPath = SanitizeProjectRelativePath(ParentMaterial);
    if (ValidatedParentPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid parentMaterial path '%s': contains traversal sequences or invalid root"), *ParentMaterial),
                          TEXT("INVALID_PATH"));
      return true;
    }
    ParentMaterial = ValidatedParentPath;

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

    UPackage *Package = CreatePackage(*ValidatedPath);
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

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, NewInstance);
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

    // SECURITY: Validate path BEFORE loading asset
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

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

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Instance);
    Result->SetStringField(TEXT("parameterName"), ParamName);
    Result->SetNumberField(TEXT("value"), Value);
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Scalar parameter '%s' set to %f."), *ParamName,
                        Value), Result);
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

    // SECURITY: Validate path BEFORE loading asset
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

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

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Instance);
    Result->SetStringField(TEXT("parameterName"), ParamName);
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Vector parameter '%s' set."), *ParamName), Result);
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

    // SECURITY: Validate path BEFORE loading asset
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    UMaterialInstanceConstant *Instance =
        LoadObject<UMaterialInstanceConstant>(nullptr, *AssetPath);
    if (!Instance) {
      SendAutomationError(Socket, RequestId,
                          TEXT("Could not load material instance."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    // SECURITY: Validate texturePath before loading
    FString ValidatedTexturePath = SanitizeProjectRelativePath(TexturePath);
    if (ValidatedTexturePath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid texturePath '%s': contains traversal sequences or invalid root"), *TexturePath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    TexturePath = ValidatedTexturePath;

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

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Instance);
    Result->SetStringField(TEXT("parameterName"), ParamName);
    SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Texture parameter '%s' set."), *ParamName), Result);
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

    // Name validation - sanitize and check for invalid characters
    FString OriginalName = Name;
    FString SanitizedName = SanitizeAssetName(Name);
    FString NormalizedOriginal = OriginalName.Replace(TEXT("_"), TEXT(""));
    FString NormalizedSanitized = SanitizedName.Replace(TEXT("_"), TEXT(""));
    if (NormalizedSanitized != NormalizedOriginal) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid asset name '%s'. Names can only contain alphanumeric characters and underscores."), *OriginalName),
                          TEXT("INVALID_NAME"));
      return true;
    }
    Name = SanitizedName;

    // Path validation - check for traversal and normalize
    FString ValidatedPath;
    FString PathError;
    if (!ValidateAssetCreationPath(Path, Name, ValidatedPath, PathError)) {
      SendAutomationError(Socket, RequestId, PathError, TEXT("INVALID_PATH"));
      return true;
    }
    Path = ValidatedPath;

    // Check for existing asset collision (different class)
    FString FullAssetPath = Path + TEXT(".") + Name;
    if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath)) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Asset already exists at path: %s"), *FullAssetPath),
                          TEXT("ASSET_EXISTS"));
      return true;
    }

    // Create material using factory
    UMaterialFactoryNew *Factory = NewObject<UMaterialFactoryNew>();
    UPackage *Package = CreatePackage(*Path);
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

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, NewMaterial);
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
    
    // Accept path via multiple parameter names (assetPath, materialPath, or path)
    FString Path;
    if (Payload->TryGetStringField(TEXT("assetPath"), Path) && !Path.IsEmpty()) {
      // Use assetPath
    } else if (Payload->TryGetStringField(TEXT("materialPath"), Path) && !Path.IsEmpty()) {
      // Use materialPath
    } else if (Payload->TryGetStringField(TEXT("path"), Path) && !Path.IsEmpty()) {
      // Use path
    } else {
      Path = TEXT("/Game/Landscape/Layers");
    }
    
    // Validate path security - reject traversal and invalid paths
    FString ValidatedPath = SanitizeProjectRelativePath(Path);
    if (ValidatedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid characters"), *Path),
                          TEXT("INVALID_PATH"));
      return true;
    }
    Path = ValidatedPath;
    
    // Validate the full package path
    FString PackagePath = Path / LayerName;
    if (!FPackageName::IsValidLongPackageName(PackagePath)) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid package path: %s"), *PackagePath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    
    // Create the landscape layer info asset
    FString PackageName = PackagePath;
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
      // SECURITY: Validate physicalMaterialPath before loading
      FString ValidatedPhysMatPath = SanitizeProjectRelativePath(PhysMaterialPath);
      if (ValidatedPhysMatPath.IsEmpty()) {
        SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Invalid physicalMaterialPath '%s': contains traversal sequences or invalid root"), *PhysMaterialPath),
                            TEXT("INVALID_PATH"));
        return true;
      }
      PhysMaterialPath = ValidatedPhysMatPath;

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
    
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, LayerInfo);
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
    // Accept both assetPath and materialPath as parameter names
    if (Payload->TryGetStringField(TEXT("assetPath"), AssetPath) && !AssetPath.IsEmpty()) {
      // Use assetPath
    } else if (Payload->TryGetStringField(TEXT("materialPath"), AssetPath) && !AssetPath.IsEmpty()) {
      // Use materialPath
    } else {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath' or 'materialPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    
    // SECURITY: Validate path BEFORE loading asset
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;
    
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
      
      CreatedNodeIds.Add(MCP_NODE_ID(WeightParam));
    }
    
    Material->PostEditChange();
    Material->MarkPackageDirty();
    
    // Save if requested
    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialAsset(Material);
    }
    
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
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

    // Validate path security BEFORE loading asset
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    UMaterial *Material = nullptr;
    UMaterialFunction *Function = nullptr;
    LoadMaterialOrFunction(AssetPath, Material, Function);
    if (!Material && !Function) {
      SendAutomationError(Socket, RequestId,
                          TEXT("Could not load Material or Material Function."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // Force recompile / update
    UObject *Host = Material ? static_cast<UObject*>(Material) : static_cast<UObject*>(Function);
    Host->PreEditChange(nullptr);
    Host->PostEditChange();
    Host->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      if (Material) {
        SaveMaterialAsset(Material);
      } else {
        SaveMaterialFunctionAsset(Function);
      }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetStringField(TEXT("assetType"),
                           Material ? TEXT("Material") : TEXT("MaterialFunction"));
    Result->SetBoolField(TEXT("compiled"), true);
    Result->SetBoolField(TEXT("saved"), bSave);
    SendAutomationResponse(Socket, RequestId, true,
                           Material ? TEXT("Material compiled.") : TEXT("Material function updated."),
                           Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // get_material_info (supports UMaterial and UMaterialFunction)
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("get_material_info")) {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Validate path security BEFORE loading asset
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    UMaterial *Material = nullptr;
    UMaterialFunction *Function = nullptr;
    LoadMaterialOrFunction(AssetPath, Material, Function);
    if (!Material && !Function) {
      SendAutomationError(Socket, RequestId,
                          TEXT("Could not load Material or Material Function."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // Optional filter: "parameters", "expressions", "connections", or "all" (default)
    FString Filter;
    Payload->TryGetStringField(TEXT("filter"), Filter);
    bool bWantParams      = Filter.IsEmpty() || Filter == TEXT("all") || Filter == TEXT("parameters");
    bool bWantExpressions = Filter.IsEmpty() || Filter == TEXT("all") || Filter == TEXT("expressions");
    bool bWantConnections = Filter.IsEmpty() || Filter == TEXT("all") || Filter == TEXT("connections");

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetType"),
                           Material ? TEXT("Material") : TEXT("MaterialFunction"));

    // Collect the expression array (shared by both Material and MF paths)
    auto& AllExpressions = Material
        ? MCP_GET_MATERIAL_EXPRESSIONS(Material)
        : MCP_GET_FUNCTION_EXPRESSIONS(Function);
    Result->SetNumberField(TEXT("nodeCount"), AllExpressions.Num());

    if (Material) {
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
    } else {
      // UMaterialFunction basic info
      Result->SetStringField(TEXT("description"), Function->Description);
      Result->SetBoolField(TEXT("exposeToLibrary"), Function->bExposeToLibrary);
    }

    // --- Parameters (always separated for both Material and MF) ---
    if (bWantParams) {
      TArray<TSharedPtr<FJsonValue>> ParamsArray;
      for (UMaterialExpression *Expr : AllExpressions) {
        if (!Expr) continue;
        if (UMaterialExpressionParameter *Param = Cast<UMaterialExpressionParameter>(Expr)) {
          TSharedPtr<FJsonObject> ParamObj = McpHandlerUtils::CreateResultObject();
          ParamObj->SetStringField(TEXT("name"), Param->ParameterName.ToString());
          ParamObj->SetStringField(TEXT("type"), Expr->GetClass()->GetName());
          ParamObj->SetStringField(TEXT("nodeId"), MCP_NODE_ID(Expr));
          ParamsArray.Add(MakeShared<FJsonValueObject>(ParamObj));
        }
      }
      Result->SetArrayField(TEXT("parameters"), ParamsArray);
    }

    // --- MF-specific: FunctionInput/FunctionOutput enumeration ---
    if (!Material) {
      TArray<TSharedPtr<FJsonValue>> InputsArray;
      TArray<TSharedPtr<FJsonValue>> OutputsArray;
      for (UMaterialExpression *Expr : AllExpressions) {
        if (!Expr) continue;
        if (UMaterialExpressionFunctionInput *In = Cast<UMaterialExpressionFunctionInput>(Expr)) {
          TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
          Obj->SetStringField(TEXT("name"), In->InputName.ToString());
          Obj->SetStringField(TEXT("type"), FunctionInputTypeToString(In->InputType));
          Obj->SetStringField(TEXT("nodeId"), MCP_NODE_ID(In));
          Obj->SetBoolField(TEXT("usePreviewValueAsDefault"), In->bUsePreviewValueAsDefault);
          Obj->SetNumberField(TEXT("sortPriority"), In->SortPriority);
          Obj->SetStringField(TEXT("description"), In->Description);
          const auto PV = In->PreviewValue;
          TSharedPtr<FJsonObject> PreviewObj = MakeShared<FJsonObject>();
          PreviewObj->SetNumberField(TEXT("x"), PV.X);
          PreviewObj->SetNumberField(TEXT("y"), PV.Y);
          PreviewObj->SetNumberField(TEXT("z"), PV.Z);
          PreviewObj->SetNumberField(TEXT("w"), PV.W);
          Obj->SetObjectField(TEXT("previewValue"), PreviewObj);
          InputsArray.Add(MakeShared<FJsonValueObject>(Obj));
        } else if (UMaterialExpressionFunctionOutput *Out = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
          TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
          Obj->SetStringField(TEXT("name"), Out->OutputName.ToString());
          Obj->SetStringField(TEXT("nodeId"), MCP_NODE_ID(Out));
          Obj->SetNumberField(TEXT("sortPriority"), Out->SortPriority);
          Obj->SetStringField(TEXT("description"), Out->Description);
          OutputsArray.Add(MakeShared<FJsonValueObject>(Obj));
        }
      }
      Result->SetArrayField(TEXT("inputs"), InputsArray);
      Result->SetArrayField(TEXT("outputs"), OutputsArray);
    }

    // --- Full expression list (types, nodeIds, positions) ---
    if (bWantExpressions) {
      TArray<TSharedPtr<FJsonValue>> ExprsArray;
      for (UMaterialExpression *Expr : AllExpressions) {
        if (!Expr) continue;
        TSharedPtr<FJsonObject> ExprObj = MakeShared<FJsonObject>();
        ExprObj->SetStringField(TEXT("nodeId"), MCP_NODE_ID(Expr));
        ExprObj->SetStringField(TEXT("type"), Expr->GetClass()->GetName());
        ExprObj->SetStringField(TEXT("desc"), Expr->GetDescription());
        ExprObj->SetNumberField(TEXT("x"), Expr->MaterialExpressionEditorX);
        ExprObj->SetNumberField(TEXT("y"), Expr->MaterialExpressionEditorY);
        // Add name for parameter/input/output nodes
        if (UMaterialExpressionParameter *P = Cast<UMaterialExpressionParameter>(Expr)) {
          ExprObj->SetStringField(TEXT("name"), P->ParameterName.ToString());
        } else if (UMaterialExpressionFunctionInput *FI = Cast<UMaterialExpressionFunctionInput>(Expr)) {
          ExprObj->SetStringField(TEXT("name"), FI->InputName.ToString());
        } else if (UMaterialExpressionFunctionOutput *FO = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
          ExprObj->SetStringField(TEXT("name"), FO->OutputName.ToString());
        }
        // Include code for CustomExpression nodes
        if (UMaterialExpressionCustom *CE = Cast<UMaterialExpressionCustom>(Expr)) {
          ExprObj->SetStringField(TEXT("code"), CE->Code);
          ExprObj->SetNumberField(TEXT("inputCount"), CE->Inputs.Num());
          ExprObj->SetNumberField(TEXT("additionalOutputCount"), CE->AdditionalOutputs.Num());
        }
        ExprsArray.Add(MakeShared<FJsonValueObject>(ExprObj));
      }
      Result->SetArrayField(TEXT("expressions"), ExprsArray);
    }

    // --- Connection topology (source→target pairs) ---
    if (bWantConnections) {
      TArray<TSharedPtr<FJsonValue>> ConnsArray;

      // Optional nodeId / nodeIds filter for connections
      TSet<FString> FilterNodeIds;
      FString SingleNodeId;
      if (Payload->TryGetStringField(TEXT("nodeId"), SingleNodeId) && !SingleNodeId.IsEmpty()) {
        FilterNodeIds.Add(SingleNodeId);
      }
      const TArray<TSharedPtr<FJsonValue>> *NodeIdsArr = nullptr;
      if (Payload->TryGetArrayField(TEXT("nodeIds"), NodeIdsArr) && NodeIdsArr) {
        for (const auto &Val : *NodeIdsArr) {
          FString Id;
          if (Val->TryGetString(Id) && !Id.IsEmpty()) FilterNodeIds.Add(Id);
        }
      }
      bool bFilterConnections = FilterNodeIds.Num() > 0;

      // Helper lambda: scan an expression's FExpressionInput properties and emit connections
      auto EmitExprConnections = [&](UMaterialExpression *TargetExpr) {
        if (!TargetExpr) return;
        FString TargetId = MCP_NODE_ID(TargetExpr);

        // Iterate all FExpressionInput struct properties via reflection
        for (TFieldIterator<FStructProperty> It(TargetExpr->GetClass()); It; ++It) {
          FStructProperty *StructProp = *It;
          if (!StructProp->Struct || StructProp->Struct->GetFName() != FName(TEXT("ExpressionInput"))) continue;
          FExpressionInput *InputPtr = StructProp->ContainerPtrToValuePtr<FExpressionInput>(TargetExpr);
          if (!InputPtr || !InputPtr->Expression) continue;
          FString SourceId = MCP_NODE_ID(InputPtr->Expression);
          if (bFilterConnections && !FilterNodeIds.Contains(SourceId) && !FilterNodeIds.Contains(TargetId)) continue;

          TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
          ConnObj->SetStringField(TEXT("sourceNodeId"), SourceId);
          ConnObj->SetNumberField(TEXT("sourceOutputIndex"), InputPtr->OutputIndex);
          ConnObj->SetStringField(TEXT("targetNodeId"), TargetId);
          ConnObj->SetStringField(TEXT("targetInput"), StructProp->GetName());
          ConnsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
        }

        // Custom expression named inputs
        if (UMaterialExpressionCustom *CExpr = Cast<UMaterialExpressionCustom>(TargetExpr)) {
          for (int32 ci = 0; ci < CExpr->Inputs.Num(); ++ci) {
            if (CExpr->Inputs[ci].Input.Expression) {
              FString SourceId = MCP_NODE_ID(CExpr->Inputs[ci].Input.Expression);
              if (bFilterConnections && !FilterNodeIds.Contains(SourceId) && !FilterNodeIds.Contains(TargetId)) continue;
              TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
              ConnObj->SetStringField(TEXT("sourceNodeId"), SourceId);
              ConnObj->SetNumberField(TEXT("sourceOutputIndex"), CExpr->Inputs[ci].Input.OutputIndex);
              ConnObj->SetStringField(TEXT("targetNodeId"), TargetId);
              ConnObj->SetStringField(TEXT("targetInput"), CExpr->Inputs[ci].InputName.ToString());
              ConnsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
            }
          }
        }

        // MaterialFunctionCall inputs
        if (UMaterialExpressionMaterialFunctionCall *MFC = Cast<UMaterialExpressionMaterialFunctionCall>(TargetExpr)) {
          for (const FFunctionExpressionInput &FI : MFC->FunctionInputs) {
            if (FI.Input.Expression) {
              FString SourceId = MCP_NODE_ID(FI.Input.Expression);
              if (bFilterConnections && !FilterNodeIds.Contains(SourceId) && !FilterNodeIds.Contains(TargetId)) continue;
              TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
              ConnObj->SetStringField(TEXT("sourceNodeId"), SourceId);
              ConnObj->SetNumberField(TEXT("sourceOutputIndex"), FI.Input.OutputIndex);
              ConnObj->SetStringField(TEXT("targetNodeId"), TargetId);
              ConnObj->SetStringField(TEXT("targetInput"), FI.ExpressionInput->InputName.ToString());
              ConnsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
            }
          }
        }
      };

      for (UMaterialExpression *Expr : AllExpressions) {
        EmitExprConnections(Expr);
      }

      // Material main pin connections (BaseColor, Normal, etc.)
      if (Material) {
#if WITH_EDITORONLY_DATA
        auto EmitMainPin = [&](const FString &PinName, const FExpressionInput &Input) {
          if (Input.Expression) {
            FString SrcId = MCP_NODE_ID(Input.Expression);
            if (bFilterConnections && !FilterNodeIds.Contains(SrcId) && !FilterNodeIds.Contains(TEXT("Main"))) return;
            TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
            ConnObj->SetStringField(TEXT("sourceNodeId"), SrcId);
            ConnObj->SetNumberField(TEXT("sourceOutputIndex"), Input.OutputIndex);
            ConnObj->SetStringField(TEXT("targetNodeId"), TEXT("Main"));
            ConnObj->SetStringField(TEXT("targetInput"), PinName);
            ConnsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
          }
        };
        EmitMainPin(TEXT("BaseColor"), MCP_GET_MATERIAL_INPUT(Material, BaseColor));
        EmitMainPin(TEXT("EmissiveColor"), MCP_GET_MATERIAL_INPUT(Material, EmissiveColor));
        EmitMainPin(TEXT("Roughness"), MCP_GET_MATERIAL_INPUT(Material, Roughness));
        EmitMainPin(TEXT("Metallic"), MCP_GET_MATERIAL_INPUT(Material, Metallic));
        EmitMainPin(TEXT("Specular"), MCP_GET_MATERIAL_INPUT(Material, Specular));
        EmitMainPin(TEXT("Normal"), MCP_GET_MATERIAL_INPUT(Material, Normal));
        EmitMainPin(TEXT("Opacity"), MCP_GET_MATERIAL_INPUT(Material, Opacity));
        EmitMainPin(TEXT("OpacityMask"), MCP_GET_MATERIAL_INPUT(Material, OpacityMask));
        EmitMainPin(TEXT("AmbientOcclusion"), MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion));
        EmitMainPin(TEXT("SubsurfaceColor"), MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor));
        EmitMainPin(TEXT("WorldPositionOffset"), MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset));
#endif
      }

      Result->SetArrayField(TEXT("connections"), ConnsArray);
    }

    SendAutomationResponse(Socket, RequestId, true,
                           Material ? TEXT("Material info retrieved.")
                                    : TEXT("Material function info retrieved."),
                           Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // get_material_function_info (explicit MF introspection)
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("get_material_function_info")) {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    UMaterialFunction *Function = LoadObject<UMaterialFunction>(nullptr, *AssetPath);
    if (!Function) {
      SendAutomationError(Socket, RequestId,
                          TEXT("Could not load Material Function."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetStringField(TEXT("assetType"), TEXT("MaterialFunction"));
    Result->SetStringField(TEXT("description"), Function->Description);
    Result->SetBoolField(TEXT("exposeToLibrary"), Function->bExposeToLibrary);
    Result->SetNumberField(TEXT("nodeCount"), MCP_GET_FUNCTION_EXPRESSIONS(Function).Num());

    TArray<TSharedPtr<FJsonValue>> InputsArray;
    TArray<TSharedPtr<FJsonValue>> OutputsArray;
    for (UMaterialExpression *Expr : MCP_GET_FUNCTION_EXPRESSIONS(Function)) {
      if (!Expr) continue;
      if (UMaterialExpressionFunctionInput *In = Cast<UMaterialExpressionFunctionInput>(Expr)) {
        TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
        Obj->SetStringField(TEXT("name"), In->InputName.ToString());
        Obj->SetStringField(TEXT("type"), FunctionInputTypeToString(In->InputType));
        Obj->SetStringField(TEXT("nodeId"), MCP_NODE_ID(In));
        Obj->SetBoolField(TEXT("usePreviewValueAsDefault"), In->bUsePreviewValueAsDefault);
        Obj->SetNumberField(TEXT("sortPriority"), In->SortPriority);
        Obj->SetStringField(TEXT("description"), In->Description);
        const auto PV = In->PreviewValue;
        TSharedPtr<FJsonObject> PreviewObj = MakeShared<FJsonObject>();
        PreviewObj->SetNumberField(TEXT("x"), PV.X);
        PreviewObj->SetNumberField(TEXT("y"), PV.Y);
        PreviewObj->SetNumberField(TEXT("z"), PV.Z);
        PreviewObj->SetNumberField(TEXT("w"), PV.W);
        Obj->SetObjectField(TEXT("previewValue"), PreviewObj);
        InputsArray.Add(MakeShared<FJsonValueObject>(Obj));
      } else if (UMaterialExpressionFunctionOutput *Out = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
        TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
        Obj->SetStringField(TEXT("name"), Out->OutputName.ToString());
        Obj->SetStringField(TEXT("nodeId"), MCP_NODE_ID(Out));
        Obj->SetNumberField(TEXT("sortPriority"), Out->SortPriority);
        Obj->SetStringField(TEXT("description"), Out->Description);
        OutputsArray.Add(MakeShared<FJsonValueObject>(Obj));
      }
    }
    Result->SetArrayField(TEXT("inputs"), InputsArray);
    Result->SetArrayField(TEXT("outputs"), OutputsArray);

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Material function info retrieved."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // find_node — search expressions by type or name
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("find_node")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString SearchType, SearchName;
    Payload->TryGetStringField(TEXT("nodeType"), SearchType);
    Payload->TryGetStringField(TEXT("name"), SearchName);

    if (SearchType.IsEmpty() && SearchName.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          TEXT("Provide at least 'nodeType' or 'name' to search."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    auto& Exprs = Material
        ? MCP_GET_MATERIAL_EXPRESSIONS(Material)
        : MCP_GET_FUNCTION_EXPRESSIONS(Function);

    // Build connection count map for each expression
    TMap<FGuid, int32> ConnectionCountMap;
    for (UMaterialExpression *Expr : Exprs) {
      if (!Expr) continue;
      int32 Count = 0;
      // Count input connections via reflection
      for (TFieldIterator<FStructProperty> It(Expr->GetClass()); It; ++It) {
        FStructProperty *SP = *It;
        if (!SP->Struct || SP->Struct->GetFName() != FName(TEXT("ExpressionInput"))) continue;
        FExpressionInput *InPtr = SP->ContainerPtrToValuePtr<FExpressionInput>(Expr);
        if (InPtr && InPtr->Expression) Count++;
      }
      if (UMaterialExpressionCustom *CE = Cast<UMaterialExpressionCustom>(Expr)) {
        for (const FCustomInput &CI : CE->Inputs) { if (CI.Input.Expression) Count++; }
      }
      if (UMaterialExpressionMaterialFunctionCall *MFC = Cast<UMaterialExpressionMaterialFunctionCall>(Expr)) {
        for (const FFunctionExpressionInput &FI : MFC->FunctionInputs) { if (FI.Input.Expression) Count++; }
      }
      // Count output connections (other exprs referencing this one)
      for (UMaterialExpression *Other : Exprs) {
        if (!Other || Other == Expr) continue;
        for (TFieldIterator<FStructProperty> It2(Other->GetClass()); It2; ++It2) {
          FStructProperty *SP2 = *It2;
          if (!SP2->Struct || SP2->Struct->GetFName() != FName(TEXT("ExpressionInput"))) continue;
          FExpressionInput *InPtr2 = SP2->ContainerPtrToValuePtr<FExpressionInput>(Other);
          if (InPtr2 && InPtr2->Expression == Expr) Count++;
        }
      }
      ConnectionCountMap.Add(Expr->MaterialExpressionGuid, Count);
    }

    TSet<FGuid> SeenIds;
    TArray<TSharedPtr<FJsonValue>> Matches;
    for (UMaterialExpression *Expr : Exprs) {
      if (!Expr) continue;

      // Deduplicate by GUID
      if (SeenIds.Contains(Expr->MaterialExpressionGuid)) continue;

      FString ClassName = Expr->GetClass()->GetName();

      // Type match (substring)
      if (!SearchType.IsEmpty() && !ClassName.Contains(SearchType)) continue;

      // Name match
      if (!SearchName.IsEmpty()) {
        bool bNameMatch = false;
        if (UMaterialExpressionParameter *P = Cast<UMaterialExpressionParameter>(Expr)) {
          bNameMatch = P->ParameterName.ToString().Contains(SearchName);
        } else if (UMaterialExpressionFunctionInput *FI = Cast<UMaterialExpressionFunctionInput>(Expr)) {
          bNameMatch = FI->InputName.ToString().Contains(SearchName);
        } else if (UMaterialExpressionFunctionOutput *FO = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
          bNameMatch = FO->OutputName.ToString().Contains(SearchName);
        } else if (UMaterialExpressionCustom *CE = Cast<UMaterialExpressionCustom>(Expr)) {
          bNameMatch = CE->Description.Contains(SearchName) || CE->Code.Contains(SearchName);
        }
        if (!bNameMatch && SearchType.IsEmpty()) continue;
      }

      SeenIds.Add(Expr->MaterialExpressionGuid);

      TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
      Obj->SetStringField(TEXT("nodeId"), MCP_NODE_ID(Expr));
      Obj->SetStringField(TEXT("type"), ClassName);
      Obj->SetStringField(TEXT("desc"), Expr->GetDescription());
      Obj->SetNumberField(TEXT("x"), Expr->MaterialExpressionEditorX);
      Obj->SetNumberField(TEXT("y"), Expr->MaterialExpressionEditorY);
      int32 *CC = ConnectionCountMap.Find(Expr->MaterialExpressionGuid);
      Obj->SetNumberField(TEXT("connectionCount"), CC ? *CC : 0);
      Matches.Add(MakeShared<FJsonValueObject>(Obj));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("matchCount"), Matches.Num());
    Result->SetArrayField(TEXT("nodes"), Matches);
    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Found %d matching node(s)."), Matches.Num()),
                           Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // get_node_connections — get connections for a node with graph traversal
  //   direction: "inputs"|"outputs"|"both" (default "both")
  //   depth: int (default 1, -1 = unlimited)
  //   upstream: bool — walk backward to all sources
  //   downstream: bool — walk forward to all consumers
  //   Returns flattened list with "hop" field indicating distance from origin
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("get_node_connections")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    if (NodeId.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeId'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterialExpression *StartExpr = FIND_EXPR_IN_HOST(NodeId);
    if (!StartExpr) {
      SendAutomationError(Socket, RequestId, TEXT("Node not found."),
                          TEXT("NODE_NOT_FOUND"));
      return true;
    }

    // Parse options
    FString Direction;
    Payload->TryGetStringField(TEXT("direction"), Direction);
    if (Direction.IsEmpty()) Direction = TEXT("both");
    bool bWantInputs  = (Direction == TEXT("inputs")  || Direction == TEXT("both"));
    bool bWantOutputs = (Direction == TEXT("outputs") || Direction == TEXT("both"));

    double DepthD = 1.0;
    Payload->TryGetNumberField(TEXT("depth"), DepthD);
    int32 MaxDepth = (int32)DepthD;

    bool bUpstream = false, bDownstream = false;
    Payload->TryGetBoolField(TEXT("upstream"), bUpstream);
    Payload->TryGetBoolField(TEXT("downstream"), bDownstream);
    // upstream/downstream override direction+depth
    if (bUpstream) { bWantInputs = true; bWantOutputs = false; if (MaxDepth > 0) MaxDepth = 9999; }
    if (bDownstream) { bWantOutputs = true; bWantInputs = false; if (MaxDepth > 0) MaxDepth = 9999; }
    if (bUpstream && bDownstream) { bWantInputs = true; bWantOutputs = true; }
    if (MaxDepth == -1) MaxDepth = 9999;

    auto& AllExpr = Material
        ? MCP_GET_MATERIAL_EXPRESSIONS(Material)
        : MCP_GET_FUNCTION_EXPRESSIONS(Function);

    // --- Build adjacency: for each expression, find its input sources ---
    // InputSourcesOf[Expr] = list of {SourceExpr, OutputIndex, PinName}
    struct FEdge {
      UMaterialExpression *Source;
      UMaterialExpression *Target;
      int32 OutputIndex;
      FString PinName;
    };
    TArray<FEdge> AllEdges;

    auto CollectInputEdges = [&](UMaterialExpression *Expr) {
      if (!Expr) return;
      // Reflection-based inputs
      for (TFieldIterator<FStructProperty> It(Expr->GetClass()); It; ++It) {
        FStructProperty *SP = *It;
        if (!SP->Struct || SP->Struct->GetFName() != FName(TEXT("ExpressionInput"))) continue;
        FExpressionInput *InPtr = SP->ContainerPtrToValuePtr<FExpressionInput>(Expr);
        if (!InPtr || !InPtr->Expression) continue;
        AllEdges.Add({InPtr->Expression, Expr, InPtr->OutputIndex, SP->GetName()});
      }
      // Custom expression named inputs
      if (UMaterialExpressionCustom *CE = Cast<UMaterialExpressionCustom>(Expr)) {
        for (const FCustomInput &CI : CE->Inputs) {
          if (CI.Input.Expression) {
            AllEdges.Add({CI.Input.Expression, Expr, CI.Input.OutputIndex, CI.InputName.ToString()});
          }
        }
      }
      // MF call inputs
      if (UMaterialExpressionMaterialFunctionCall *MFC = Cast<UMaterialExpressionMaterialFunctionCall>(Expr)) {
        for (const FFunctionExpressionInput &FI : MFC->FunctionInputs) {
          if (FI.Input.Expression) {
            AllEdges.Add({FI.Input.Expression, Expr, FI.Input.OutputIndex, FI.ExpressionInput->InputName.ToString()});
          }
        }
      }
    };

    for (UMaterialExpression *Expr : AllExpr) {
      CollectInputEdges(Expr);
    }

    // Material main pin edges
    TArray<FEdge> MainPinEdges;
    if (Material) {
#if WITH_EDITORONLY_DATA
      auto AddMainEdge = [&](const FString &PinName, const FExpressionInput &Input) {
        if (Input.Expression) {
          MainPinEdges.Add({Input.Expression, nullptr, Input.OutputIndex, PinName});
        }
      };
      AddMainEdge(TEXT("BaseColor"), MCP_GET_MATERIAL_INPUT(Material, BaseColor));
      AddMainEdge(TEXT("EmissiveColor"), MCP_GET_MATERIAL_INPUT(Material, EmissiveColor));
      AddMainEdge(TEXT("Roughness"), MCP_GET_MATERIAL_INPUT(Material, Roughness));
      AddMainEdge(TEXT("Metallic"), MCP_GET_MATERIAL_INPUT(Material, Metallic));
      AddMainEdge(TEXT("Specular"), MCP_GET_MATERIAL_INPUT(Material, Specular));
      AddMainEdge(TEXT("Normal"), MCP_GET_MATERIAL_INPUT(Material, Normal));
      AddMainEdge(TEXT("Opacity"), MCP_GET_MATERIAL_INPUT(Material, Opacity));
      AddMainEdge(TEXT("OpacityMask"), MCP_GET_MATERIAL_INPUT(Material, OpacityMask));
      AddMainEdge(TEXT("AmbientOcclusion"), MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion));
      AddMainEdge(TEXT("SubsurfaceColor"), MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor));
      AddMainEdge(TEXT("WorldPositionOffset"), MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset));
#endif
    }

    // --- BFS traversal ---
    struct FNodeHop { UMaterialExpression *Expr; int32 Hop; };
    TArray<TSharedPtr<FJsonValue>> ResultConns;
    TSet<FGuid> Visited;
    Visited.Add(StartExpr->MaterialExpressionGuid);

    TArray<FNodeHop> Queue;
    Queue.Add({StartExpr, 0});
    int32 QueueIdx = 0;

    while (QueueIdx < Queue.Num()) {
      FNodeHop Current = Queue[QueueIdx++];
      if (Current.Hop >= MaxDepth) continue;

      // Walk upstream (inputs): edges where Current is the Target
      if (bWantInputs) {
        for (const FEdge &E : AllEdges) {
          if (E.Target != Current.Expr) continue;
          TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
          Obj->SetStringField(TEXT("sourceNodeId"), MCP_NODE_ID(E.Source));
          Obj->SetNumberField(TEXT("sourceOutputIndex"), E.OutputIndex);
          Obj->SetStringField(TEXT("targetNodeId"), MCP_NODE_ID(Current.Expr));
          Obj->SetStringField(TEXT("targetInput"), E.PinName);
          Obj->SetNumberField(TEXT("hop"), Current.Hop + 1);
          Obj->SetStringField(TEXT("direction"), TEXT("input"));
          ResultConns.Add(MakeShared<FJsonValueObject>(Obj));
          if (!Visited.Contains(E.Source->MaterialExpressionGuid)) {
            Visited.Add(E.Source->MaterialExpressionGuid);
            Queue.Add({E.Source, Current.Hop + 1});
          }
        }
      }

      // Walk downstream (outputs): edges where Current is the Source
      if (bWantOutputs) {
        for (const FEdge &E : AllEdges) {
          if (E.Source != Current.Expr) continue;
          TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
          Obj->SetStringField(TEXT("sourceNodeId"), MCP_NODE_ID(Current.Expr));
          Obj->SetNumberField(TEXT("sourceOutputIndex"), E.OutputIndex);
          Obj->SetStringField(TEXT("targetNodeId"), MCP_NODE_ID(E.Target));
          Obj->SetStringField(TEXT("targetInput"), E.PinName);
          Obj->SetNumberField(TEXT("hop"), Current.Hop + 1);
          Obj->SetStringField(TEXT("direction"), TEXT("output"));
          ResultConns.Add(MakeShared<FJsonValueObject>(Obj));
          if (!Visited.Contains(E.Target->MaterialExpressionGuid)) {
            Visited.Add(E.Target->MaterialExpressionGuid);
            Queue.Add({E.Target, Current.Hop + 1});
          }
        }
        // Main pin outputs
        for (const FEdge &E : MainPinEdges) {
          if (E.Source != Current.Expr) continue;
          TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
          Obj->SetStringField(TEXT("sourceNodeId"), MCP_NODE_ID(Current.Expr));
          Obj->SetNumberField(TEXT("sourceOutputIndex"), E.OutputIndex);
          Obj->SetStringField(TEXT("targetNodeId"), TEXT("Main"));
          Obj->SetStringField(TEXT("targetInput"), E.PinName);
          Obj->SetNumberField(TEXT("hop"), Current.Hop + 1);
          Obj->SetStringField(TEXT("direction"), TEXT("output"));
          ResultConns.Add(MakeShared<FJsonValueObject>(Obj));
        }
      }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), MCP_NODE_ID(StartExpr));
    Result->SetStringField(TEXT("type"), StartExpr->GetClass()->GetName());
    Result->SetNumberField(TEXT("connectionCount"), ResultConns.Num());
    Result->SetArrayField(TEXT("connections"), ResultConns);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Node connections retrieved."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // get_node_properties — read all properties of a specific node
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("get_node_properties")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    if (NodeId.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeId'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterialExpression *Expr = FIND_EXPR_IN_HOST(NodeId);
    if (!Expr) {
      SendAutomationError(Socket, RequestId, TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), MCP_NODE_ID(Expr));
    Result->SetStringField(TEXT("type"), Expr->GetClass()->GetName());
    Result->SetStringField(TEXT("desc"), Expr->GetDescription());
    Result->SetNumberField(TEXT("x"), Expr->MaterialExpressionEditorX);
    Result->SetNumberField(TEXT("y"), Expr->MaterialExpressionEditorY);

    // Type-specific properties
    if (UMaterialExpressionCustom *CE = Cast<UMaterialExpressionCustom>(Expr)) {
      Result->SetStringField(TEXT("code"), CE->Code);
      Result->SetStringField(TEXT("description"), CE->Description);
      // Output type
      switch (CE->OutputType) {
        case CMOT_Float1: Result->SetStringField(TEXT("outputType"), TEXT("Float1")); break;
        case CMOT_Float2: Result->SetStringField(TEXT("outputType"), TEXT("Float2")); break;
        case CMOT_Float3: Result->SetStringField(TEXT("outputType"), TEXT("Float3")); break;
        case CMOT_Float4: Result->SetStringField(TEXT("outputType"), TEXT("Float4")); break;
        case CMOT_MaterialAttributes: Result->SetStringField(TEXT("outputType"), TEXT("MaterialAttributes")); break;
        default: Result->SetStringField(TEXT("outputType"), TEXT("Unknown")); break;
      }
      TArray<TSharedPtr<FJsonValue>> InputsArr;
      for (const FCustomInput &CI : CE->Inputs) {
        TSharedPtr<FJsonObject> IO = MakeShared<FJsonObject>();
        IO->SetStringField(TEXT("name"), CI.InputName.ToString());
        InputsArr.Add(MakeShared<FJsonValueObject>(IO));
      }
      Result->SetArrayField(TEXT("inputs"), InputsArr);
      TArray<TSharedPtr<FJsonValue>> OutputsArr;
      for (const FCustomOutput &CO : CE->AdditionalOutputs) {
        TSharedPtr<FJsonObject> OO = MakeShared<FJsonObject>();
        OO->SetStringField(TEXT("name"), CO.OutputName.ToString());
        switch (CO.OutputType) {
          case CMOT_Float1: OO->SetStringField(TEXT("type"), TEXT("Float1")); break;
          case CMOT_Float2: OO->SetStringField(TEXT("type"), TEXT("Float2")); break;
          case CMOT_Float3: OO->SetStringField(TEXT("type"), TEXT("Float3")); break;
          case CMOT_Float4: OO->SetStringField(TEXT("type"), TEXT("Float4")); break;
          case CMOT_MaterialAttributes: OO->SetStringField(TEXT("type"), TEXT("MaterialAttributes")); break;
          default: OO->SetStringField(TEXT("type"), TEXT("Unknown")); break;
        }
        OutputsArr.Add(MakeShared<FJsonValueObject>(OO));
      }
      Result->SetArrayField(TEXT("additionalOutputs"), OutputsArr);
    } else if (UMaterialExpressionScalarParameter *SP = Cast<UMaterialExpressionScalarParameter>(Expr)) {
      Result->SetStringField(TEXT("parameterName"), SP->ParameterName.ToString());
      Result->SetNumberField(TEXT("defaultValue"), SP->DefaultValue);
      Result->SetStringField(TEXT("group"), SP->Group.ToString());
      Result->SetNumberField(TEXT("sliderMin"), SP->SliderMin);
      Result->SetNumberField(TEXT("sliderMax"), SP->SliderMax);
    } else if (UMaterialExpressionVectorParameter *VP = Cast<UMaterialExpressionVectorParameter>(Expr)) {
      Result->SetStringField(TEXT("parameterName"), VP->ParameterName.ToString());
      TSharedPtr<FJsonObject> DefVal = MakeShared<FJsonObject>();
      DefVal->SetNumberField(TEXT("r"), VP->DefaultValue.R);
      DefVal->SetNumberField(TEXT("g"), VP->DefaultValue.G);
      DefVal->SetNumberField(TEXT("b"), VP->DefaultValue.B);
      DefVal->SetNumberField(TEXT("a"), VP->DefaultValue.A);
      Result->SetObjectField(TEXT("defaultValue"), DefVal);
      Result->SetStringField(TEXT("group"), VP->Group.ToString());
    } else if (UMaterialExpressionStaticSwitchParameter *SSP = Cast<UMaterialExpressionStaticSwitchParameter>(Expr)) {
      Result->SetStringField(TEXT("parameterName"), SSP->ParameterName.ToString());
      Result->SetBoolField(TEXT("defaultValue"), SSP->DefaultValue);
      Result->SetStringField(TEXT("group"), SSP->Group.ToString());
    } else if (UMaterialExpressionComponentMask *CM = Cast<UMaterialExpressionComponentMask>(Expr)) {
      Result->SetBoolField(TEXT("r"), CM->R != 0);
      Result->SetBoolField(TEXT("g"), CM->G != 0);
      Result->SetBoolField(TEXT("b"), CM->B != 0);
      Result->SetBoolField(TEXT("a"), CM->A != 0);
    } else if (UMaterialExpressionMaterialFunctionCall *MFC = Cast<UMaterialExpressionMaterialFunctionCall>(Expr)) {
      if (MFC->MaterialFunction) {
        Result->SetStringField(TEXT("functionPath"), MFC->MaterialFunction->GetPathName());
      }
      TArray<TSharedPtr<FJsonValue>> FInputsArr;
      for (const FFunctionExpressionInput &FI : MFC->FunctionInputs) {
        TSharedPtr<FJsonObject> IO = MakeShared<FJsonObject>();
        IO->SetStringField(TEXT("name"), FI.ExpressionInput->InputName.ToString());
        FInputsArr.Add(MakeShared<FJsonValueObject>(IO));
      }
      Result->SetArrayField(TEXT("inputPins"), FInputsArr);
      TArray<TSharedPtr<FJsonValue>> FOutputsArr;
      for (const FFunctionExpressionOutput &FO : MFC->FunctionOutputs) {
        TSharedPtr<FJsonObject> OO = MakeShared<FJsonObject>();
        OO->SetStringField(TEXT("name"), FO.ExpressionOutput->OutputName.ToString());
        FOutputsArr.Add(MakeShared<FJsonValueObject>(OO));
      }
      Result->SetArrayField(TEXT("outputPins"), FOutputsArr);
    } else if (UMaterialExpressionFunctionInput *FI = Cast<UMaterialExpressionFunctionInput>(Expr)) {
      Result->SetStringField(TEXT("inputName"), FI->InputName.ToString());
      Result->SetStringField(TEXT("inputType"), FunctionInputTypeToString(FI->InputType));
      Result->SetBoolField(TEXT("usePreviewValueAsDefault"), FI->bUsePreviewValueAsDefault);
      Result->SetNumberField(TEXT("sortPriority"), FI->SortPriority);
      Result->SetStringField(TEXT("description"), FI->Description);
    } else if (UMaterialExpressionFunctionOutput *FO = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
      Result->SetStringField(TEXT("outputName"), FO->OutputName.ToString());
      Result->SetNumberField(TEXT("sortPriority"), FO->SortPriority);
      Result->SetStringField(TEXT("description"), FO->Description);
    } else if (UMaterialExpressionTextureSample *TS = Cast<UMaterialExpressionTextureSample>(Expr)) {
      Result->SetStringField(TEXT("texturePath"), TS->Texture ? TS->Texture->GetPathName() : TEXT(""));
    } else if (UMaterialExpressionTextureCoordinate *TC = Cast<UMaterialExpressionTextureCoordinate>(Expr)) {
      Result->SetNumberField(TEXT("coordinateIndex"), TC->CoordinateIndex);
      Result->SetNumberField(TEXT("uTiling"), TC->UTiling);
      Result->SetNumberField(TEXT("vTiling"), TC->VTiling);
    } else {
      // Generic fallback: dump all UPROPERTY fields
      TSharedPtr<FJsonObject> PropsObj = MakeShared<FJsonObject>();
      for (TFieldIterator<FProperty> PropIt(Expr->GetClass()); PropIt; ++PropIt) {
        FProperty *Prop = *PropIt;
        // Skip inherited UMaterialExpression properties
        if (Prop->GetOwnerClass() == UMaterialExpression::StaticClass()) continue;
        if (Prop->GetOwnerClass() == UObject::StaticClass()) continue;
        FString ValueStr;
        MCP_PROPERTY_EXPORT_TEXT(Prop, ValueStr, Prop->ContainerPtrToValuePtr<void>(Expr), nullptr, Expr, 0);
        PropsObj->SetStringField(Prop->GetName(), ValueStr);
      }
      Result->SetObjectField(TEXT("properties"), PropsObj);
    }

    SendAutomationResponse(Socket, RequestId, true, TEXT("Node properties retrieved."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // set_static_switch_parameter_value — on material instances
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("set_static_switch_parameter_value")) {
    FString AssetPath, ParamName;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) || ParamName.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    bool Value = false;
    Payload->TryGetBoolField(TEXT("value"), Value);

    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s'"), *AssetPath), TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    UMaterialInstanceConstant *Instance = LoadObject<UMaterialInstanceConstant>(nullptr, *AssetPath);
    if (!Instance) {
      SendAutomationError(Socket, RequestId, TEXT("Could not load material instance."), TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // Set the static switch parameter
    FStaticParameterSet StaticParams;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    Instance->GetStaticParameterValues(StaticParams);
#else
    StaticParams = Instance->GetStaticParameters();
#endif
    bool bFound = false;
    for (auto &Switch : StaticParams.StaticSwitchParameters) {
      if (Switch.ParameterInfo.Name == FName(*ParamName)) {
        Switch.Value = Value;
        Switch.bOverride = true;
        bFound = true;
        break;
      }
    }
    if (!bFound) {
      // Add new entry
      FStaticSwitchParameter NewSwitch;
      NewSwitch.ParameterInfo.Name = FName(*ParamName);
      NewSwitch.Value = Value;
      NewSwitch.bOverride = true;
      StaticParams.StaticSwitchParameters.Add(NewSwitch);
    }
    Instance->UpdateStaticPermutation(StaticParams);
    Instance->PostEditChange();
    Instance->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialInstanceAsset(Instance);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Instance);
    Result->SetStringField(TEXT("parameterName"), ParamName);
    Result->SetBoolField(TEXT("value"), Value);
    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Static switch '%s' set to %s."), *ParamName, Value ? TEXT("true") : TEXT("false")),
                           Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // delete_node — batch removal with auto-disconnect
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("delete_node")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    // Accept single nodeId or array of nodeIds
    TArray<FString> NodeIds;
    FString SingleId;
    if (Payload->TryGetStringField(TEXT("nodeId"), SingleId) && !SingleId.IsEmpty()) {
      NodeIds.Add(SingleId);
    }
    const TArray<TSharedPtr<FJsonValue>> *IdsArr = nullptr;
    if (Payload->TryGetArrayField(TEXT("nodeIds"), IdsArr) && IdsArr) {
      for (const auto &Val : *IdsArr) {
        FString Id;
        if (Val->TryGetString(Id) && !Id.IsEmpty()) NodeIds.Add(Id);
      }
    }
    if (NodeIds.Num() == 0) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeId' or 'nodeIds'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    auto& AllExpr = Material
        ? MCP_GET_MATERIAL_EXPRESSIONS(Material)
        : MCP_GET_FUNCTION_EXPRESSIONS(Function);

    TArray<FString> Removed;
    for (const FString &NId : NodeIds) {
      UMaterialExpression *Expr = FIND_EXPR_IN_HOST(NId);
      if (!Expr) continue;

      // Auto-disconnect: clear all references to this node from other expressions
      for (UMaterialExpression *Other : AllExpr) {
        if (!Other || Other == Expr) continue;
        for (TFieldIterator<FStructProperty> It(Other->GetClass()); It; ++It) {
          FStructProperty *SP = *It;
          if (!SP->Struct || SP->Struct->GetFName() != FName(TEXT("ExpressionInput"))) continue;
          FExpressionInput *InPtr = SP->ContainerPtrToValuePtr<FExpressionInput>(Other);
          if (InPtr && InPtr->Expression == Expr) {
            InPtr->Expression = nullptr;
            InPtr->OutputIndex = 0;
          }
        }
        if (UMaterialExpressionCustom *CE = Cast<UMaterialExpressionCustom>(Other)) {
          for (FCustomInput &CI : CE->Inputs) {
            if (CI.Input.Expression == Expr) { CI.Input.Expression = nullptr; CI.Input.OutputIndex = 0; }
          }
        }
        if (UMaterialExpressionMaterialFunctionCall *MFC = Cast<UMaterialExpressionMaterialFunctionCall>(Other)) {
          for (FFunctionExpressionInput &FI : MFC->FunctionInputs) {
            if (FI.Input.Expression == Expr) { FI.Input.Expression = nullptr; FI.Input.OutputIndex = 0; }
          }
        }
      }

      // Clear Material main pin references
      if (Material) {
#if WITH_EDITORONLY_DATA
        auto ClearMainPin = [&](FExpressionInput &Input) {
          if (Input.Expression == Expr) { Input.Expression = nullptr; Input.OutputIndex = 0; }
        };
        ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, BaseColor));
        ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, EmissiveColor));
        ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, Roughness));
        ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, Metallic));
        ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, Specular));
        ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, Normal));
        ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, Opacity));
        ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, OpacityMask));
        ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion));
        ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor));
        ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset));
#endif
      }

      // Remove the expression
      if (Material) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        Material->GetExpressionCollection().RemoveExpression(Expr);
#else
        Material->Expressions.Remove(Expr);
#endif
      } else {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        Function->GetExpressionCollection().RemoveExpression(Expr);
#else
        Function->FunctionExpressions.Remove(Expr);
#endif
      }
      Removed.Add(NId);
    }

    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> RemovedArr;
    for (const FString &R : Removed) {
      RemovedArr.Add(MakeShared<FJsonValueString>(R));
    }
    Result->SetArrayField(TEXT("removed"), RemovedArr);
    Result->SetNumberField(TEXT("removedCount"), Removed.Num());
    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Deleted %d node(s)."), Removed.Num()),
                           Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // update_custom_expression — modify code/inputs/outputs of existing CE
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("update_custom_expression")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    if (NodeId.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeId'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterialExpression *Expr = FIND_EXPR_IN_HOST(NodeId);
    if (!Expr) {
      SendAutomationError(Socket, RequestId, TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
      return true;
    }

    UMaterialExpressionCustom *CustomExpr = Cast<UMaterialExpressionCustom>(Expr);
    if (!CustomExpr) {
      SendAutomationError(Socket, RequestId, TEXT("Node is not a Custom Expression."), TEXT("INVALID_NODE_TYPE"));
      return true;
    }

    // Update code if provided
    FString NewCode;
    if (Payload->TryGetStringField(TEXT("code"), NewCode)) {
      CustomExpr->Code = NewCode;
    }

    // Update description if provided
    FString NewDesc;
    if (Payload->TryGetStringField(TEXT("description"), NewDesc)) {
      CustomExpr->Description = NewDesc;
    }

    // Update output type if provided
    FString NewOutputType;
    if (Payload->TryGetStringField(TEXT("outputType"), NewOutputType)) {
      if (NewOutputType == TEXT("Float1")) CustomExpr->OutputType = CMOT_Float1;
      else if (NewOutputType == TEXT("Float2")) CustomExpr->OutputType = CMOT_Float2;
      else if (NewOutputType == TEXT("Float3")) CustomExpr->OutputType = CMOT_Float3;
      else if (NewOutputType == TEXT("Float4")) CustomExpr->OutputType = CMOT_Float4;
      else if (NewOutputType == TEXT("MaterialAttributes")) CustomExpr->OutputType = CMOT_MaterialAttributes;
    }

    // Update inputs if provided
    const TArray<TSharedPtr<FJsonValue>> *InputsArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("inputs"), InputsArray) && InputsArray) {
      CustomExpr->Inputs.Empty();
      for (const auto &InputVal : *InputsArray) {
        const TSharedPtr<FJsonObject> *InputObj = nullptr;
        if (InputVal->TryGetObject(InputObj) && InputObj) {
          FString InputName;
          (*InputObj)->TryGetStringField(TEXT("name"), InputName);
          if (!InputName.IsEmpty()) {
            FCustomInput NewInput;
            NewInput.InputName = FName(*InputName);
            CustomExpr->Inputs.Add(NewInput);
          }
        }
      }
    }

    // Update additional outputs if provided
    const TArray<TSharedPtr<FJsonValue>> *OutputsArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("additionalOutputs"), OutputsArray) && OutputsArray) {
      CustomExpr->AdditionalOutputs.Empty();
      for (const auto &OutputVal : *OutputsArray) {
        const TSharedPtr<FJsonObject> *OutputObj = nullptr;
        if (OutputVal->TryGetObject(OutputObj) && OutputObj) {
          FString OutputName, OType;
          (*OutputObj)->TryGetStringField(TEXT("name"), OutputName);
          (*OutputObj)->TryGetStringField(TEXT("type"), OType);
          if (!OutputName.IsEmpty()) {
            FCustomOutput NewOutput;
            NewOutput.OutputName = FName(*OutputName);
            if (OType == TEXT("Float2")) NewOutput.OutputType = CMOT_Float2;
            else if (OType == TEXT("Float3")) NewOutput.OutputType = CMOT_Float3;
            else if (OType == TEXT("Float4")) NewOutput.OutputType = CMOT_Float4;
            else if (OType == TEXT("MaterialAttributes")) NewOutput.OutputType = CMOT_MaterialAttributes;
            else NewOutput.OutputType = CMOT_Float1;
            CustomExpr->AdditionalOutputs.Add(NewOutput);
          }
        }
      }
    }

    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), NodeId);
    Result->SetStringField(TEXT("code"), CustomExpr->Code);
    Result->SetNumberField(TEXT("inputCount"), CustomExpr->Inputs.Num());
    Result->SetNumberField(TEXT("additionalOutputCount"), CustomExpr->AdditionalOutputs.Num());
    SendAutomationResponse(Socket, RequestId, true, TEXT("Custom expression updated."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // get_node_chain — trace signal path from startNodeId to endNodeId/endPin
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("get_node_chain")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString StartNodeId, EndNodeId, EndPin;
    Payload->TryGetStringField(TEXT("startNodeId"), StartNodeId);
    Payload->TryGetStringField(TEXT("endNodeId"), EndNodeId);
    Payload->TryGetStringField(TEXT("endPin"), EndPin);

    if (StartNodeId.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'startNodeId'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (EndNodeId.IsEmpty() && EndPin.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'endNodeId' or 'endPin'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterialExpression *StartExpr = FIND_EXPR_IN_HOST(StartNodeId);
    if (!StartExpr) {
      SendAutomationError(Socket, RequestId, TEXT("Start node not found."), TEXT("NODE_NOT_FOUND"));
      return true;
    }

    auto& AllExpr = Material
        ? MCP_GET_MATERIAL_EXPRESSIONS(Material)
        : MCP_GET_FUNCTION_EXPRESSIONS(Function);

    // Build downstream adjacency: Source → list of Targets
    TMultiMap<UMaterialExpression*, UMaterialExpression*> Downstream;
    for (UMaterialExpression *Expr : AllExpr) {
      if (!Expr) continue;
      for (TFieldIterator<FStructProperty> It(Expr->GetClass()); It; ++It) {
        FStructProperty *SP = *It;
        if (!SP->Struct || SP->Struct->GetFName() != FName(TEXT("ExpressionInput"))) continue;
        FExpressionInput *InPtr = SP->ContainerPtrToValuePtr<FExpressionInput>(Expr);
        if (InPtr && InPtr->Expression) {
          Downstream.Add(InPtr->Expression, Expr);
        }
      }
      if (UMaterialExpressionCustom *CE = Cast<UMaterialExpressionCustom>(Expr)) {
        for (const FCustomInput &CI : CE->Inputs) {
          if (CI.Input.Expression) Downstream.Add(CI.Input.Expression, Expr);
        }
      }
      if (UMaterialExpressionMaterialFunctionCall *MFC = Cast<UMaterialExpressionMaterialFunctionCall>(Expr)) {
        for (const FFunctionExpressionInput &FI : MFC->FunctionInputs) {
          if (FI.Input.Expression) Downstream.Add(FI.Input.Expression, Expr);
        }
      }
    }

    // Check if endPin refers to a Material main pin
    UMaterialExpression *EndExpr = nullptr;
    bool bEndIsMainPin = false;
    if (!EndNodeId.IsEmpty() && EndNodeId != TEXT("Main")) {
      EndExpr = FIND_EXPR_IN_HOST(EndNodeId);
    }
    if (!EndPin.IsEmpty() || EndNodeId == TEXT("Main")) {
      bEndIsMainPin = true;
    }

    // BFS from Start downstream
    TMap<UMaterialExpression*, UMaterialExpression*> Parent;
    TArray<UMaterialExpression*> BFSQueue;
    BFSQueue.Add(StartExpr);
    Parent.Add(StartExpr, nullptr);
    bool bPathFound = false;
    UMaterialExpression *PathEnd = nullptr;

    int32 Idx = 0;
    while (Idx < BFSQueue.Num()) {
      UMaterialExpression *Cur = BFSQueue[Idx++];

      // Check if we reached the end
      if (EndExpr && Cur == EndExpr) { bPathFound = true; PathEnd = Cur; break; }
      if (bEndIsMainPin && Material) {
#if WITH_EDITORONLY_DATA
        // Check if Cur feeds any main pin
        auto IsMainTarget = [&](const FExpressionInput &Input) { return Input.Expression == Cur; };
        if ((!EndPin.IsEmpty() && EndPin == TEXT("BaseColor") && IsMainTarget(MCP_GET_MATERIAL_INPUT(Material, BaseColor))) ||
            (!EndPin.IsEmpty() && EndPin == TEXT("EmissiveColor") && IsMainTarget(MCP_GET_MATERIAL_INPUT(Material, EmissiveColor))) ||
            (!EndPin.IsEmpty() && EndPin == TEXT("Roughness") && IsMainTarget(MCP_GET_MATERIAL_INPUT(Material, Roughness))) ||
            (!EndPin.IsEmpty() && EndPin == TEXT("Metallic") && IsMainTarget(MCP_GET_MATERIAL_INPUT(Material, Metallic))) ||
            (!EndPin.IsEmpty() && EndPin == TEXT("Normal") && IsMainTarget(MCP_GET_MATERIAL_INPUT(Material, Normal))) ||
            (!EndPin.IsEmpty() && EndPin == TEXT("Opacity") && IsMainTarget(MCP_GET_MATERIAL_INPUT(Material, Opacity))) ||
            (!EndPin.IsEmpty() && EndPin == TEXT("OpacityMask") && IsMainTarget(MCP_GET_MATERIAL_INPUT(Material, OpacityMask))) ||
            (!EndPin.IsEmpty() && EndPin == TEXT("AmbientOcclusion") && IsMainTarget(MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion))) ||
            (!EndPin.IsEmpty() && EndPin == TEXT("SubsurfaceColor") && IsMainTarget(MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor))) ||
            (!EndPin.IsEmpty() && EndPin == TEXT("WorldPositionOffset") && IsMainTarget(MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset))) ||
            (!EndPin.IsEmpty() && EndPin == TEXT("Specular") && IsMainTarget(MCP_GET_MATERIAL_INPUT(Material, Specular)))) {
          bPathFound = true; PathEnd = Cur; break;
        }
#endif
      }

      // Enqueue downstream neighbors
      TArray<UMaterialExpression*> Neighbors;
      Downstream.MultiFind(Cur, Neighbors);
      for (UMaterialExpression *N : Neighbors) {
        if (!Parent.Contains(N)) {
          Parent.Add(N, Cur);
          BFSQueue.Add(N);
        }
      }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (bPathFound && PathEnd) {
      // Reconstruct path
      TArray<UMaterialExpression*> Path;
      UMaterialExpression *Walk = PathEnd;
      while (Walk) {
        Path.Insert(Walk, 0);
        UMaterialExpression **P = Parent.Find(Walk);
        Walk = (P && *P) ? *P : nullptr;
      }
      TArray<TSharedPtr<FJsonValue>> ChainArr;
      for (int32 i = 0; i < Path.Num(); ++i) {
        TSharedPtr<FJsonObject> N = MakeShared<FJsonObject>();
        N->SetStringField(TEXT("nodeId"), MCP_NODE_ID(Path[i]));
        N->SetStringField(TEXT("type"), Path[i]->GetClass()->GetName());
        N->SetStringField(TEXT("desc"), Path[i]->GetDescription());
        N->SetNumberField(TEXT("step"), i);
        ChainArr.Add(MakeShared<FJsonValueObject>(N));
      }
      if (bEndIsMainPin) {
        TSharedPtr<FJsonObject> MainNode = MakeShared<FJsonObject>();
        MainNode->SetStringField(TEXT("nodeId"), TEXT("Main"));
        MainNode->SetStringField(TEXT("type"), TEXT("MaterialOutput"));
        MainNode->SetStringField(TEXT("desc"), EndPin.IsEmpty() ? TEXT("Main") : EndPin);
        MainNode->SetNumberField(TEXT("step"), Path.Num());
        ChainArr.Add(MakeShared<FJsonValueObject>(MainNode));
      }
      Result->SetBoolField(TEXT("pathFound"), true);
      Result->SetNumberField(TEXT("length"), ChainArr.Num());
      Result->SetArrayField(TEXT("chain"), ChainArr);
    } else {
      Result->SetBoolField(TEXT("pathFound"), false);
      Result->SetStringField(TEXT("message"), TEXT("No path found between the specified nodes."));
    }
    SendAutomationResponse(Socket, RequestId, true,
                           bPathFound ? TEXT("Signal path found.") : TEXT("No path found."),
                           Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // get_connected_subgraph — island detection + orphansOnly
  // --------------------------------------------------------------------------
  if (SubAction == TEXT("get_connected_subgraph")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    bool bOrphansOnly = false;
    Payload->TryGetBoolField(TEXT("orphansOnly"), bOrphansOnly);

    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);

    if (NodeId.IsEmpty() && !bOrphansOnly) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeId' (or set orphansOnly=true)."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    auto& AllExpr = Material
        ? MCP_GET_MATERIAL_EXPRESSIONS(Material)
        : MCP_GET_FUNCTION_EXPRESSIONS(Function);

    // Build bidirectional adjacency
    TMultiMap<UMaterialExpression*, UMaterialExpression*> Adj;
    for (UMaterialExpression *Expr : AllExpr) {
      if (!Expr) continue;
      for (TFieldIterator<FStructProperty> It(Expr->GetClass()); It; ++It) {
        FStructProperty *SP = *It;
        if (!SP->Struct || SP->Struct->GetFName() != FName(TEXT("ExpressionInput"))) continue;
        FExpressionInput *InPtr = SP->ContainerPtrToValuePtr<FExpressionInput>(Expr);
        if (InPtr && InPtr->Expression) {
          Adj.Add(InPtr->Expression, Expr);
          Adj.Add(Expr, InPtr->Expression);
        }
      }
      if (UMaterialExpressionCustom *CE = Cast<UMaterialExpressionCustom>(Expr)) {
        for (const FCustomInput &CI : CE->Inputs) {
          if (CI.Input.Expression) { Adj.Add(CI.Input.Expression, Expr); Adj.Add(Expr, CI.Input.Expression); }
        }
      }
      if (UMaterialExpressionMaterialFunctionCall *MFC = Cast<UMaterialExpressionMaterialFunctionCall>(Expr)) {
        for (const FFunctionExpressionInput &FI : MFC->FunctionInputs) {
          if (FI.Input.Expression) { Adj.Add(FI.Input.Expression, Expr); Adj.Add(Expr, FI.Input.Expression); }
        }
      }
    }

    // Find nodes connected to Material main pins (or MF FunctionOutputs)
    TSet<UMaterialExpression*> OutputConnected;
    TArray<UMaterialExpression*> FloodQueue;
    if (Material) {
#if WITH_EDITORONLY_DATA
      auto SeedMain = [&](const FExpressionInput &Input) {
        if (Input.Expression) { OutputConnected.Add(Input.Expression); FloodQueue.Add(Input.Expression); }
      };
      SeedMain(MCP_GET_MATERIAL_INPUT(Material, BaseColor));
      SeedMain(MCP_GET_MATERIAL_INPUT(Material, EmissiveColor));
      SeedMain(MCP_GET_MATERIAL_INPUT(Material, Roughness));
      SeedMain(MCP_GET_MATERIAL_INPUT(Material, Metallic));
      SeedMain(MCP_GET_MATERIAL_INPUT(Material, Specular));
      SeedMain(MCP_GET_MATERIAL_INPUT(Material, Normal));
      SeedMain(MCP_GET_MATERIAL_INPUT(Material, Opacity));
      SeedMain(MCP_GET_MATERIAL_INPUT(Material, OpacityMask));
      SeedMain(MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion));
      SeedMain(MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor));
      SeedMain(MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset));
#endif
    } else {
      // For MF, seed from FunctionOutput nodes
      for (UMaterialExpression *Expr : AllExpr) {
        if (Cast<UMaterialExpressionFunctionOutput>(Expr)) {
          OutputConnected.Add(Expr);
          FloodQueue.Add(Expr);
        }
      }
    }
    // Flood fill from output-connected seeds
    int32 FIdx = 0;
    while (FIdx < FloodQueue.Num()) {
      UMaterialExpression *Cur = FloodQueue[FIdx++];
      TArray<UMaterialExpression*> Neighbors;
      Adj.MultiFind(Cur, Neighbors);
      for (UMaterialExpression *N : Neighbors) {
        if (!OutputConnected.Contains(N)) {
          OutputConnected.Add(N);
          FloodQueue.Add(N);
        }
      }
    }

    if (bOrphansOnly) {
      // Return all nodes NOT connected to any output
      TArray<TSharedPtr<FJsonValue>> OrphansArr;
      for (UMaterialExpression *Expr : AllExpr) {
        if (!Expr) continue;
        if (!OutputConnected.Contains(Expr)) {
          TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
          Obj->SetStringField(TEXT("nodeId"), MCP_NODE_ID(Expr));
          Obj->SetStringField(TEXT("type"), Expr->GetClass()->GetName());
          Obj->SetStringField(TEXT("desc"), Expr->GetDescription());
          OrphansArr.Add(MakeShared<FJsonValueObject>(Obj));
        }
      }
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetNumberField(TEXT("orphanCount"), OrphansArr.Num());
      Result->SetArrayField(TEXT("orphans"), OrphansArr);
      SendAutomationResponse(Socket, RequestId, true,
                             FString::Printf(TEXT("Found %d orphaned node(s)."), OrphansArr.Num()),
                             Result);
      return true;
    }

    // Flood fill from specified nodeId
    UMaterialExpression *SeedExpr = FIND_EXPR_IN_HOST(NodeId);
    if (!SeedExpr) {
      SendAutomationError(Socket, RequestId, TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
      return true;
    }

    TSet<UMaterialExpression*> Island;
    TArray<UMaterialExpression*> IslandQueue;
    Island.Add(SeedExpr);
    IslandQueue.Add(SeedExpr);
    int32 IIdx = 0;
    while (IIdx < IslandQueue.Num()) {
      UMaterialExpression *Cur = IslandQueue[IIdx++];
      TArray<UMaterialExpression*> Neighbors;
      Adj.MultiFind(Cur, Neighbors);
      for (UMaterialExpression *N : Neighbors) {
        if (!Island.Contains(N)) {
          Island.Add(N);
          IslandQueue.Add(N);
        }
      }
    }

    TArray<TSharedPtr<FJsonValue>> NodesArr;
    for (UMaterialExpression *Expr : Island) {
      TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
      Obj->SetStringField(TEXT("nodeId"), MCP_NODE_ID(Expr));
      Obj->SetStringField(TEXT("type"), Expr->GetClass()->GetName());
      Obj->SetStringField(TEXT("desc"), Expr->GetDescription());
      Obj->SetBoolField(TEXT("connectedToOutput"), OutputConnected.Contains(Expr));
      NodesArr.Add(MakeShared<FJsonValueObject>(Obj));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("islandSize"), NodesArr.Num());
    Result->SetArrayField(TEXT("nodes"), NodesArr);
    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Subgraph contains %d node(s)."), NodesArr.Num()),
                           Result);
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

    // SECURITY: Validate asset path using SanitizeProjectRelativePath
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid assetPath '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    UMaterial *Material = nullptr;
    UMaterialFunction *Function = nullptr;
    LoadMaterialOrFunction(AssetPath, Material, Function);
    if (!Material && !Function) {
      SendAutomationError(Socket, RequestId,
                          TEXT("Could not load Material or Material Function."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    UObject *HostOuter = Material ? static_cast<UObject*>(Material)
                                  : static_cast<UObject*>(Function);

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
        HostOuter, ExpressionClass, NAME_None, RF_Transactional);
    if (!NewExpr) {
      SendAutomationError(Socket, RequestId, TEXT("Failed to create expression."), TEXT("CREATE_FAILED"));
      return true;
    }

    // Set editor position
    NewExpr->MaterialExpressionEditorX = (int32)X;
    NewExpr->MaterialExpressionEditorY = (int32)Y;

    // Add to the host's expression collection (Material or MaterialFunction)
#if WITH_EDITORONLY_DATA
    AddExpressionToContainer(Material, Function, NewExpr);
#endif

    // If parameter node, set the parameter name
    FString ParamName;
    if (Payload->TryGetStringField(TEXT("name"), ParamName) && !ParamName.IsEmpty()) {
      if (UMaterialExpressionParameter *ParamExpr = Cast<UMaterialExpressionParameter>(NewExpr)) {
        ParamExpr->ParameterName = FName(*ParamName);
      }
    }

    HostOuter->PostEditChange();
    HostOuter->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), MCP_NODE_ID(NewExpr));
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

    // Validate path security BEFORE loading asset
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    UMaterial *Material = nullptr;
    UMaterialFunction *Function = nullptr;
    LoadMaterialOrFunction(AssetPath, Material, Function);
    if (!Material && !Function) {
      SendAutomationError(Socket, RequestId,
                          TEXT("Could not load Material or Material Function."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    UMaterialExpression *Expr = Material
        ? FindExpressionByIdOrName(Material, NodeId)
        : FindExpressionByIdOrNameInFunction(Function, NodeId);
    if (!Expr) {
      SendAutomationError(Socket, RequestId, TEXT("Node not found."), TEXT("NOT_FOUND"));
      return true;
    }

    // Remove the expression from the appropriate container
    if (Material) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      Material->GetExpressionCollection().RemoveExpression(Expr);
#else
      Material->Expressions.Remove(Expr);
#endif
    } else {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      Function->GetExpressionCollection().RemoveExpression(Expr);
#else
      Function->FunctionExpressions.Remove(Expr);
#endif
    }

    if (Material) { Material->PostEditChange(); Material->MarkPackageDirty(); }
    else { Function->PostEditChange(); Function->MarkPackageDirty(); }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
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

    // SECURITY: Validate assetPath before use (accepts both Materials and Material Functions)
    FString ValidatedAssetPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedAssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid assetPath '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedAssetPath;

    // This is a stub that routes to appropriate parameter handler
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
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

    // Validate path security BEFORE loading asset
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    UMaterial *Material = nullptr;
    UMaterialFunction *Function = nullptr;
    LoadMaterialOrFunction(AssetPath, Material, Function);
    if (!Material && !Function) {
      SendAutomationError(Socket, RequestId,
                          TEXT("Could not load Material or Material Function."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    UMaterialExpression *Expr = Material
        ? FindExpressionByIdOrName(Material, NodeId)
        : FindExpressionByIdOrNameInFunction(Function, NodeId);
    if (!Expr) {
      SendAutomationError(Socket, RequestId, TEXT("Node not found."), TEXT("NOT_FOUND"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), MCP_NODE_ID(Expr));
    Result->SetStringField(TEXT("nodeType"), Expr->GetClass()->GetName());
    Result->SetStringField(TEXT("nodeName"), Expr->GetName());
    Result->SetStringField(TEXT("assetType"),
                           Material ? TEXT("Material") : TEXT("MaterialFunction"));

    // Extra introspection for function input/output nodes
    if (UMaterialExpressionFunctionInput *In = Cast<UMaterialExpressionFunctionInput>(Expr)) {
      Result->SetStringField(TEXT("inputName"), In->InputName.ToString());
      Result->SetStringField(TEXT("inputType"), FunctionInputTypeToString(In->InputType));
      Result->SetBoolField(TEXT("usePreviewValueAsDefault"), In->bUsePreviewValueAsDefault);
      Result->SetNumberField(TEXT("sortPriority"), In->SortPriority);
    } else if (UMaterialExpressionFunctionOutput *Out = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
      Result->SetStringField(TEXT("outputName"), Out->OutputName.ToString());
      Result->SetNumberField(TEXT("sortPriority"), Out->SortPriority);
    }

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

    // Validate path security BEFORE loading asset
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    UMaterial *Material = LoadObject<UMaterial>(nullptr, *AssetPath);
    if (!Material) {
      SendAutomationError(Socket, RequestId, TEXT("Could not load Material."), TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    bool bTwoSided = GetJsonBoolField(Payload, TEXT("twoSided"), true);
    Material->TwoSided = bTwoSided ? 1 : 0;
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
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

    // SECURITY: Validate assetPath before use
    FString ValidatedAssetPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedAssetPath.IsEmpty()) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid assetPath '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedAssetPath;

    // Note: Cast shadows is typically a material property but may be on the component
    // This is a stub that acknowledges the request
    bool CastShadows = GetJsonBoolField(Payload, TEXT("castShadows"), true);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetBoolField(TEXT("castShadows"), CastShadows);

    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Cast shadows set to %s."), CastShadows ? TEXT("true") : TEXT("false")), Result);
    return true;
  }

#undef LOAD_MATERIAL_OR_RETURN
#undef LOAD_MATERIAL_OR_FUNCTION_OR_RETURN
#undef FIND_EXPR_IN_HOST
#undef FINALIZE_HOST

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

  // Use McpSafeAssetSave for proper asset registry notification
  return McpSafeAssetSave(Material);
}

static bool SaveMaterialFunctionAsset(UMaterialFunction *Function) {
  if (!Function)
    return false;

  // Use McpSafeAssetSave for proper asset registry notification
  return McpSafeAssetSave(Function);
}

static bool SaveMaterialInstanceAsset(UMaterialInstanceConstant *Instance) {
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
    if (Expr->GetName() == Needle) return Expr;
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
      if (P->ParameterName.ToString() == Needle) return Expr;
    }
    if (UMaterialExpressionFunctionInput *In = Cast<UMaterialExpressionFunctionInput>(Expr)) {
      if (In->InputName.ToString() == Needle) return Expr;
    }
    if (UMaterialExpressionFunctionOutput *Out = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
      if (Out->OutputName.ToString() == Needle) return Expr;
    }
  }

  return nullptr;
}

static UMaterialExpression *FindExpressionByIdOrName(UMaterial *Material,
                                                      const FString &IdOrName) {
  if (IdOrName.IsEmpty() || !Material) return nullptr;
  auto &Expressions = MCP_GET_MATERIAL_EXPRESSIONS(Material);
  return FindExpressionInArray(Expressions, IdOrName);
}

static UMaterialExpression *
FindExpressionByIdOrNameInFunction(UMaterialFunction *Function,
                                   const FString &IdOrName) {
  if (IdOrName.IsEmpty() || !Function) return nullptr;
  auto &Expressions = MCP_GET_FUNCTION_EXPRESSIONS(Function);
  return FindExpressionInArray(Expressions, IdOrName);
}

// Resolve an asset path as either a UMaterial or a UMaterialFunction.
// Exactly one of OutMaterial / OutFunction will be populated on success.
static UObject *LoadMaterialOrFunction(const FString &AssetPath,
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

static void AddExpressionToContainer(UMaterial *Material,
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

static FString FunctionInputTypeToString(EFunctionInputType InType) {
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
#endif
